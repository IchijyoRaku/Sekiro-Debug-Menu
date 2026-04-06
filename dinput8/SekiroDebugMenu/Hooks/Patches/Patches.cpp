#include "../../SekiroDebugMenu.h"

#include <Windows.h>

#include <algorithm>
#include <codecvt>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <locale>
#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
	enum class ELoggedTextCategory {
		Static,
		State,
		Dynamic,
		Mixed,
	};

	struct SLoggedLine {
		std::wstring wText;
		float fX;
		float fY;
		ELoggedTextCategory eCategory;
	};

	struct SExportConfig {
		bool bTextOutputEnabled = true;
		bool bLogOutputEnabled = true;
		bool bConsoleEnabled = false;
	};

	struct SPageCaptureState {
		bool bInitialised = false;
		bool bConsoleInitialised = false;
		std::wstring wDataDirectory;
		std::wstring wIniPath;
		std::wstring wOutputPath;
		std::wstring wLogPath;
		std::wstring wTranslationPath;
		std::wofstream fMenuTextLog;
		std::wofstream fDebugLog;
		std::set<std::wstring> sExportedTexts;
		std::map<std::wstring, std::wstring> mTranslations;
		SExportConfig sConfig;
	};

	SPageCaptureState gPageCapture;

	const wchar_t* kDataDirectoryName = L"dbgmenu_data";
	const wchar_t* kIniFileName = L"dbgmenu.ini";
	const wchar_t* kMenuTextFileName = L"menu_text.txt";
	const wchar_t* kDebugLogFileName = L"dbgmenu.log";
	const wchar_t* kTranslationFileName = L"zh-cn.json";
	const wchar_t* kIniSectionName = L"TextOutput";
	const wchar_t* kIniTextOutputKey = L"TextOutput";
	const wchar_t* kIniLogOutputKey = L"LogOutput";
	const wchar_t* kIniConsoleKey = L"Console";
	const UINT kDefaultCaptureKey = VK_F9;

	std::wstring TrimCopy(const std::wstring& wText) {
		size_t start = 0;
		while (start < wText.size() && iswspace(wText[start]))
			++start;

		size_t end = wText.size();
		while (end > start && iswspace(wText[end - 1]))
			--end;

		return wText.substr(start, end - start);
	}

	bool IsWhitespaceOnly(const std::wstring& wText) {
		for (wchar_t wc : wText) {
			if (!iswspace(wc))
				return false;
		}
		return true;
	}

	std::wstring SanitizeForLog(const std::wstring& wText) {
		std::wstring wSanitized = wText;
		for (wchar_t& wc : wSanitized) {
			if (wc == L'\r' || wc == L'\n' || wc == L'\t')
				wc = L' ';
		}
		return TrimCopy(wSanitized);
	}

	bool StartsWith(const std::wstring& wText, const std::wstring& wPrefix) {
		return wText.size() >= wPrefix.size() && wText.compare(0, wPrefix.size(), wPrefix) == 0;
	}

	bool EndsWith(const std::wstring& wText, const std::wstring& wSuffix) {
		return wText.size() >= wSuffix.size() && wText.compare(wText.size() - wSuffix.size(), wSuffix.size(), wSuffix) == 0;
	}

	std::wstring StripLeadingMarkers(const std::wstring& wText) {
		size_t index = 0;
		while (index < wText.size()) {
			const wchar_t wc = wText[index];
			if (wc == L'■' || wc == L'□' || wc == L'●' || wc == L'▼' || wc == L'▲' || wc == L'▽' || wc == L'◆' || wc == L'◇' || wc == L'○') {
				++index;
				continue;
			}
			if (iswspace(wc)) {
				++index;
				continue;
			}
			break;
		}
		return TrimCopy(wText.substr(index));
	}

	std::wstring StripTrailingParenthesizedCount(const std::wstring& wText) {
		std::wstring wValue = TrimCopy(wText);
		if (wValue.size() < 3)
			return wValue;

		size_t closePos = wValue.size() - 1;
		if (wValue[closePos] != L')')
			return wValue;

		size_t openPos = wValue.find_last_of(L'(');
		if (openPos == std::wstring::npos || openPos >= closePos)
			return wValue;

		std::wstring wInner = TrimCopy(wValue.substr(openPos + 1, closePos - openPos - 1));
		if (wInner.empty())
			return wValue;

		const bool bWildcard = (wInner == L"*" || wInner == L"-");
		const bool bNumeric = std::all_of(wInner.begin(), wInner.end(), [](wchar_t wc) { return iswdigit(wc); });
		if (!bWildcard && !bNumeric)
			return wValue;

		return TrimCopy(wValue.substr(0, openPos));
	}

	std::wstring StripNumericDecorators(const std::wstring& wText) {
		std::wstring wValue = TrimCopy(wText);
		if (wValue.empty())
			return wValue;

		wValue = StripLeadingMarkers(wValue);
		wValue = StripTrailingParenthesizedCount(wValue);

		if (StartsWith(wValue, L"【") && wValue.find(L"】") != std::wstring::npos) {
			size_t closePos = wValue.find(L"】");
			std::wstring wPrefix = wValue.substr(1, closePos - 1);
			const bool bAllDigits = !wPrefix.empty() && std::all_of(wPrefix.begin(), wPrefix.end(), [](wchar_t wc) { return iswdigit(wc); });
			const bool bOffOnState = (wPrefix == L"Off" || wPrefix == L"On" || wPrefix == L"OFF" || wPrefix == L"ON");
			if (bAllDigits || bOffOnState)
				wValue = TrimCopy(wValue.substr(closePos + 1));
		}

		if (StartsWith(wValue, L"[") && wValue.find(L"]") != std::wstring::npos) {
			size_t closePos = wValue.find(L"]");
			std::wstring wPrefix = wValue.substr(1, closePos - 1);
			std::wstring wUpper = wPrefix;
			std::transform(wUpper.begin(), wUpper.end(), wUpper.begin(), towupper);
			if (wUpper == L"ON" || wUpper == L"OFF")
				wValue = TrimCopy(wValue.substr(closePos + 1));
		}

		if (EndsWith(wValue, L"の数")) {
			size_t ltPos = wValue.find(L'<');
			if (ltPos != std::wstring::npos && ltPos + 1 < wValue.size())
				wValue = TrimCopy(wValue.substr(ltPos + 1));
		}

		return StripLeadingMarkers(TrimCopy(wValue));
	}

	bool LooksLikePureDynamicIdentifier(const std::wstring& wText) {
		if (wText.empty())
			return false;

		int alphaCount = 0;
		int digitCount = 0;
		int underscoreCount = 0;
		for (wchar_t wc : wText) {
			if ((wc >= L'A' && wc <= L'Z') || (wc >= L'a' && wc <= L'z'))
				++alphaCount;
			else if (iswdigit(wc))
				++digitCount;
			else if (wc == L'_')
				++underscoreCount;
			else if (wc == L' ')
				return false;
			else
				return false;
		}

		return underscoreCount >= 2 && digitCount >= 4 && alphaCount >= 1;
	}

	bool ContainsKana(const std::wstring& wText) {
		for (wchar_t wc : wText) {
			if ((wc >= 0x3040 && wc <= 0x309F) || (wc >= 0x30A0 && wc <= 0x30FF) || (wc >= 0x31F0 && wc <= 0x31FF))
				return true;
		}
		return false;
	}

	bool ContainsCJK(const std::wstring& wText) {
		for (wchar_t wc : wText) {
			if ((wc >= 0x4E00 && wc <= 0x9FFF) || (wc >= 0x3400 && wc <= 0x4DBF) || (wc >= 0xF900 && wc <= 0xFAFF))
				return true;
		}
		return false;
	}

	bool ContainsLatin(const std::wstring& wText) {
		for (wchar_t wc : wText) {
			if ((wc >= L'A' && wc <= L'Z') || (wc >= L'a' && wc <= L'z'))
				return true;
		}
		return false;
	}

	bool ContainsDigit(const std::wstring& wText) {
		for (wchar_t wc : wText) {
			if (iswdigit(wc))
				return true;
		}
		return false;
	}

	bool HasDynamicMarkers(const std::wstring& wText) {
		if (wText.find(L"0x") != std::wstring::npos || wText.find(L"0X") != std::wstring::npos)
			return true;
		if (wText.find(L"(") != std::wstring::npos && wText.find(L",") != std::wstring::npos && wText.find(L")") != std::wstring::npos)
			return true;

		int digitCount = 0;
		int punctuationCount = 0;
		for (wchar_t wc : wText) {
			if (iswdigit(wc))
				++digitCount;
			if (wc == L'.' || wc == L':' || wc == L'/' || wc == L'-' || wc == L'+')
				++punctuationCount;
		}

		if (digitCount >= 3)
			return true;
		if (digitCount >= 1 && punctuationCount >= 1)
			return true;
		return false;
	}

	bool IsKnownStateText(const std::wstring& wText) {
		static const wchar_t* pStateTexts[] = {
			L"ON", L"OFF", L"TRUE", L"FALSE", L"ENABLE", L"DISABLE", L"ENABLED", L"DISABLED",
			L"YES", L"NO", L"NONE", L"DEFAULT", L"OPEN", L"CLOSE"
		};

		std::wstring wUpper = wText;
		std::transform(wUpper.begin(), wUpper.end(), wUpper.begin(), towupper);
		for (const wchar_t* pState : pStateTexts) {
			if (wUpper == pState)
				return true;
		}
		return false;
	}

	bool NormalizeLoggedText(const std::wstring& wInput, std::wstring& wNormalized, ELoggedTextCategory& eCategory) {
		std::wstring wText = StripNumericDecorators(SanitizeForLog(wInput));
		if (wText.empty() || IsWhitespaceOnly(wText))
			return false;
		if (LooksLikePureDynamicIdentifier(wText))
			return false;

		const bool bHasKana = ContainsKana(wText);
		const bool bHasCJK = ContainsCJK(wText);
		const bool bHasLatin = ContainsLatin(wText);
		const bool bHasDigit = ContainsDigit(wText);

		if (IsKnownStateText(wText))
			eCategory = ELoggedTextCategory::State;
		else if (HasDynamicMarkers(wText) || (bHasDigit && wText.size() >= 6))
			eCategory = ELoggedTextCategory::Dynamic;
		else {
			const int languageKinds = (bHasKana ? 1 : 0) + (bHasCJK ? 1 : 0) + (bHasLatin ? 1 : 0);
			eCategory = (languageKinds >= 2) ? ELoggedTextCategory::Mixed : ELoggedTextCategory::Static;
		}

		wNormalized = wText;
		return true;
	}

	std::wstring GetModuleDirectoryPath() {
		wchar_t pPath[MAX_PATH] = {};
		HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
		GetModuleFileNameW(hModule, pPath, MAX_PATH);
		std::wstring wPath(pPath);
		size_t slashPos = wPath.find_last_of(L"\\/");
		if (slashPos == std::wstring::npos)
			return L".";
		return wPath.substr(0, slashPos);
	}

	std::wstring JoinPath(const std::wstring& wLeft, const std::wstring& wRight) {
		if (wLeft.empty())
			return wRight;
		if (wLeft.back() == L'\\' || wLeft.back() == L'/')
			return wLeft + wRight;
		return wLeft + L"\\" + wRight;
	}

	void EnsureDirectoryExists(const std::wstring& wPath) {
		CreateDirectoryW(wPath.c_str(), nullptr);
	}

	void OpenWideAppendFile(std::wofstream& file, const std::wstring& wPath) {
		file.open(wPath, std::ios::app);
		if (!file.is_open())
			return;
		file.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));
	}

	void OpenWideTruncateFile(std::wofstream& file, const std::wstring& wPath) {
		file.open(wPath, std::ios::trunc);
		if (!file.is_open())
			return;
		file.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));
	}

	void EnsureConsoleReady() {
		if (!gPageCapture.sConfig.bConsoleEnabled || gPageCapture.bConsoleInitialised)
			return;
		FILE* fp = nullptr;
		AllocConsole();
		SetConsoleTitleA("Sekiro Debug Menu Logger");
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		gPageCapture.bConsoleInitialised = true;
	}

	void DebugLog(const wchar_t* pwFormat, ...) {
		if ((!gPageCapture.sConfig.bLogOutputEnabled || !gPageCapture.fDebugLog.is_open()) && !gPageCapture.sConfig.bConsoleEnabled)
			return;

		wchar_t pBuffer[1024] = {};
		va_list args;
		va_start(args, pwFormat);
		vswprintf_s(pBuffer, pwFormat, args);
		va_end(args);

		if (gPageCapture.sConfig.bLogOutputEnabled && gPageCapture.fDebugLog.is_open()) {
			gPageCapture.fDebugLog << pBuffer << L"\n";
			gPageCapture.fDebugLog.flush();
		}
		if (gPageCapture.sConfig.bConsoleEnabled) {
			EnsureConsoleReady();
			if (gPageCapture.bConsoleInitialised)
				wprintf_s(L"%s\n", pBuffer);
		}
	}

	UINT ResolveKeyboardVirtualKey(const std::wstring& wKeyName) {
		std::wstring wUpper = wKeyName;
		std::transform(wUpper.begin(), wUpper.end(), wUpper.begin(), towupper);
		if (wUpper == L"LSHIFT" || wUpper == L"LEFTSHIFT" || wUpper == L"SHIFT")
			return VK_LSHIFT;
		if (wUpper == L"RSHIFT" || wUpper == L"RIGHTSHIFT")
			return VK_RSHIFT;
		if (wUpper == L"LCTRL" || wUpper == L"LEFTCTRL" || wUpper == L"CTRL")
			return VK_LCONTROL;
		if (wUpper == L"RCTRL" || wUpper == L"RIGHTCTRL")
			return VK_RCONTROL;
		if (wUpper == L"LALT" || wUpper == L"LEFTALT" || wUpper == L"ALT")
			return VK_LMENU;
		if (wUpper == L"RALT" || wUpper == L"RIGHTALT")
			return VK_RMENU;
		if (wUpper == L"INSERT")
			return VK_INSERT;
		if (wUpper == L"F9")
			return VK_F9;
		return kDefaultCaptureKey;
	}

	void WriteDefaultIni(const std::wstring& wIniPath) {
		WritePrivateProfileStringW(kIniSectionName, kIniTextOutputKey, L"on", wIniPath.c_str());
		WritePrivateProfileStringW(kIniSectionName, kIniLogOutputKey, L"on", wIniPath.c_str());
		WritePrivateProfileStringW(kIniSectionName, kIniConsoleKey, L"off", wIniPath.c_str());
	}

	void LoadConfig() {
		wchar_t pValue[64] = {};
		GetPrivateProfileStringW(kIniSectionName, kIniTextOutputKey, L"on", pValue, 64, gPageCapture.wIniPath.c_str());
		std::wstring wTextOutput = pValue;
		std::transform(wTextOutput.begin(), wTextOutput.end(), wTextOutput.begin(), towlower);
		gPageCapture.sConfig.bTextOutputEnabled = (wTextOutput != L"off");

		GetPrivateProfileStringW(kIniSectionName, kIniLogOutputKey, L"on", pValue, 64, gPageCapture.wIniPath.c_str());
		std::wstring wLogOutput = pValue;
		std::transform(wLogOutput.begin(), wLogOutput.end(), wLogOutput.begin(), towlower);
		gPageCapture.sConfig.bLogOutputEnabled = (wLogOutput != L"off");

		GetPrivateProfileStringW(kIniSectionName, kIniConsoleKey, L"off", pValue, 64, gPageCapture.wIniPath.c_str());
		std::wstring wConsole = pValue;
		std::transform(wConsole.begin(), wConsole.end(), wConsole.begin(), towlower);
		gPageCapture.sConfig.bConsoleEnabled = (wConsole == L"on");
	}

	std::wstring UnescapeJsonString(const std::wstring& wText) {
		std::wstring wOut;
		wOut.reserve(wText.size());
		for (size_t i = 0; i < wText.size(); ++i) {
			wchar_t wc = wText[i];
			if (wc == L'\\' && i + 1 < wText.size()) {
				wchar_t next = wText[++i];
				switch (next) {
				case L'"': wOut.push_back(L'"'); break;
				case L'\\': wOut.push_back(L'\\'); break;
				case L'/': wOut.push_back(L'/'); break;
				case L'b': wOut.push_back(L'\b'); break;
				case L'f': wOut.push_back(L'\f'); break;
				case L'n': wOut.push_back(L'\n'); break;
				case L'r': wOut.push_back(L'\r'); break;
				case L't': wOut.push_back(L'\t'); break;
				default: wOut.push_back(next); break;
				}
			}
			else {
				wOut.push_back(wc);
			}
		}
		return wOut;
	}

	void LoadTranslations() {
		gPageCapture.mTranslations.clear();
		std::wifstream file(gPageCapture.wTranslationPath);
		if (!file.is_open()) {
			DebugLog(L"[Translation] zh-cn.json not found: %s", gPageCapture.wTranslationPath.c_str());
			return;
		}
		file.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));
		std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
		file.close();

		size_t pos = 0;
		while (true) {
			size_t keyStart = content.find(L'"', pos);
			if (keyStart == std::wstring::npos)
				break;
			size_t keyEnd = content.find(L'"', keyStart + 1);
			if (keyEnd == std::wstring::npos)
				break;
			size_t colonPos = content.find(L':', keyEnd + 1);
			if (colonPos == std::wstring::npos)
				break;
			size_t valueStart = content.find(L'"', colonPos + 1);
			if (valueStart == std::wstring::npos)
				break;
			size_t valueEnd = valueStart + 1;
			while (true) {
				valueEnd = content.find(L'"', valueEnd);
				if (valueEnd == std::wstring::npos)
					break;
				if (content[valueEnd - 1] != L'\\')
					break;
				++valueEnd;
			}
			if (valueEnd == std::wstring::npos)
				break;

			std::wstring wKey = UnescapeJsonString(content.substr(keyStart + 1, keyEnd - keyStart - 1));
			std::wstring wValue = UnescapeJsonString(content.substr(valueStart + 1, valueEnd - valueStart - 1));
			gPageCapture.mTranslations[wKey] = wValue;
			pos = valueEnd + 1;
		}

		DebugLog(L"[Translation] loaded %u items from %s", static_cast<unsigned>(gPageCapture.mTranslations.size()), gPageCapture.wTranslationPath.c_str());
	}

	std::wstring BuildExportText(const std::wstring& wInput) {
		std::wstring wText = StripNumericDecorators(SanitizeForLog(wInput));
		if (wText.empty() || IsWhitespaceOnly(wText))
			return L"";
		if (LooksLikePureDynamicIdentifier(wText))
			return L"";
		return wText;
	}

	std::wstring TranslateDisplayText(const std::wstring& wDisplayText) {
		std::wstring wResult = wDisplayText;
		for (std::map<std::wstring, std::wstring>::const_iterator it = gPageCapture.mTranslations.begin(); it != gPageCapture.mTranslations.end(); ++it) {
			const std::wstring& wFrom = it->first;
			const std::wstring& wTo = it->second;
			if (wFrom.empty())
				continue;

			size_t pos = 0;
			while ((pos = wResult.find(wFrom, pos)) != std::wstring::npos) {
				wResult.replace(pos, wFrom.size(), wTo);
				pos += wTo.size();
			}
		}
		return wResult;
	}

	void ExportTextIfNeeded(const std::wstring& wDisplayText) {
		if (!gPageCapture.sConfig.bTextOutputEnabled)
			return;
		if (!gPageCapture.fMenuTextLog.is_open()) {
			DebugLog(L"[Export] skip: menu_text log file is not open");
			return;
		}

		std::wstring wExportText = BuildExportText(wDisplayText);
		if (wExportText.empty())
			return;
		if (gPageCapture.sExportedTexts.find(wExportText) != gPageCapture.sExportedTexts.end())
			return;

		gPageCapture.sExportedTexts.insert(wExportText);
		gPageCapture.fMenuTextLog << wExportText << L"\n";
		gPageCapture.fMenuTextLog.flush();
		DebugLog(L"[Export] wrote unique text: %s", wExportText.c_str());
	}

	void InitialiseCapture() {
		if (gPageCapture.bInitialised)
			return;

		gPageCapture.wDataDirectory = JoinPath(GetModuleDirectoryPath(), kDataDirectoryName);
		EnsureDirectoryExists(gPageCapture.wDataDirectory);
		gPageCapture.wIniPath = JoinPath(gPageCapture.wDataDirectory, kIniFileName);
		gPageCapture.wOutputPath = JoinPath(gPageCapture.wDataDirectory, kMenuTextFileName);
		gPageCapture.wLogPath = JoinPath(gPageCapture.wDataDirectory, kDebugLogFileName);
		gPageCapture.wTranslationPath = JoinPath(gPageCapture.wDataDirectory, kTranslationFileName);
		if (GetFileAttributesW(gPageCapture.wIniPath.c_str()) == INVALID_FILE_ATTRIBUTES)
			WriteDefaultIni(gPageCapture.wIniPath);
		LoadConfig();
		OpenWideAppendFile(gPageCapture.fMenuTextLog, gPageCapture.wOutputPath);
		OpenWideTruncateFile(gPageCapture.fDebugLog, gPageCapture.wLogPath);
		DebugLog(L"[Init] ini=%s", gPageCapture.wIniPath.c_str());
		DebugLog(L"[Init] output=%s", gPageCapture.wOutputPath.c_str());
		DebugLog(L"[Init] log=%s", gPageCapture.wLogPath.c_str());
		DebugLog(L"[Init] translation=%s", gPageCapture.wTranslationPath.c_str());
		DebugLog(L"[Init] text_output=%d log_output=%d console=%d", gPageCapture.sConfig.bTextOutputEnabled ? 1 : 0, gPageCapture.sConfig.bLogOutputEnabled ? 1 : 0, gPageCapture.sConfig.bConsoleEnabled ? 1 : 0);
		LoadTranslations();
		gPageCapture.bInitialised = true;
	}
}

