#include "../SekiroDebugMenu.h"
#include "HookSites.h"

bool CHooks::Patch() {

	EarlyBootLog(L"[HookInit] Patch start: bIsMHActive=%d", bIsMHActive);
	if (bIsMHActive != MH_OK) {
		EarlyBootLog(L"[HookInit] Patch abort: MinHook not active (%d)", bIsMHActive);
		return false;
	};

	bool bReturn = true;
	uint8_t pMovAl01Bytes[] = { 0xB0, 0x01 };
	uint8_t pXorAlBytes[] = { 0x30, 0xC0 };
	uint8_t pCallBytes[] = { 0xE8 };
	uint8_t pRetBytes[] = { 0xC3 };

	while (!qActivateDebugMenu) {
		qActivateDebugMenu = GetSignature()->GetSignature(&sActivateDebugMenu);
		if (!qActivateDebugMenu)
			EarlyBootLog(L"[HookInit] waiting for sActivateDebugMenu signature");
		Sleep(25);
	};
	EarlyBootLog(L"[HookInit] sActivateDebugMenu found at 0x%llX", qActivateDebugMenu);

	qActivateDebugMenu += 9;
	EarlyBootLog(L"[HookInit] qActivateDebugMenu patched address = 0x%llX", qActivateDebugMenu);
	bReturn &= Tweak((void*)qActivateDebugMenu, pMovAl01Bytes, sizeof(pMovAl01Bytes));

	while (!GetHookSites()) {
		EarlyBootLog(L"[HookInit] waiting for hook sites");
		Sleep(100);
	}
	EarlyBootLog(L"[HookInit] hook sites ready: qMenuDrawHook=0x%llX qMissingParamHook1=0x%llX qMissingParamHook2=0x%llX", qMenuDrawHook, qMissingParamHook1, qMissingParamHook2);
	EarlyBootLog(L"[HookInit] other sites: qDisableRemnantMenuHook=0x%llX qDisableDebugFeature1=0x%llX qEnableFreezeCam=0x%llX qEnablePanCam=0x%llX", qDisableRemnantMenuHook, qDisableDebugFeature1, qEnableFreezeCam, qEnablePanCam);
	EarlyBootLog(L"[HookInit] feature sites: qEnableDebugFeature1=0x%llX qEnableDebugFeature2=0x%llX qEnableDebugFeature3=0x%llX", qEnableDebugFeature1, qEnableDebugFeature2, qEnableDebugFeature3);

	bReturn &= Hook((void*)qMenuDrawHook, DrawMenuHook, 0, 0) & Tweak((void*)qMenuDrawHook, pCallBytes, sizeof(pCallBytes));
	bReturn &= Tweak((void*)qMissingParamHook1, pRetBytes, sizeof(pRetBytes));
	bReturn &= Tweak((void*)qMissingParamHook2, pRetBytes, sizeof(pRetBytes));
	bReturn &= Tweak((void*)qDisableRemnantMenuHook, pXorAlBytes, sizeof(pXorAlBytes));

	bReturn &= Tweak((void*)qEnableDebugFeature1, pMovAl01Bytes, sizeof(pMovAl01Bytes));
	bReturn &= Tweak((void*)qEnableDebugFeature2, pMovAl01Bytes, sizeof(pMovAl01Bytes));
	bReturn &= Tweak((void*)qEnableDebugFeature3, pMovAl01Bytes, sizeof(pMovAl01Bytes));

	bReturn &= Tweak((void*)qDisableDebugFeature1, pRetBytes, sizeof(pRetBytes));
	DebugPrint((wchar_t*)L"[HookInit] patch stage result so far = %d", bReturn ? 1 : 0);

	uint8_t pFreezeCamBytes[] = { 0xC6, 0x05, 0xEF, 0xED, 0x53, 0x03, 0x02, 0x8B, 0x83, 0xE0, 0x00, 0x00, 0x00, 0xFF, 0xC8, 0x83, 0xF8, 0x01, 0x0F, 0x87, 0x2C, 0x02, 0x00, 0x00, 0xC6, 0x05, 0xD7, 0xED, 0x53, 0x03, 0x01 };
	bReturn &= Tweak((void*)qEnableFreezeCam, pFreezeCamBytes, sizeof(pFreezeCamBytes));
	uint8_t pPanCamBytes[] = { 0xE8, 0x99, 0x06, 0xDB, 0x01 };
	bReturn &= Tweak((void*)qEnablePanCam, pPanCamBytes, sizeof(pPanCamBytes));
	DebugPrint((wchar_t*)L"[HookInit] Patch finished: result=%d", bReturn ? 1 : 0);

	return true;
};

