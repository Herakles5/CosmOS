import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """extern "C" WIN_ABI void* meinos_SHBrowseForFolderA(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI void* meinos_SHBrowseForFolderW(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListA(void* pidl, char* pszPath) {
    return 0;
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListW(void* pidl, void* pszPath) {
    return 0;
}"""

content = re.sub(r'extern "C" WIN_ABI void\* meinos_SHBrowseForFolderA\(void\* lpbi\) \{.*?\nextern "C" WIN_ABI int meinos_SHGetPathFromIDListA\(void\* pidl, char\* pszPath\) \{.*?\n\}', new_funcs, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
