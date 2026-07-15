                    /// NAVIGATION
                    _15(current_folder_id NEQ 255) {
                        Text(wx+15, wy+120, "TARGET:", 0xAAAAAA, _128);
                        Text(wx+80, wy+120, cfs_files[current_folder_id].name, 0xAAAAAA, _128);

                        DrawRoundedRect(wx+15, wy+140, 60, 20, 2, 0x444444); Text(wx+20, wy+145, "[ BACK ]", 0xFFFFFF, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+15, wy+140, 60, 20)) {
                            current_folder_id = 255; current_page_offset = 0; need_ui_refresh = _128; input_cooldown = 15; active_ntfs_folder_lba = 5;
                            if (is_fat32_drive) active_fat32_folder_lba = fat32_root_lba;
                        }
                    } _41 {
                        Text(wx+15, wy+120, "--- ROOT DIRECTORY ---", 0xFFFFFF, _128);
                    }
                    
                    /// PREV / NEXT BUTTONS FOR EVERYWHERE
                    DrawRoundedRect(wx+85, wy+140, 45, 20, 2, 0x444444); Text(wx+92, wy+145, "PREV", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+85, wy+140, 45, 20)) {
                        if (current_page_offset >= 10) current_page_offset -= 10;
                        need_ui_refresh = _128; input_cooldown = 15;
                    }
                    DrawRoundedRect(wx+135, wy+140, 45, 20, 2, 0x444444); Text(wx+142, wy+145, "NEXT", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+135, wy+140, 45, 20)) {
                        current_page_offset += 10;
                        need_ui_refresh = _128; input_cooldown = 15;
                    }
                    DrawRoundedRect(wx+185, wy+140, 45, 20, 2, 0x0055AA); Text(wx+194, wy+145, "REF", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+185, wy+140, 45, 20)) {
                        need_ui_refresh = _128; input_cooldown = 15;
                    }