bool CHooks::GetHookSites() {

	CSignature* Signature = GetSignature();
	EarlyBootLog(L"[HookSite] GetHookSites start");

	if (!qMenuDrawHook) {
		uint64_t qMenuDrawHookRaw = Signature->GetSignature(&sMenuDrawHook);
		EarlyBootLog(L"[HookSite] sMenuDrawHook raw = 0x%llX", qMenuDrawHookRaw);
		qMenuDrawHook = qMenuDrawHookRaw;
		if (!qMenuDrawHook)
			return false;
		else {
			qMenuDrawHook += 40;
			EarlyBootLog(L"[HookSite] qMenuDrawHook final = 0x%llX", qMenuDrawHook);
		}
	};
	if (!qMissingParamHook1) {
		uint64_t qMissingParamHook1Raw = Signature->GetSignature(&sDisableMissingParam1);
		DebugPrint((wchar_t*)L"[HookSite] sDisableMissingParam1 raw = 0x%llX", qMissingParamHook1Raw);
		qMissingParamHook1 = qMissingParamHook1Raw;
		if (!qMissingParamHook1)
			return false;
		else {
			qMissingParamHook1 += 9;
			DebugPrint((wchar_t*)L"[HookSite] qMissingParamHook1 final = 0x%llX", qMissingParamHook1);
		}
	};
	if (!qMissingParamHook2) {
		qMissingParamHook2 = Signature->GetSignature(&sDisableMissingParam2);
		DebugPrint((wchar_t*)L"[HookSite] qMissingParamHook2 final = 0x%llX", qMissingParamHook2);
		if (!qMissingParamHook2)
			return false;
	};
	if (!qDisableRemnantMenuHook) {
		uint64_t qDisableRemnantMenuHookRaw = Signature->GetSignature(&sDisableRemnantMenu);
		DebugPrint((wchar_t*)L"[HookSite] sDisableRemnantMenu raw = 0x%llX", qDisableRemnantMenuHookRaw);
		qDisableRemnantMenuHook = qDisableRemnantMenuHookRaw;
		if (!qDisableRemnantMenuHook)
			return false;
		else {
			qDisableRemnantMenuHook += 49;
			DebugPrint((wchar_t*)L"[HookSite] qDisableRemnantMenuHook final = 0x%llX", qDisableRemnantMenuHook);
		}
	};
	if (!qDisableDebugFeature1) {
		qDisableDebugFeature1 = Signature->GetSignature(&sSigDisableFeature1);
		DebugPrint((wchar_t*)L"[HookSite] qDisableDebugFeature1 = 0x%llX", qDisableDebugFeature1);
		if (!qDisableDebugFeature1)
			return false;
	};


	if (!qEnableFreezeCam) {
		qEnableFreezeCam = Signature->GetSignature(&sSigEnableFreezeCam);
		DebugPrint((wchar_t*)L"[HookSite] qEnableFreezeCam raw = 0x%llX", qEnableFreezeCam);
		if (!qEnableFreezeCam)
			return false;
		else {
			qEnablePanCam = qEnableFreezeCam - 83;
			DebugPrint((wchar_t*)L"[HookSite] qEnablePanCam derived = 0x%llX", qEnablePanCam);
		}
	};
	uint64_t qEnableDebugFeature = Signature->GetSignature(&sSigEnable3Areas);
	DebugPrint((wchar_t*)L"[HookSite] qEnableDebugFeature raw = 0x%llX", qEnableDebugFeature);
	qEnableDebugFeature1 = qEnableDebugFeature + 8;
	qEnableDebugFeature2 = qEnableDebugFeature + 24;
	qEnableDebugFeature3 = qEnableDebugFeature + 40;
	DebugPrint((wchar_t*)L"[HookSite] qEnableDebugFeature1 = 0x%llX qEnableDebugFeature2 = 0x%llX qEnableDebugFeature3 = 0x%llX", qEnableDebugFeature1, qEnableDebugFeature2, qEnableDebugFeature3);
	DebugPrint((wchar_t*)L"[HookSite] GetHookSites success");

	return true;
};

bool CHooks::Hook(void* pHookSite, void* pDetour, DWORD64* pRetn, int iLen) {
	if (pRetn) *pRetn = (DWORD64)pHookSite + iLen;
	const MH_STATUS createStatus = MH_CreateHook(pHookSite, pDetour, 0);
		EarlyBootLog(L"[HookCall] MH_CreateHook site=0x%llX detour=0x%llX status=%d", (uint64_t)pHookSite, (uint64_t)pDetour, createStatus);
if (createStatus != MH_OK) return false;
	const MH_STATUS enableStatus = MH_EnableHook(pHookSite);
	EarlyBootLog(L"[HookCall] MH_EnableHook site=0x%llX status=%d", (uint64_t)pHookSite, enableStatus);
	if (enableStatus != MH_OK) return false;
	return true;
};

bool CHooks::Tweak(void* pMem, void* pNewBytes, int iLen) {
	DWORD dOldProtect;
	if (!VirtualProtect(pMem, iLen, PAGE_READWRITE, &dOldProtect)) {
		EarlyBootLog(L"[HookTweak] VirtualProtect RW failed site=0x%llX len=%d err=%u", (uint64_t)pMem, iLen, GetLastError());
		return false;
	}
	memcpy(pMem, pNewBytes, iLen);
	const BOOL bRestore = VirtualProtect(pMem, iLen, dOldProtect, &dOldProtect);
	EarlyBootLog(L"[HookTweak] write site=0x%llX len=%d restore=%d", (uint64_t)pMem, iLen, bRestore ? 1 : 0);
	if (!bRestore)
		EarlyBootLog(L"[HookTweak] restore protect failed site=0x%llX err=%u", (uint64_t)pMem, GetLastError());
	return bRestore != FALSE;
};

bool CHooks::Hookless(void** pHookMem, void* pDetour, void** pOld) {
	DWORD dOldProtect;
	if (!VirtualProtect(pHookMem, 8, PAGE_READWRITE, &dOldProtect)) return false;
	*pOld = *pHookMem;
	*pHookMem = pDetour;
	return VirtualProtect(pHookMem, 8, dOldProtect, &dOldProtect);
};