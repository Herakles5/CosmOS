import re

with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# 1. Add globals
if "bool dsk_mgr_picker_mode" not in content:
    content = content.replace("static uint32_t bg_color = 0x000000;", "static uint32_t bg_color = 0x000000;\nstatic bool dsk_mgr_picker_mode = false;\nstatic char dsk_mgr_picked_file[256] = {0};")

# 2. Update GetOpenFileNameA and GetSaveFileNameA
new_openfile = """extern "C" WIN_ABI int meinos_GetOpenFileNameA(void* lpofn) {
    if (!lpofn) return 0;
    dsk_mgr_picker_mode = true;
    dsk_mgr_picked_file[0] = 0;
    windows[4].open = true;
    windows[4].minimized = false;
    focus_window(4);
    
    while (windows[4].open && dsk_mgr_picker_mode) {
        sleep_ms(10);
    }
    dsk_mgr_picker_mode = false;
    
    if (dsk_mgr_picked_file[0] != 0) {
        char* lpstrFile = *(char**)((uint8_t*)lpofn + 48);
        if (lpstrFile) {
            strncpy(lpstrFile, dsk_mgr_picked_file, 255);
            return 1;
        }
    }
    return 0; // Simulate cancel
}

extern "C" WIN_ABI int meinos_GetSaveFileNameA(void* lpofn) {
    if (!lpofn) return 0;
    char* lpstrFile = *(char**)((uint8_t*)lpofn + 48);
    if (lpstrFile) {
        strncpy(lpstrFile, "Z:\\\\new_image.bin", 255);
        return 1;
    }
    return 0;
}"""

content = re.sub(r'extern "C" WIN_ABI int meinos_GetOpenFileNameA\(void\* lpofn\) \{.*?\nextern "C" WIN_ABI int meinos_GetSaveFileNameA\(void\* lpofn\) \{.*?\n\}', new_openfile, content, flags=re.DOTALL)

# 3. Update DSK MGR OPEN button for Linux Drive
linux_open = """_15(input_cooldown EQ 0 AND mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx+190, y_off, 40, 16)) {
                                if (!is_active) focus_window(win->id);
                                if (dsk_mgr_picker_mode) {
                                    char full_z_path[256];
                                    snprintf(full_z_path, 256, "Z:\\\\%s", entries[i].filename);
                                    strncpy(dsk_mgr_picked_file, full_z_path, 255);
                                    dsk_mgr_picker_mode = false;
                                    windows[4].open = false;
                                    input_cooldown = 25;
                                } else {"""
content = content.replace("""_15(input_cooldown EQ 0 AND mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx+190, y_off, 40, 16)) {
                                if (!is_active) focus_window(win->id);""", linux_open)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
