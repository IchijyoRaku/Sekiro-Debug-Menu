#include <windows.h>
#include <stdio.h>
#include "SekiroDebugMenu/SekiroDebugMenu.h"

HINSTANCE mHinst = 0, mHinstDLL = 0;
extern "C" UINT_PTR mProcs[6] = { 0 };

LPCSTR mImportNames[6] = {

	"DirectInput8Create",
	"GetdfDIJoystick",
	"DllCanUnloadNow",
	"DllGetClassObject",
	"DllRegisterServer",
	"DllUnregisterServer",

};

void Begin() {

	char dllpath[MAX_PATH];
	GetSystemDirectoryA(dllpath, MAX_PATH);
	strcat_s(dllpath, "\\dinput8.dll");
	mHinstDLL = LoadLibraryA(dllpath);
	if (!mHinstDLL) {
		MessageBoxA(NULL, "Failed to load original DLL", "Error", MB_ICONERROR);
		return;
	};

	for (int i = 0; i < 6; i++)
		mProcs[i] = (UINT_PTR)GetProcAddress(mHinstDLL, mImportNames[i]);

	EarlyBootLog(L"[InitChain] Begin enter: mHinstDLL=0x%llX", (uint64_t)mHinstDLL);
	for (int i = 0; i < 6; i++)
		EarlyBootLog(L"[InitChain] import[%d]=%S addr=0x%llX", i, mImportNames[i], (uint64_t)mProcs[i]);

	const bool bInitResult = Initialise();
	EarlyBootLog(L"[InitChain] Begin Initialise() result=%d", bInitResult ? 1 : 0);

	return;
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {

	switch (fdwReason) {
		case (DLL_PROCESS_ATTACH):{
			DisableThreadLibraryCalls(hinstDLL);
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Begin, 0, 0, 0);
			break;
		};
		case(DLL_PROCESS_DETACH): {

			FreeLibrary(mHinstDLL);

			break;
		};
	}

	return 1;
}

extern "C" void _DirectInput8Create();