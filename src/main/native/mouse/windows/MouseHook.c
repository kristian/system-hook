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
#include <windowsx.h>
#include <limits.h>
#include <jni.h>
#include "MouseHook.h"

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
jobject mouseHookObj = NULL;
jmethodID handleMouseMeth = NULL;

jint xPosOld = (jint)SHRT_MIN, yPosOld = (jint)SHRT_MIN;

DWORD hookThreadId = 0;

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

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)  {
	JNIEnv* env; MSLLHOOKSTRUCT* pStruct = (MSLLHOOKSTRUCT*)lParam;
	if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL)>=JNI_OK) {
		if(nCode==HC_ACTION) {
			jint tState = (jint)TS_UP, xPos = (jint)pStruct->pt.x, yPos = (jint)pStruct->pt.y, mkButton = 0;
			switch(wParam) {
				case WM_LBUTTONDOWN:
					tState = (jint)TS_DOWN;
					/* no break */
				case WM_LBUTTONUP:
					mkButton = MK_LBUTTON;
					break;
				case WM_RBUTTONDOWN:
					tState = (jint)TS_DOWN;
					/* no break */
				case WM_RBUTTONUP:
					mkButton = MK_RBUTTON;
					break;
				case WM_MBUTTONDOWN:
					tState = (jint)TS_DOWN;
					/* no break */
				case WM_MBUTTONUP:
					mkButton = MK_MBUTTON;
					break;
				case WM_MOUSEMOVE:
					tState = (jint)TS_MOVE;
					if(xPos!=xPosOld||yPos!=yPos)
						(*env)->CallVoidMethod(env, mouseHookObj, handleMouseMeth, tState, (jint)0, /*(jint)wParam,*/xPosOld=xPos, yPosOld=yPos, (jint)0);
					break;
				case WM_MOUSEWHEEL:
					(*env)->CallVoidMethod(env, mouseHookObj, handleMouseMeth, tState=TS_WHEEL, (jint)0, /*(jint)GET_KEYSTATE_WPARAM(wParam),*/xPos, yPos, (jint)GET_WHEEL_DELTA_WPARAM(pStruct->mouseData));
					break;
				default: break;
			}
			if(tState<=TS_DOWN&&mkButton!=0) //tState==TS_DOWN||tState==TS_UP
				(*env)->CallVoidMethod(env, mouseHookObj, handleMouseMeth, tState, mkButton, /*(jint)wParam,*/xPos, yPos, (jint)0);
		}
	} else DEBUG_PRINT(("NATIVE: LowLevelMouseProc - Error on the attach current thread.\n"));

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static inline _Bool notifyHookObj(JNIEnv *env) {
	jclass objectCls = (*env)->FindClass(env, "java/lang/Object");
	jmethodID notifyMeth = (*env)->GetMethodID(env, objectCls, "notify", "()V");
	if(notifyMeth==0)
		return FALSE;

	(*env)->MonitorEnter(env, mouseHookObj);
	(*env)->CallVoidMethod(env, mouseHookObj,notifyMeth);
	(*env)->MonitorExit(env, mouseHookObj);

	return TRUE;
}

JNIEXPORT jint JNICALL Java_lc_kra_system_mouse_GlobalMouseHook_00024NativeMouseHook_registerHook(JNIEnv *env, jobject thisObj) {
	DEBUG_PRINT(("NATIVE: registerHook - Hook start\n"));
	
	(*env)->GetJavaVM(env, &jvm);
	hookThreadId = GetCurrentThreadId();

	mouseHookObj = (*env)->NewGlobalRef(env, thisObj);
	jclass mouseHookCls = (*env)->GetObjectClass(env, mouseHookObj);
	handleMouseMeth = (*env)->GetMethodID(env, mouseHookCls, "handleMouse", "(IIIII)V");
	if(handleMouseMeth==0) {
		(*env)->ExceptionClear(env);
		DEBUG_PRINT(("NATIVE: registerHook - No handle method!\n"));
		return (jint)E_NO_HANDLE_METHOD;
	}

	HHOOK hookHandle = SetWindowsHookEx(WH_MOUSE_LL,LowLevelMouseProc,hInst,0);
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

	(*env)->DeleteGlobalRef(env, mouseHookObj);
	if(UnhookWindowsHookEx(hookHandle)) {
		DEBUG_PRINT(("NATIVE: registerHook - Unhook success\n"));
		return (jint)E_SUCCESS;
	} else {
		debugPrintLastError("NATIVE: registerHook - Unhook failed!");
		return (jint)E_UNHOOK_FAILED;
	}
}

JNIEXPORT void JNICALL Java_lc_kra_system_mouse_GlobalMouseHook_00024NativeMouseHook_unregisterHook(JNIEnv *env,jobject thisObj) {
	if(hookThreadId!=0) {
		DEBUG_PRINT(("NATIVE: unregisterHook - Call PostThreadMessage WM_QUIT.\n"));
		PostThreadMessage(hookThreadId, WM_QUIT, 0, 0L);
	}
}
