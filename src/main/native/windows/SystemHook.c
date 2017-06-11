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

#include "SystemHook.h"
#include <windows.h>
#include <jni.h>

#ifdef DEBUG
#define DEBUG_PRINT(x) do{ printf x; fflush(stdout); } while(0)
#else
#define DEBUG_PRINT(x) do{ } while(0)
#endif

#ifdef __cplusplus
extern "C"
#endif

#define HOOK_KEYBOARD 0
#define HOOK_MOUSE 1

const LPCSTR lpszClassNames[] = { "jkh" /* java keyboard hook */, "jmh" /* java mouse hook */ };
const USHORT mouseLeftButton = RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_LEFT_BUTTON_UP,
	mouseRightButton = RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_UP,
	mouseMiddleButton = RI_MOUSE_MIDDLE_BUTTON_DOWN|RI_MOUSE_MIDDLE_BUTTON_UP;

static inline void debugPrintLastError(TCHAR *szErrorText) {
	DWORD dw = GetLastError();

	void *pMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (TCHAR*)&pMsgBuf, 0, NULL);
	DEBUG_PRINT(("%s with error %ld: %s\n", szErrorText, dw, (TCHAR*)pMsgBuf));

	LocalFree(pMsgBuf);
}

HINSTANCE hInst = NULL;
RAWINPUTDEVICE rid[2];

JavaVM *jvm = NULL;
jobject hookObj[2] = { NULL, NULL };
jmethodID handleMeth[2] = { NULL, NULL };

DWORD hookThreadId[2] = { 0, 0 };

BYTE keyState[256];
WCHAR buffer[4];

jint lOldX = (jint)SHRT_MIN, lOldY = (jint)SHRT_MIN;

