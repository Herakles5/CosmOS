import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# Add a field to Control for items
control_struct = """struct Control {
    int type; // 0=none, 1=button, 2=static, 3=progress, 4=combobox
    int x, y, w, h;
    char text[64];
    int id;
    int state;
    int data;
    char items[10][32];
    int num_items;
    int selected_item;
    int is_dropped;
};"""
content = re.sub(r'struct Control \{.*?\};', control_struct, content, flags=re.DOTALL)

# In meinos_CreateWindowExA, initialize the new fields
init_new = """                windows[parent_slot].controls[c_idx].id = (int)(uint64_t)hMenu;
                windows[parent_slot].controls[c_idx].state = 0;
                windows[parent_slot].controls[c_idx].num_items = 0;
                windows[parent_slot].controls[c_idx].selected_item = 0;
                windows[parent_slot].controls[c_idx].is_dropped = 0;
                windows[parent_slot].num_controls++;"""
content = re.sub(r'                windows\[parent_slot\]\.controls\[c_idx\]\.id = \(int\)\(uint64_t\)hMenu;\n                windows\[parent_slot\]\.controls\[c_idx\]\.state = 0;\n                windows\[parent_slot\]\.num_controls\+\+;', init_new, content)

# In meinos_SendMessageA, implement CB_ADDSTRING and CB_SETCURSEL
send_msg = """extern "C" WIN_ABI int meinos_SendMessageA(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        uint64_t parent_slot = (hw >> 8) & 0xFF;
        uint64_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            Control* c = &windows[parent_slot].controls[c_idx];
            if (c->type == 3 && Msg == 0x0402) { // PBM_SETPOS
                c->state = (int)wParam;
                return (int)wParam;
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
                        return (int)wParam;
                    }
                    return -1;
                }
                if (Msg == 0x0147) { // CB_GETCURSEL
                    return c->selected_item;
                }
            }
        }
    }
    return 0;
}"""
content = re.sub(r'extern "C" WIN_ABI int meinos_SendMessageA\(void\* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam\) \{.*?\n\}', send_msg, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
