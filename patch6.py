with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI void meinos_ExitThread(uint32_t dwExitCode) {
    siglongjmp(jump_env, 1);
}

extern "C" WIN_ABI int* meinos_errno() {
    static __thread int thread_local_errno = 0;
    return &thread_local_errno;
}

extern "C" WIN_ABI int meinos_WideCharToMultiByte(uint32_t CodePage, uint32_t dwFlags, const uint16_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar) {
    if (cchWideChar < 0) {
        cchWideChar = 0;
        const uint16_t* p = lpWideCharStr;
        while(*p++) cchWideChar++;
    }
    if (cbMultiByte == 0) return cchWideChar;
    int len = cchWideChar < cbMultiByte ? cchWideChar : cbMultiByte;
    if (lpMultiByteStr && lpWideCharStr) {
        for(int i=0; i<len; i++) lpMultiByteStr[i] = (char)lpWideCharStr[i];
    }
    return len;
}

extern "C" WIN_ABI int meinos_MultiByteToWideChar(uint32_t CodePage, uint32_t dwFlags, const char* lpMultiByteStr, int cbMultiByte, uint16_t* lpWideCharStr, int cchWideChar) {
    if (cbMultiByte < 0) {
        cbMultiByte = 0;
        const char* p = lpMultiByteStr;
        while(*p++) cbMultiByte++;
    }
    if (cchWideChar == 0) return cbMultiByte;
    int len = cbMultiByte < cchWideChar ? cbMultiByte : cchWideChar;
    if (lpWideCharStr && lpMultiByteStr) {
        for(int i=0; i<len; i++) lpWideCharStr[i] = (uint16_t)lpMultiByteStr[i];
    }
    return len;
}

extern "C" WIN_ABI int meinos_wgetmainargs(int* _Argc, uint16_t*** _Argv, uint16_t*** _Env, int _DoWildCard, void* _StartInfo) {
    if (_Argc) *_Argc = 1;
    if (_Argv) {
        static uint16_t* argv[2] = {0, 0};
        static uint16_t arg0[] = {'C',':','\\\\','p','r','o','g','r','a','m','.','e','x','e',0};
        argv[0] = arg0;
        *_Argv = argv;
    }
    if (_Env) {
        static uint16_t* env[1] = {0};
        *_Env = env;
    }
    return 0; // Returns 0 on success in msvcrt
}

extern "C" WIN_ABI int meinos_PathFileExistsW(const uint16_t* pszPath) {
    return 1; // Always return true for now
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "ExitThread")) return (uint64_t)meinos_ExitThread;
        if (str_iequals(func_name, "TerminateThread")) return (uint64_t)meinos_ExitThread;
        if (str_iequals(func_name, "_errno")) return (uint64_t)meinos_errno;
        if (str_iequals(func_name, "WideCharToMultiByte")) return (uint64_t)meinos_WideCharToMultiByte;
        if (str_iequals(func_name, "MultiByteToWideChar")) return (uint64_t)meinos_MultiByteToWideChar;
        if (str_iequals(func_name, "__wgetmainargs")) return (uint64_t)meinos_wgetmainargs;
        if (str_iequals(func_name, "PathFileExistsW")) return (uint64_t)meinos_PathFileExistsW;
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