BOOL APIENTRY DllMain(HINSTANCE _hInst, DWORD reason, LPVOID reserved) {
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

static void handleKey(const char *nProc, UINT event, USHORT vkCode, USHORT scanCode, HANDLE hDevice) {
	// As ToUnicode/ToAscii is messing up dead keys (e.g. ^, `, ...), we have to check the MSB of the MapVirtualKey return value first.
	// (see Stack Overflow at http://stackoverflow.com/questions/1964614/toascii-tounicode-in-a-keyboard-hook-destroys-dead-keys)
	if(!(MapVirtualKey(vkCode, MAPVK_VK_TO_CHAR)>>(sizeof(UINT)*8-1) & 1)) {
		switch(ToUnicode(vkCode, scanCode, (PBYTE)&keyState, (LPWSTR)&buffer, sizeof(buffer)/2, 0)) {
			case -1:
				DEBUG_PRINT(("NATIVE: %s - Wrong dead key mapping.\n", nProc));
				break;
		}
	} else buffer[0] = '\0';

	jboolean tState = (jboolean)(keyState[vkCode]=FALSE);
	switch(event) {
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
			tState = (jboolean)(keyState[vkCode]=TRUE);
			/* no break */
		case WM_KEYUP: case WM_SYSKEYUP: {
			JNIEnv* env;
			if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL)>=JNI_OK)
				(*env)->CallVoidMethod(env, hookObj[HOOK_KEYBOARD], handleMeth[HOOK_KEYBOARD], vkCode, tState, (jchar)buffer[0], (jlong)(uintptr_t)hDevice);
			else DEBUG_PRINT(("NATIVE: %s - Error on the attach current thread.\n", nProc));
		}
	}
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)  {
	KBDLLHOOKSTRUCT* pStruct = (KBDLLHOOKSTRUCT*)lParam;

	handleKey("LowLevelKeyboardProc", wParam, pStruct->vkCode, pStruct->scanCode, 0);

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)  {
	if(nCode==HC_ACTION) {
		JNIEnv* env; MSLLHOOKSTRUCT* pStruct = (MSLLHOOKSTRUCT*)lParam;
		if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL)>=JNI_OK) {
			jint tState = (jint)TS_UP, lPosX = (jint)pStruct->pt.x, lPosY = (jint)pStruct->pt.y, mkButton = 0;
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
					if(lPosX!=lOldX||lOldX!=lOldY)
						(*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE], tState, (jint)0, /*(jint)wParam,*/lOldX=lPosX, lOldY=lPosY, (jint)0);
					break;
				case WM_MOUSEWHEEL:
					(*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE], tState=TS_WHEEL, (jint)0, /*(jint)GET_KEYSTATE_WPARAM(wParam),*/lPosX, lPosY, (jint)GET_WHEEL_DELTA_WPARAM(pStruct->mouseData));
					break;
			}
			if(tState<=TS_DOWN&&mkButton!=0) //tState==TS_DOWN||tState==TS_UP
				(*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE], tState, mkButton, /*(jint)wParam,*/lPosX, lPosY, (jint)0);
		} else DEBUG_PRINT(("NATIVE: LowLevelMouseProc - Error on the attach current thread.\n"));
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
LRESULT CALLBACK WndProc(HWND hWndMain, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg){
		case WM_CREATE: {
			// determine the hook window (by lpszClassName)
			size_t hook = SHRT_MAX; for(size_t cmpClsName=0;cmpClsName<sizeof(lpszClassNames)/sizeof(lpszClassNames[0]);cmpClsName++)
				if(lstrcmp(((CREATESTRUCT*)lParam)->lpszClass, lpszClassNames[cmpClsName])==0)
					{ hook = cmpClsName; break; }
			if(hook==SHRT_MAX) return FALSE; // unknown hook

			// build raw input device identifier (rid)
			rid[hook].dwFlags = RIDEV_NOLEGACY|RIDEV_INPUTSINK; // ignore legacy messages and receive system wide keystrokes
			rid[hook].usUsagePage = 1; // raw data only
			switch(hook) {
				case HOOK_KEYBOARD:
					rid[hook].usUsage = 6; // keyboard
					break;
				case HOOK_MOUSE:
					rid[hook].usUsage = 2; // mouse
					break;
			}
			rid[hook].hwndTarget = hWndMain;

			// register interest in raw data
			RegisterRawInputDevices(&rid[hook], 1, sizeof(rid[hook]));

			// get initial state of keyboard
			GetKeyboardState((PBYTE)&keyState);

			break;
		}
		case WM_INPUT: {
			UINT dwSize;
			if(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER))==UINT_MAX)
				break;

			LPBYTE* lpb;
			if((lpb=malloc(dwSize*sizeof(BYTE)))==NULL)
				break;

			if(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER))!=dwSize) {
				free(lpb);
				break;
			}

			PRAWINPUT raw = (PRAWINPUT)lpb;
			HANDLE hDevice = raw->header.hDevice;
			switch(raw->header.dwType) {
				case RIM_TYPEKEYBOARD: {
					UINT event = raw->data.keyboard.Message;
					USHORT vkCode = raw->data.keyboard.VKey,
						scanCode = raw->data.keyboard.MakeCode;
					free(lpb); // free this now to save memory

					handleKey("WndProc", event, vkCode, scanCode, hDevice);

					break;
				}
				case RIM_TYPEMOUSE: {
					LONG lLastX = raw->data.mouse.lLastX, lLastY = raw->data.mouse.lLastY;
					USHORT buttonFlags = raw->data.mouse.usButtonFlags, buttonData = raw->data.mouse.usButtonData;
					free(lpb); // free this now to save memory

					JNIEnv* env;
					if((*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL)>=JNI_OK) {
						jint tState = (jint)TS_UP, mkButton = 0;
						if((buttonFlags&mouseLeftButton)!=0) {
							mkButton = MK_LBUTTON;
							if((buttonFlags&RI_MOUSE_LEFT_BUTTON_DOWN)==RI_MOUSE_LEFT_BUTTON_DOWN)
								tState = (jint)TS_DOWN;
						} else if((buttonFlags&mouseRightButton)!=0) {
							mkButton = MK_RBUTTON;
							if((buttonFlags&RI_MOUSE_RIGHT_BUTTON_DOWN)==RI_MOUSE_RIGHT_BUTTON_DOWN)
								tState = (jint)TS_DOWN;
						} else if((buttonFlags&mouseMiddleButton)!=0) {
							mkButton = MK_MBUTTON;
							if((buttonFlags&RI_MOUSE_MIDDLE_BUTTON_DOWN)==RI_MOUSE_MIDDLE_BUTTON_DOWN)
								tState = (jint)TS_DOWN;
						}
						// handle mouse buttons
						if(mkButton!=0) (*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE],
							tState, mkButton, lLastX, lLastY, (jint)0, (jlong)(uintptr_t)hDevice);

						// handle mouse move
						if(lLastX!=0||lLastY!=0) (*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE],
							(jint)TS_MOVE, (jint)0, lLastX, lLastY, (jint)0, (jlong)(uintptr_t)hDevice);

						// handle mouse wheel
						if((buttonFlags&RI_MOUSE_WHEEL)==RI_MOUSE_WHEEL) (*env)->CallVoidMethod(env, hookObj[HOOK_MOUSE], handleMeth[HOOK_MOUSE],
							(jint)TS_WHEEL, (jint)0, lLastX, lLastY, (jint)((short)buttonData), (jlong)(uintptr_t)hDevice);
					} else DEBUG_PRINT(("NATIVE: LowLevelMouseProc - Error on the attach current thread.\n"));

					break;
				}
			}
			break;
		}
		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWndMain, uMsg, wParam, lParam);
	}
	return FALSE;
}