void BeginMenuFrameCapture() {
	InitialiseCapture();
}

void EndMenuFrameCapture() {
}

void DrawMenuHook(uint64_t qUnkClass, SMenuDrawLocation* pLocationData, wchar_t* pwString) {

	CGraphics* Graphics = GetGraphics();
	if (!Graphics || !pLocationData || !pwString)
		return;

	std::wstring wDisplayText = SanitizeForLog(pwString);
	if (wDisplayText.empty() || IsWhitespaceOnly(wDisplayText))
		return;

	std::wstring wTranslatedText = TranslateDisplayText(wDisplayText);
	DebugLog(L"[Text] original=%s translated=%s", wDisplayText.c_str(), wTranslatedText.c_str());
	ExportTextIfNeeded(wDisplayText);

	int iArraySize = Graphics->GetDebugPrintArraySize();
	SDebugPrintStruct* pStruct = Graphics->GetDebugPrintArrayStart();

	Graphics->EnterCS();

	for (int i = 0; i < iArraySize; i++, pStruct++) {
		if (pStruct->bIsActive) continue;
		wcsncpy_s(pStruct->wText, wTranslatedText.c_str(), _TRUNCATE);
		pStruct->fX = pLocationData->f1;
		pStruct->fY = pLocationData->f2;
		pStruct->bIsActive = true;
		break;
	};

	Graphics->LeaveCS();
	return;
};
