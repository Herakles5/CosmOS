bool disk_read_auto(uint32_t lba, uint64_t target_ram_addr) {
#ifdef __linux__
    if (selected_drive_idx == -1) {
        if (lba == 1002) {
        /// DIRECTORY READ (CFS LBA 1002)
        CFS_DIR_ENTRY* entries = (CFS_DIR_ENTRY*)target_ram_addr;
        for (int i=0; i<28; i++) entries[i].type = 0;
        
        const char* path = ".";
        if (selected_drive_idx == 1) path = "/media";
        
        DIR* dir = opendir(path);
        if (dir) {
            int slot = 0;
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL && slot < 28) {
                if (ent->d_name[0] == '.') continue;
                int len = 0; while(ent->d_name[len]) len++;
                
                // FILTER HACK: Hide Linux build files to leave room for Windows EXEs
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 'c' && ent->d_name[len-2] == 'p' && ent->d_name[len-1] == 'p') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'o') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'h') continue;
                if (len > 3 && ent->d_name[len-3] == '.' && ent->d_name[len-2] == 's' && ent->d_name[len-1] == 'h') continue;
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 't' && ent->d_name[len-2] == 'x' && ent->d_name[len-1] == 't') continue;
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 'i' && ent->d_name[len-2] == 'm' && ent->d_name[len-1] == 'g') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'l') continue;
                
                if (ent->d_type != DT_DIR) {
                    int has_ext = 0;
                    for (int k=0; k<len; k++) if (ent->d_name[k] == '.') has_ext = 1;
                    if (!has_ext) continue;
                }
                
                entries[slot].type = (ent->d_type == DT_DIR) ? 2 : 1;
                
                if (len > 4 && ent->d_name[len-4] == '.') {
                    strncpy((char*)entries[slot].filename, ent->d_name, 7);
                    entries[slot].filename[7] = '.';
                    entries[slot].filename[8] = ent->d_name[len-3];
                    entries[slot].filename[9] = ent->d_name[len-2];
                    entries[slot].filename[10] = ent->d_name[len-1];
                } else {
                    strncpy((char*)entries[slot].filename, ent->d_name, 10);
                    entries[slot].filename[10] = 0;
                }
                entries[slot].start_lba = 1000000 + slot; 
                
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);
                strncpy(linux_file_paths[slot], full_path, 255);
                linux_file_paths[slot][255] = 0;
                
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    entries[slot].file_size = st.st_size;
                } else {
                    entries[slot].file_size = 0;
                }
                slot++;
            }
            closedir(dir);
        }
        return true;
    } else if (lba >= 1000000 && lba < 1000028) {