static inline _Bool notifyHookObj(JNIEnv *env, size_t hook) {
	jclass objectCls = (*env)->FindClass(env, "java/lang/Object");
	jmethodID notifyMeth = (*env)->GetMethodID(env, objectCls, "notify", "()V");
	if(notifyMeth==0)
		return FALSE;

	(*env)->MonitorEnter(env, hookObj[hook]);
	(*env)->CallVoidMethod(env, hookObj[hook], notifyMeth);
	(*env)->MonitorExit(env, hookObj[hook]);

	return TRUE;
}

static inline jint registerHook(JNIEnv *env, jobject thisObj, size_t hook, const char *handleName, const char *handleSig, jboolean raw) {
	DEBUG_PRINT(("NATIVE: registerHook - Hook start\n"));
	
	if(jvm==NULL) (*env)->GetJavaVM(env, &jvm);
	hookThreadId[hook] = GetCurrentThreadId();

	hookObj[hook] = (*env)->NewGlobalRef(env, thisObj);
	jclass hookCls = (*env)->GetObjectClass(env, hookObj[hook]);
	if((handleMeth[hook]=(*env)->GetMethodID(env, hookCls, handleName, handleSig))==0) {
		(*env)->ExceptionClear(env);
		DEBUG_PRINT(("NATIVE: registerHook - No handle method!\n"));
		return (jint)E_NO_HANDLE_METHOD;
	}

	HHOOK hHook; HWND hWnd;
	switch(raw) {
		case JNI_FALSE:
			switch(hook) {
				case HOOK_KEYBOARD:
					hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
					break;
				case HOOK_MOUSE:
					hHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInst, 0);
					break;
			}

			break;
		case JNI_TRUE: {
			WNDCLASS wndCls = {0};
			wndCls.lpfnWndProc = WndProc;
			wndCls.hInstance = hInst;
			wndCls.lpszClassName = lpszClassNames[hook];
			RegisterClass(&wndCls);

			// create a invisible Message-Only Window (https://msdn.microsoft.com/library/windows/desktop/ms632599.aspx#message_only)
			hWnd = CreateWindow(wndCls.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, NULL);

			break;
		}
	}
	if(hHook==NULL&&hWnd==NULL) {
		debugPrintLastError("NATIVE: registerHook - Hook failed");
		return (jint)E_HOOK_FAILED;
	} else DEBUG_PRINT(("NATIVE: registerHook - %sHook success\n", raw?"Raw ":""));

	if(!notifyHookObj(env, hook)) {
		(*env)->ExceptionClear(env);
		DEBUG_PRINT(("NATIVE: registerHook - Notify failed!\n"));
		return (jint)E_NOTIFY_FAILED;
	}

	MSG message;
	while(GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	(*env)->DeleteGlobalRef(env, hookObj[hook]);
	if((hHook!=NULL&&UnhookWindowsHookEx(hHook))||(hWnd!=NULL&&DestroyWindow(hWnd))) {
		DEBUG_PRINT(("NATIVE: registerHook - Unhook success\n"));
		return (jint)E_SUCCESS;
	} else {
		debugPrintLastError("NATIVE: registerHook - Unhook failed");
		return (jint)E_UNHOOK_FAILED;
	}
}
JNIEXPORT jint JNICALL Java_lc_kra_system_keyboard_GlobalKeyboardHook_00024NativeKeyboardHook_registerHook(JNIEnv *env, jobject thisObj, jboolean raw) {
	return registerHook(env, thisObj, HOOK_KEYBOARD, "handleKey", "(IICJ)V", raw);
}
JNIEXPORT jint JNICALL Java_lc_kra_system_mouse_GlobalMouseHook_00024NativeMouseHook_registerHook(JNIEnv *env, jobject thisObj, jboolean raw) {
	return registerHook(env, thisObj, HOOK_MOUSE, "handleMouse", "(IIIIIJ)V", raw);
}

