import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

send_msg = """extern "C" WIN_ABI uint64_t meinos_SendMessageA(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        // Child control
        uint32_t parent_slot = (hw >> 8) & 0xFF;
        uint32_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            Control* c = &windows[parent_slot].controls[c_idx];
            if (c->type == 3 && Msg == 0x0402) { // PBM_SETPOS
                c->state = (int)wParam;
                return wParam;
            }
            if (c->type == 4) { // COMBOBOX
                if (Msg == 0x0143) { // CB_ADDSTRING
                    if (c->num_items < 10 && lParam) {
                        const char* str = (const char*)lParam;
                        int i = 0;
                        while(str[i] && i < 31) { c->items[c->num_items][i] = str[i]; i++; }
                        c->items[c->num_items][i] = 0;
                        c->num_items++;
                        return c->num_items - 1;
                    }
                    return -1; // CB_ERR
                }
                if (Msg == 0x014E) { // CB_SETCURSEL
                    if ((int)wParam >= 0 && (int)wParam < c->num_items) {
                        c->selected_item = (int)wParam;
                        return wParam;
                    }
                    return -1;
                }
                if (Msg == 0x0147) { // CB_GETCURSEL
                    return c->selected_item;
                }
            }
        }
        return 0;
    } else {
        return 0;
    }
}

extern "C" WIN_ABI uint64_t meinos_SendMessageW(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    uint64_t hw = (uint64_t)hWnd;
    if ((hw & 0x10000ULL) && Msg == 0x0143) { // CB_ADDSTRING
        uint32_t parent_slot = (hw >> 8) & 0xFF;
        uint32_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            Control* c = &windows[parent_slot].controls[c_idx];
            if (c->type == 4 && c->num_items < 10 && lParam) { // COMBOBOX
                const uint16_t* wstr = (const uint16_t*)lParam;
                int i = 0;
                while(wstr[i] && i < 31) { c->items[c->num_items][i] = (char)wstr[i]; i++; }
                c->items[c->num_items][i] = 0;
                c->num_items++;
                return c->num_items - 1;
            }
        }
    }
    return meinos_SendMessageA(hWnd, Msg, wParam, lParam);
}"""

content = re.sub(r'extern "C" WIN_ABI uint64_t meinos_SendMessageA\(void\* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam\) \{.*?\nextern "C" WIN_ABI uint64_t meinos_SendMessageW\(void\* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam\) \{.*?\n\}', send_msg, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
