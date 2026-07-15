import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# Update drawing logic
draw_code = """                } else if (c->type == 4) { // COMBOBOX
                    int box_h = 20; // Fixed height for the closed combobox
                    DrawRoundedRect(cx, cy, cw, box_h, 0, 0x222222);
                    DrawRoundedRect(cx+1, cy+1, cw-2, box_h-2, 0, 0x555555);
                    
                    const char* display_text = c->text;
                    if (c->num_items > 0 && c->selected_item >= 0 && c->selected_item < c->num_items) {
                        display_text = c->items[c->selected_item];
                    }
                    Text(cx + 5, cy + (box_h - 16)/2, display_text, 0xFFFFFF, _128);
                    
                    // Dropdown arrow
                    Text(cx + cw - 15, cy + (box_h - 16)/2, "v", 0xFFFFFF, _128);
                    
                    if (c->is_dropped) {
                        int drop_h = c->num_items * 20;
                        DrawRoundedRect(cx, cy + box_h, cw, drop_h, 0, 0x111111);
                        DrawRoundedRect(cx+1, cy + box_h + 1, cw-2, drop_h-2, 0, 0x444444);
                        for (int j = 0; j < c->num_items; j++) {
                            int item_y = cy + box_h + (j * 20);
                            Text(cx + 5, item_y + 2, c->items[j], 0xAAAAAA, _128);
                        }
                    }
                }"""
content = re.sub(r'                \} else if \(c->type == 4\) \{ // COMBOBOX.*?Text\(cx \+ cw - 15, cy \+ \(ch - 16\)/2, "v", 0xFFFFFF, _128\);\n                \}', draw_code, content, flags=re.DOTALL)

# Update click logic
click_logic = """                               if (c->type == 1 || c->type == 4) { // BUTTON or COMBOBOX
                                   int cx = wx + c->x;
                                   int cy = wy + title_bar + c->y;
                                   
                                   if (c->type == 4) {
                                       int box_h = 20;
                                       if (c->is_dropped) {
                                           int drop_h = c->num_items * 20;
                                           if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy + box_h && mouse_y <= cy + box_h + drop_h) {
                                               int clicked_item = (mouse_y - (cy + box_h)) / 20;
                                               if (clicked_item >= 0 && clicked_item < c->num_items) {
                                                   c->selected_item = clicked_item;
                                               }
                                               c->is_dropped = 0;
                                               input_cooldown = 20;
                                               // Send CBN_SELCHANGE
                                               if (win->wndproc) {
                                                   WndProc_t wndproc = (WndProc_t)win->wndproc;
                                                   uint64_t msg = 0x0111; // WM_COMMAND
                                                   uint64_t wparam = (uint64_t)c->id | (1ULL << 16); // 1 = CBN_SELCHANGE
                                                   uint64_t lparam = 0;
                                                   wndproc((void*)(uint64_t)win_z[i], msg, wparam, lparam);
                                               }
                                               continue;
                                           }
                                       }
                                       if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy && mouse_y <= cy + box_h) {
                                           c->is_dropped = !c->is_dropped;
                                           input_cooldown = 20;
                                           continue;
                                       }
                                   } else if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy && mouse_y <= cy + c->h) {
                                       if (win->wndproc) {
                                           WndProc_t wndproc = (WndProc_t)win->wndproc;
                                           uint64_t msg = 0x0111; // WM_COMMAND
                                           uint64_t wparam = (uint64_t)c->id;
                                           uint64_t lparam = 0; // HWND
                                           wndproc((void*)(uint64_t)win_z[i], msg, wparam, lparam);
                                       }
                                   }
                               }"""
content = re.sub(r'                               if \(c->type == 1 \|\| c->type == 4\) \{ // BUTTON or COMBOBOX.*?\}\n                               \}', click_logic, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
