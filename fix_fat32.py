with open("linux_stubs.cpp", "r") as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if line.startswith("    bool fat32_list_dir"):
        continue
    new_lines.append(line)

new_lines.append("\n#include \"cosmos_fat32.h\"\n")
new_lines.append("bool fat32_list_dir(int folder_lba, uint8_t* dir_buffer, FAT32_ParsedFile* output_files, int max_files, uint8_t current_folder_id, int current_page_offset) { return false; }\n")

with open("linux_stubs.cpp", "w") as f:
    f.writelines(new_lines)
