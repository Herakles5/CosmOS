import re

with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
#define GWLP_USERDATA -21
#define GWLP_WNDPROC -4

extern "C" WIN_ABI uint64_t meinos_SetWindowLongPtrA(uint64_t hWnd, int nIndex, uint64_t dwNewLong) {
    uint64_t w = hWnd;
    if (w < 20) {
        if (nIndex == GWLP_USERDATA || nIndex == -21) {
            uint64_t old = windows[w].user_data;
            windows[w].user_data = dwNewLong;
            return old;
        }
    } else {
        int p = (w >> 16) & 0xFFFF;
        int c = w & 0xFFFF;
        if (p < 20 && c < windows[p].num_controls) {
            if (nIndex == GWLP_USERDATA || nIndex == -21) {
                uint64_t old = windows[p].controls[c].user_data;
                windows[p].controls[c].user_data = dwNewLong;
                return old;
            }
            if (nIndex == GWLP_WNDPROC || nIndex == -4) {
                uint64_t old = windows[p].controls[c].wndproc;
                windows[p].controls[c].wndproc = dwNewLong;
                return old;
            }
        }
    }
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_GetWindowLongPtrA(uint64_t hWnd, int nIndex) {
    uint64_t w = hWnd;
    if (w < 20) {
        if (nIndex == GWLP_USERDATA || nIndex == -21) return windows[w].user_data;
    } else {
        int p = (w >> 16) & 0xFFFF;
        int c = w & 0xFFFF;
        if (p < 20 && c < windows[p].num_controls) {
            if (nIndex == GWLP_USERDATA || nIndex == -21) return windows[p].controls[c].user_data;
            if (nIndex == GWLP_WNDPROC || nIndex == -4) return windows[p].controls[c].wndproc;
        }
    }
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_SetWindowLongA(uint64_t hWnd, int nIndex, uint32_t dwNewLong) {
    return meinos_SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
}

extern "C" WIN_ABI uint64_t meinos_GetWindowLongA(uint64_t hWnd, int nIndex) {
    return meinos_GetWindowLongPtrA(hWnd, nIndex);
}

extern "C" WIN_ABI uint64_t meinos_CallWindowProcA(void* lpPrevWndFunc, uint64_t hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    if (!lpPrevWndFunc || lpPrevWndFunc == (void*)1) return 0;
    uint64_t (*wndproc)(uint64_t, uint32_t, uint64_t, uint64_t) = (uint64_t (*)(uint64_t, uint32_t, uint64_t, uint64_t))lpPrevWndFunc;
    return wndproc(hWnd, Msg, wParam, lParam);
}

extern "C" WIN_ABI void* meinos_FindResourceA(void* hModule, const char* lpName, const char* lpType) {
    return (void*)0x1234;
}
extern "C" WIN_ABI uint32_t meinos_SizeofResource(void* hModule, void* hResInfo) {
    return 64;
}
extern "C" WIN_ABI void* meinos_LoadResource(void* hModule, void* hResInfo) {
    return (void*)0x5678;
}
static uint8_t dummy_bmp[64] = {
    0x42, 0x4D, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
extern "C" WIN_ABI void* meinos_LockResource(void* hResData) {
    return dummy_bmp;
}

"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "GetWindowLongPtrA")) return (uint64_t)meinos_GetWindowLongPtrA;
        if (str_iequals(func_name, "SetWindowLongPtrA")) return (uint64_t)meinos_SetWindowLongPtrA;
        if (str_iequals(func_name, "GetWindowLongA")) return (uint64_t)meinos_GetWindowLongA;
        if (str_iequals(func_name, "SetWindowLongA")) return (uint64_t)meinos_SetWindowLongA;
        if (str_iequals(func_name, "CallWindowProcA")) return (uint64_t)meinos_CallWindowProcA;
        if (str_iequals(func_name, "FindResourceA")) return (uint64_t)meinos_FindResourceA;
        if (str_iequals(func_name, "SizeofResource")) return (uint64_t)meinos_SizeofResource;
        if (str_iequals(func_name, "LoadResource")) return (uint64_t)meinos_LoadResource;
        if (str_iequals(func_name, "LockResource")) return (uint64_t)meinos_LockResource;
"""

content = content.replace("if (str_iequals(func_name, \"DuplicateHandle\")) return (uint64_t)meinos_DuplicateHandle;", "if (str_iequals(func_name, \"DuplicateHandle\")) return (uint64_t)meinos_DuplicateHandle;" + mappings)

# Fix meinos_GetOpenFileNameA to avoid division by zero
content = content.replace("strncpy(ofn->lpstrFile, \"test.txt\", ofn->nMaxFile);", "strncpy((char*)ofn->lpstrFile, \"ArShiva_fixed.cpp\", ofn->nMaxFile);")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)

