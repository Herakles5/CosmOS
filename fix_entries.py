with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

target = """                                if (dsk_mgr_picker_mode) {
                                    char full_z_path[256];
                                    snprintf(full_z_path, 256, "Z:\\\\%s", entries[i].filename);
                                    strncpy(dsk_mgr_picked_file, full_z_path, 255);"""

replacement = """                                if (dsk_mgr_picker_mode) {
                                    char full_z_path[256];
                                    snprintf(full_z_path, 256, "Z:\\\\%s", cfs_files[i].name);
                                    strncpy(dsk_mgr_picked_file, full_z_path, 255);"""

content = content.replace(target, replacement)
with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
