import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_create = """    if (dwStyle & 0x40000000) { // WS_CHILD
        uint64_t parent_slot = (uint64_t)hWndParent;
        if (parent_slot < 20) {
            int c_idx = -1;
            for (int i = 0; i < windows[parent_slot].num_controls; i++) {
                if (windows[parent_slot].controls[i].type == 0) {
                    c_idx = i;
                    break;
                }
            }
            if (c_idx == -1 && windows[parent_slot].num_controls < 32) {
                c_idx = windows[parent_slot].num_controls;
                windows[parent_slot].num_controls++;
            }
            if (c_idx != -1) {
                windows[parent_slot].controls[c_idx].type = 1; // Temporary so it's not 0
"""

content = re.sub(r'    if \(dwStyle & 0x40000000\) \{ // WS_CHILD\n        uint64_t parent_slot = \(uint64_t\)hWndParent;\n        if \(parent_slot >= 0 && parent_slot < 20\) \{\n            int c_idx = windows\[parent_slot\]\.num_controls;\n            if \(c_idx < 32\) \{\n                windows\[parent_slot\]\.controls\[c_idx\]\.type = 0;', new_create, content, count=1)

new_destroy = """extern "C" WIN_ABI int meinos_DestroyWindow(void* hWnd) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        // It's a control
        uint64_t parent_slot = (hw >> 8) & 0xFF;
        uint64_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            windows[parent_slot].controls[c_idx].type = 0;
            return 1;
        }
    } else {
        uint64_t slot = hw;
        if (slot < 20) {
            windows[slot].open = false;
            if (windows[slot].pixels) {
                delete[] windows[slot].pixels;
                windows[slot].pixels = nullptr;
            }
            return 1;
        }
    }
    return 1;
}"""

content = re.sub(r'extern "C" WIN_ABI int meinos_DestroyWindow\(void\* hWnd\) \{.*?\n\}', new_destroy, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
