#ifdef __linux__
                    _15(selected_drive_idx == -1 AND need_ui_refresh) {
                        _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                        simulator_page_offset = current_page_offset; disk_read_auto(1002, (uint64_t)buf_dir);
                        CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                        _39(int i=0; i<28; i++) {
                            cfs_files[i].exists = 0;
                            cfs_files[i].is_folder = 0;
                            cfs_files[i].parent_idx = 255;
                            _15(dir[i].type != 0) {
                                cfs_files[i].exists = 1;
                                cfs_files[i].is_folder = (dir[i].type == 2) ? 1 : 0;
                                cfs_files[i].size = dir[i].file_size;
                                cfs_files[i].start_lba = dir[i].start_lba;
                                _39(int n=0; n<11; n++) cfs_files[i].name[n] = dir[i].filename[n];
                                cfs_files[i].name[11] = 0;
                            }
                        }
                        need_ui_refresh = _86;
                    }
#endif
