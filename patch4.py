with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI int meinos_OpenProcessToken(void* ProcessHandle, uint32_t DesiredAccess, void** TokenHandle) {
    if (TokenHandle) *TokenHandle = (void*)1;
    return 1;
}

extern "C" WIN_ABI int meinos_LookupPrivilegeValueA(const char* lpSystemName, const char* lpName, void* lpLuid) {
    return 1;
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "OpenProcessToken")) return (uint64_t)meinos_OpenProcessToken;
        if (str_iequals(func_name, "LookupPrivilegeValueA")) return (uint64_t)meinos_LookupPrivilegeValueA;
        if (str_iequals(func_name, "LookupPrivilegeValueW")) return (uint64_t)meinos_LookupPrivilegeValueA;
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
