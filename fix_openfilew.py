import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """extern "C" WIN_ABI int meinos_GetSaveFileNameA(void* lpofn) {
    return meinos_GetOpenFileNameA(lpofn);
}

extern "C" WIN_ABI int meinos_GetOpenFileNameW(void* lpofn) {
    return 0; // Simulate cancel
}

extern "C" WIN_ABI int meinos_GetSaveFileNameW(void* lpofn) {
    return 0; // Simulate cancel
}"""

content = content.replace('extern "C" WIN_ABI int meinos_GetSaveFileNameA(void* lpofn) {\n    return meinos_GetOpenFileNameA(lpofn);\n}', new_funcs)

content = content.replace('if (str_iequals(func_name, "GetSaveFileNameA")) return (uint64_t)meinos_GetSaveFileNameA;', 'if (str_iequals(func_name, "GetSaveFileNameA")) return (uint64_t)meinos_GetSaveFileNameA;\n        if (str_iequals(func_name, "GetOpenFileNameW")) return (uint64_t)meinos_GetOpenFileNameW;\n        if (str_iequals(func_name, "GetSaveFileNameW")) return (uint64_t)meinos_GetSaveFileNameW;')

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
