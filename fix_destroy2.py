import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_destroy = """extern "C" WIN_ABI int meinos_DestroyWindow(void* hWnd) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        // It's a control
        uint64_t parent_slot = (hw >> 8) & 0xFF;
        uint64_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            // We just mark it as invisible by setting type = 0 or we could shift the array.
            // Shifting array is better so we don't run out of the 32 slots.
            for (int i = c_idx; i < windows[parent_slot].num_controls - 1; i++) {
                windows[parent_slot].controls[i] = windows[parent_slot].controls[i + 1];
            }
            windows[parent_slot].num_controls--;
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
