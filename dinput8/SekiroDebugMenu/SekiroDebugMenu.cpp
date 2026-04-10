#include "SekiroDebugMenu.h"

CSekiroDebugMenu* SekiroDebugMenu = nullptr;
CHooks* Hooks = nullptr;
CSignature* Signature = nullptr;
CGraphics* Graphics = nullptr;

bool Initialise() {

	EarlyBootLog(L"[InitChain] Initialise enter");
	SekiroDebugMenu = new CSekiroDebugMenu();
	EarlyBootLog(L"[InitChain] CSekiroDebugMenu allocated: 0x%llX", (uint64_t)SekiroDebugMenu);
	Hooks = SekiroDebugMenu->Hooks;
	Signature = SekiroDebugMenu->Signature;
	Graphics = SekiroDebugMenu->Graphics;
	EarlyBootLog(L"[InitChain] Hooks=0x%llX Signature=0x%llX Graphics=0x%llX", (uint64_t)Hooks, (uint64_t)Signature, (uint64_t)Graphics);

	EarlyBootLog(L"[InitChain] calling Hooks->Patch()");
	const bool bPatchResult = Hooks->Patch();
	EarlyBootLog(L"[InitChain] Hooks->Patch() result=%d", bPatchResult ? 1 : 0);
	if (!bPatchResult)
		return false;

	EarlyBootLog(L"[InitChain] calling Graphics->HookD3D11Present()");
	const bool bPresentHookResult = Graphics->HookD3D11Present();
	EarlyBootLog(L"[InitChain] Graphics->HookD3D11Present() result=%d", bPresentHookResult ? 1 : 0);
	if (!bPresentHookResult)
		return false;

	EarlyBootLog(L"[InitChain] Initialise success");
	return true;
};

CHooks* GetHooks() {
	if (!Hooks) {

	};
	return Hooks;
};
CSignature* GetSignature() {
	if (!Signature) {

	};
	return Signature;
};
CGraphics* GetGraphics() {
	if (!Graphics) {

	};
	return Graphics;
};
void InitDebug() {
#ifdef DEBUG
	FILE* fp;
	AllocConsole();
	SetConsoleTitleA("Sekiro - Debug Console");
	freopen_s(&fp, "CONOUT$", "w", stdout);
	DebugPrint(L"Starting...");
#endif
	return;
};

void DebugPrint(wchar_t* pwcMsg, ...) {
#ifdef DEBUG
	va_list vl;
	va_start(vl, pwcMsg);
	vwprintf_s(pwcMsg, vl);
	va_end(vl);
	wprintf_s(L"\n");
#endif
	return;
};

void Panic(const wchar_t* pcError, const wchar_t* pcMsg, int iError) {

	wchar_t pBuffer[256];

	DebugPrint(L"[Engine] Panic");
	DebugPrint(pBuffer);

	if (*(int*)0 == iError) {
		exit(0);
	};
	return;
};