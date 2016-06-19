/**
 * Copyright (c) 2016 Kristian Kraljic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <windows.h>
#include <jni.h>
#include "KeyboardHook.h"

#ifdef DEBUG
#define DEBUG_PRINT(x) do{ printf x; fflush(stdout); } while(0)
#else
#define DEBUG_PRINT(x) do{ } while(0)
#endif

#ifdef __cplusplus
extern "C"
#endif

static inline void debugPrintLastError(TCHAR *szErrorText) {
	DWORD dw = GetLastError();

	void *pMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (TCHAR*)&pMsgBuf, 0, NULL);
	DEBUG_PRINT(("%s with error %ld: %s\n", szErrorText, dw, (TCHAR*)pMsgBuf));

	LocalFree(pMsgBuf);
}

HINSTANCE hInst = NULL;

JavaVM * jvm = NULL;
jobject keyboardHookObj = NULL;
jmethodID handleKeyMeth = NULL;

DWORD hookThreadId = 0;

BYTE keyState[256];
WCHAR buffer[2];

BOOL APIENTRY DllMain(HINSTANCE _hInst, DWORD reason, LPVOID reserved)  {
	switch(reason) {
		case DLL_PROCESS_ATTACH:
			DEBUG_PRINT(("NATIVE: DllMain - DLL_PROCESS_ATTACH\n"));
			hInst = _hInst;
			break;
		default:
			break;
	}
	return TRUE;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)  {
	JNIEnv* env; KBDLLHOOKSTRUCT* pStruct = (KBDLLHOOKSTRUCT*)lParam;
	if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL)>=JNI_OK) {
		GetKeyboardState((PBYTE)&keyState);

		// As ToUnicode/ToAscii is messing up dead keys (e.g. ^, `, ...), we have to check the MSB of the MapVirtualKey return value first.
		// (see Stack Overflow at http://stackoverflow.com/questions/1964614/toascii-tounicode-in-a-keyboard-hook-destroys-dead-keys)
		if(!(MapVirtualKey(pStruct->vkCode, MAPVK_VK_TO_CHAR)>>(sizeof(UINT)*8-1) & 1)) {
			switch(ToUnicode(pStruct->vkCode, pStruct->scanCode, (PBYTE)&keyState, (LPWSTR)&buffer, sizeof(buffer)/2, 0)) {
				case -1:
					DEBUG_PRINT(("NATIVE: LowLevelKeyboardProc - Wrong dead key mapping.\n"));
					break;
				default: break;
			}
		} else buffer[0] = '\0';

		jboolean tState = (jboolean)FALSE;
		switch(wParam) {
			case WM_KEYDOWN: case WM_SYSKEYDOWN:
				tState = (jboolean)TRUE;
				/* no break */
			case WM_KEYUP: case WM_SYSKEYUP:
				(*env)->CallVoidMethod(env, keyboardHookObj, handleKeyMeth, pStruct->vkCode, tState, (jchar)buffer[0]);
				break;
			default: break;
		}
	} else DEBUG_PRINT(("NATIVE: LowLevelKeyboardProc - Error on the attach current thread.\n"));

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static inline _Bool notifyHookObj(JNIEnv *env) {
	jclass objectCls = (*env)->FindClass(env, "java/lang/Object");
	jmethodID notifyMeth = (*env)->GetMethodID(env, objectCls, "notify", "()V");
	if(notifyMeth==0)
		return FALSE;

	(*env)->MonitorEnter(env, keyboardHookObj);
	(*env)->CallVoidMethod(env, keyboardHookObj,notifyMeth);
	(*env)->MonitorExit(env, keyboardHookObj);

	return TRUE;
}

JNIEXPORT jint JNICALL Java_lc_kra_system_keyboard_GlobalKeyboardHook_00024NativeKeyboardHook_registerHook(JNIEnv *env, jobject thisObj) {
	DEBUG_PRINT(("NATIVE: registerHook - Hook start\n"));
	
	(*env)->GetJavaVM(env, &jvm);
	hookThreadId = GetCurrentThreadId();

	keyboardHookObj = (*env)->NewGlobalRef(env, thisObj);
	jclass keyboardHookCls = (*env)->GetObjectClass(env, keyboardHookObj);
	handleKeyMeth = (*env)->GetMethodID(env, keyboardHookCls, "handleKey", "(IIC)V");
	if(handleKeyMeth==0) {
		(*env)->ExceptionClear(env);
		DEBUG_PRINT(("NATIVE: registerHook - No handle method!\n"));
		return (jint)E_NO_HANDLE_METHOD;
	}

	HHOOK hookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
	if(hookHandle==NULL) {
		debugPrintLastError("NATIVE: registerHook - Hook failed");
		return (jint)E_HOOK_FAILED;
	} else DEBUG_PRINT(("NATIVE: registerHook - Hook success\n"));

	if(!notifyHookObj(env)) {
		(*env)->ExceptionClear(env);
		DEBUG_PRINT(("NATIVE: registerHook - Notify failed!\n"));
		return (jint)E_NOTIFY_FAILED;
	}

	MSG message;
	while(GetMessage(&message,NULL,0,0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	(*env)->DeleteGlobalRef(env, keyboardHookObj);
	if(UnhookWindowsHookEx(hookHandle)) {
		DEBUG_PRINT(("NATIVE: registerHook - Unhook success\n"));
		return (jint)E_SUCCESS;
	} else {
		debugPrintLastError("NATIVE: registerHook - Unhook failed");
		return (jint)E_UNHOOK_FAILED;
	}
}

JNIEXPORT void JNICALL Java_lc_kra_system_keyboard_GlobalKeyboardHook_00024NativeKeyboardHook_unregisterHook(JNIEnv *env,jobject thisObj) {
	if(hookThreadId!=0) {
		DEBUG_PRINT(("NATIVE: unregisterHook - Call PostThreadMessage WM_QUIT.\n"));
		PostThreadMessage(hookThreadId, WM_QUIT, 0, 0L);
	}
}