static inline void unregisterHook(size_t hook) {
	if(hookThreadId[hook]!=0) {
		DEBUG_PRINT(("NATIVE: unregisterHook - Call PostThreadMessage WM_QUIT.\n"));
		DWORD threadId = hookThreadId[hook];
		hookThreadId[hook] = 0; //avoid race condition
		PostThreadMessage(threadId, WM_QUIT, 0, 0L);
	}
}
JNIEXPORT void JNICALL Java_lc_kra_system_keyboard_GlobalKeyboardHook_00024NativeKeyboardHook_unregisterHook(JNIEnv *env, jobject thisObj) {
	unregisterHook(HOOK_KEYBOARD);
}
JNIEXPORT void JNICALL Java_lc_kra_system_mouse_GlobalMouseHook_00024NativeMouseHook_unregisterHook(JNIEnv *env, jobject thisObj) {
	unregisterHook(HOOK_MOUSE);
}

static inline jobject listDevices(JNIEnv *env, DWORD dwType) {
	UINT nDevices;
	if(GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST))!=0) {
		debugPrintLastError("NATIVE: listDevices - Sizing device list failed");
		return NULL;
	}

	PRAWINPUTDEVICELIST pRawInputDeviceList;
	if((pRawInputDeviceList=malloc(sizeof(RAWINPUTDEVICELIST)*nDevices))==NULL) {
		DEBUG_PRINT(("NATIVE: listDevices - Device malloc failed.\n"));
		return NULL;
	}

	if(GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST))==UINT_MAX) {
		free(pRawInputDeviceList);
		debugPrintLastError("NATIVE: listDevices - Listing devices failed");
		return NULL;
	}

	jclass hashMapCls = (*env)->FindClass(env, "java/util/HashMap");
	jobject hashMapDeviceList = (*env)->NewObject(env, hashMapCls, (*env)->GetMethodID(env, hashMapCls, "<init>", "()V"));
	jmethodID putMeth = (*env)->GetMethodID(env, hashMapCls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

	jclass longCls = (*env)->FindClass(env, "java/lang/Long");
	jmethodID valueOfMeth = (*env)->GetStaticMethodID(env, longCls, "valueOf", "(J)Ljava/lang/Long;");

	for(size_t nDevice=0; nDevice<nDevices; nDevice++) {
		if(pRawInputDeviceList[nDevice].dwType!=dwType)
			continue;

		jstring deviceName = NULL; UINT pcbSize;
		if(GetRawInputDeviceInfo(pRawInputDeviceList[nDevice].hDevice, RIDI_DEVICENAME, NULL, &pcbSize)!=UINT_MAX&&pcbSize!=0) {
			char pData[pcbSize];
			if(GetRawInputDeviceInfo(pRawInputDeviceList[nDevice].hDevice, RIDI_DEVICENAME, pData, &pcbSize)!=UINT_MAX)
				deviceName = (*env)->NewStringUTF(env, pData);
		}

		(*env)->CallObjectMethod(env, hashMapDeviceList, putMeth,
			(*env)->CallStaticObjectMethod(env, longCls, valueOfMeth, (jlong)(uintptr_t)pRawInputDeviceList[nDevice].hDevice),
			deviceName /* null if device name could not be determined (return device handle in map anyway) */);
	}

	free(pRawInputDeviceList);
	return hashMapDeviceList;
}
JNIEXPORT jobject JNICALL Java_lc_kra_system_keyboard_GlobalKeyboardHook_00024NativeKeyboardHook_listDevices(JNIEnv *env) {
	return listDevices(env, RIM_TYPEKEYBOARD);
}
JNIEXPORT jobject JNICALL Java_lc_kra_system_mouse_GlobalMouseHook_00024NativeMouseHook_listDevices(JNIEnv *env) {
	return listDevices(env, RIM_TYPEMOUSE);
}
