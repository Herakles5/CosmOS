#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern "C" void* meinos_CreateWindowExA(uint32_t, const char*, const char*, uint32_t, int, int, int, int, void*, void*, void*, void*) {
    return (void*)16;
}

extern "C" void* meinos_CreateDialogIndirectParamW(void* hInstance, const void* lpTemplate, void* hWndParent, void* lpDialogFunc, uint64_t dwInitParam) {
    if (!lpTemplate) return 0;
    uint16_t* ptr = (uint16_t*)lpTemplate;
    bool is_ex = (ptr[1] == 0xFFFF);
    
    uint32_t style;
    uint32_t exStyle;
    uint16_t num_items;
    short x, y, w, h;
    
    if (is_ex) {
        exStyle = *(uint32_t*)(ptr + 4);
        style = *(uint32_t*)(ptr + 6);
        num_items = ptr[8];
        x = ptr[9];
        y = ptr[10];
        w = ptr[11];
        h = ptr[12];
        ptr += 13;
    } else {
        style = *(uint32_t*)(ptr);
        exStyle = *(uint32_t*)(ptr + 2);
        num_items = ptr[4];
        x = ptr[5];
        y = ptr[6];
        w = ptr[7];
        h = ptr[8];
        ptr += 9;
    }
    
    // Skip menu
    if (*ptr == 0x0000) ptr++;
    else if (*ptr == 0xFFFF) ptr += 2;
    else { while (*ptr) ptr++; ptr++; } // string
    
    // Skip class
    if (*ptr == 0x0000) ptr++;
    else if (*ptr == 0xFFFF) ptr += 2;
    else { while (*ptr) ptr++; ptr++; } // string
    
    // Read title
    char title[256] = {0};
    int t_i = 0;
    while (*ptr) {
        if (t_i < 255) title[t_i++] = (char)*ptr;
        ptr++;
    }
    ptr++; // null terminator
    
    if (style & 0x40) { // DS_SETFONT
        uint16_t pointSize = *ptr++;
        if (is_ex) {
            ptr++; // weight
            ptr++; // italic/charset
        }
        while (*ptr) ptr++; ptr++; // typeface
    }
    
    void* hwnd = meinos_CreateWindowExA(exStyle, "Dialog", title, style, x, y, w, h, hWndParent, 0, hInstance, 0);
    if (!hwnd) return 0;
    
    for (int i = 0; i < num_items; i++) {
        ptr = (uint16_t*)(((uintptr_t)ptr + 3) & ~3); // DWORD align
        
        short ix, iy, iw, ih;
        uint16_t id;
        uint32_t istyle;
        if (is_ex) {
            istyle = *(uint32_t*)(ptr + 4);
            ix = ptr[6]; iy = ptr[7]; iw = ptr[8]; ih = ptr[9];
            id = *(uint32_t*)(ptr + 10);
            ptr += 12;
        } else {
            istyle = *(uint32_t*)(ptr);
            ix = ptr[2]; iy = ptr[3]; iw = ptr[4]; ih = ptr[5];
            id = ptr[6];
            ptr += 9;
        }
        
        char class_name[32] = {0};
        if (*ptr == 0xFFFF) {
            ptr++;
            switch (*ptr) {
                case 0x0080: strncpy(class_name, "Button", 31); break;
                case 0x0081: strncpy(class_name, "Edit", 31); break;
                case 0x0082: strncpy(class_name, "Static", 31); break;
                case 0x0083: strncpy(class_name, "ListBox", 31); break;
                case 0x0084: strncpy(class_name, "ScrollBar", 31); break;
                case 0x0085: strncpy(class_name, "ComboBox", 31); break;
            }
            ptr++;
        } else {
            int ci = 0;
            while (*ptr) {
                if (ci < 31) class_name[ci++] = (char)*ptr;
                ptr++;
            }
            ptr++;
        }
        
        char item_title[256] = {0};
        if (*ptr == 0xFFFF) {
            ptr += 2;
        } else {
            int ti = 0;
            while (*ptr) {
                if (ti < 255) item_title[ti++] = (char)*ptr;
                ptr++;
            }
            ptr++;
        }
        
        uint16_t extra = *ptr++;
        if (extra) {
            ptr = (uint16_t*)((uint8_t*)ptr + extra - 2);
        }
        
        meinos_CreateWindowExA(0, class_name, item_title, istyle | 0x40000000, ix, iy, iw, ih, hwnd, (void*)(uint64_t)id, hInstance, 0);
    }
    
    return hwnd;
}

int main() { return 0; }
