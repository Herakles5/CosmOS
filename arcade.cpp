#include "arcade.h"
#include "schneider_lang.h"
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#endif
extern uint32_t screen_w;
extern uint32_t screen_h;
extern "C" _50 apply_static_ip(_71 _30* new_ip);
extern "C" _50 send_dns_query(_71 _30* domain);
extern _89 ip_str_to_u32(_71 _30* ip_str);
extern _50 send_tcp_syn(_89 ip, _182 port);
extern _50 Put(_43 x, _43 y, _89 col);
extern _50 DrawGlassRect(_43 x, _43 y, _43 rw, _43 rh, _43 r, _89 c);
extern "C" void open_browser_save_as();
/// BARE METAL FIX: Die exakten Kernel-Signaturen!
extern _44 ahci_read_sectors(_43 port_no, _43 lba, _43 count, _94 buffer_addr);
extern _44 ahci_write_sectors(_43 port_no, _43 lba, _43 count, _94 buffer_addr);
// Dem Compiler die Funktion bekannt machen (Global!)
extern "C" void kernel_save_download(unsigned long long source_ram, int file_size, const char* name);

/// Deine CFS (Cosmos File System) Struktur aus dem Kernel
struct CFS_DIR_ENTRY {
    uint8_t  type;        /// 0 = Leer, 1 = Datei, 2 = Ordner
    char     filename[11];
    uint32_t file_size;
    uint32_t start_lba;
} __attribute__((packed));

extern "C" void kernel_save_browser_download();
extern "C" unsigned char* decode_image(unsigned char* buffer, int len, int* w, int* h, int* comp, int req_comp);
extern "C" void free_image(unsigned char* ptr);
_184* browser_stb_img = 0;
_43 browser_stb_w = 0;
_43 browser_stb_h = 0;
_43 browser_stb_tried = 0;
_44 auto_img_state = 0; /// 0=Aus, 1=Triggered, 2=Lädt runter, 3=Fertig & Inline
_30 image_download_url[128] = {0}; // Separater Speicher für Bild-Downloads

_184 browser_download_buffer[3000000];
_43 browser_download_len = 0;
_43 browser_content_length = 0;
extern _30 browser_content[65536];
_44 browser_download_complete = 0;

/// ========================================================
extern _30 input_values[16][128];

extern "C" _50 parse_html() {
    _39(_43 i = 0; i < 16; i++) input_values[i][0] = 0;
    _43 start_idx = 0;
    /// Find HTTP headers end
    _39(_43 i = 0; i < browser_download_len - 1; i++) {
        _15(i < browser_download_len - 3 AND 
            browser_download_buffer[i] EQ '\r' AND browser_download_buffer[i+1] EQ '\n' AND 
            browser_download_buffer[i+2] EQ '\r' AND browser_download_buffer[i+3] EQ '\n') {
            start_idx = i + 4;
            _37;
        }
        _15(browser_download_buffer[i] EQ '\n' AND browser_download_buffer[i+1] EQ '\n') {
            start_idx = i + 2;
            _37;
        }
    }
    
    _43 b_idx = 0;
    _44 in_tag = _86;
    _44 in_script = _86;
    _44 in_style = _86;
    
    _39(_43 i = start_idx; i < browser_download_len; i++) {
        _184 c = browser_download_buffer[i];
        
        _15(c EQ '<') {
            _184 t1 = (i+1 < browser_download_len) ? (browser_download_buffer[i+1] | 32) : 0;
            _184 t2 = (i+2 < browser_download_len) ? (browser_download_buffer[i+2] | 32) : 0;
            _184 t3 = (i+3 < browser_download_len) ? (browser_download_buffer[i+3] | 32) : 0;
            _184 t4 = (i+4 < browser_download_len) ? (browser_download_buffer[i+4] | 32) : 0;
            _184 t5 = (i+5 < browser_download_len) ? (browser_download_buffer[i+5] | 32) : 0;
            
            _44 keep_tag = _86;
            _15(t1 EQ 'a' AND (t2 EQ ' ' OR t2 EQ '>')) keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'a' AND (t3 EQ ' ' OR t3 EQ '>')) keep_tag = _128;
            _15(t1 EQ 'f' AND t2 EQ 'o' AND t3 EQ 'r' AND t4 EQ 'm') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'f' AND t3 EQ 'o' AND t4 EQ 'r' AND t5 EQ 'm') keep_tag = _128;
            _15(t1 EQ 'i' AND t2 EQ 'n' AND t3 EQ 'p' AND t4 EQ 'u' AND t5 EQ 't') keep_tag = _128;
            _15(t1 EQ 'b' AND t2 EQ 'r') keep_tag = _128;
            _15(t1 EQ 'p' AND (t2 EQ ' ' OR t2 EQ '>')) keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'p' AND (t3 EQ ' ' OR t3 EQ '>')) keep_tag = _128;
            _15(t1 EQ 'h' AND (t2 EQ '1' OR t2 EQ '2')) keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'h' AND (t3 EQ '1' OR t3 EQ '2')) keep_tag = _128;
            _15(t1 EQ 'd' AND t2 EQ 'i' AND t3 EQ 'v') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'd' AND t3 EQ 'i' AND t4 EQ 'v') keep_tag = _128;
            _15(t1 EQ 'l' AND t2 EQ 'i') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'l' AND t3 EQ 'i') keep_tag = _128;
            _15(t1 EQ 'u' AND t2 EQ 'l') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'u' AND t3 EQ 'l') keep_tag = _128;
            _15(t1 EQ 'i' AND t2 EQ 'm' AND t3 EQ 'g') keep_tag = _128;
            _15(t1 EQ 't' AND t2 EQ 'r') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 't' AND t3 EQ 'r') keep_tag = _128;
            _15(t1 EQ 't' AND t2 EQ 'a' AND t3 EQ 'b' AND t4 EQ 'l' AND t5 EQ 'e') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 't' AND t3 EQ 'a' AND t4 EQ 'b' AND t5 EQ 'l') keep_tag = _128;
            _15(t1 EQ 'p' AND t2 EQ 'r' AND t3 EQ 'e') keep_tag = _128;
            _15(t1 EQ '/' AND t2 EQ 'p' AND t3 EQ 'r' AND t4 EQ 'e') keep_tag = _128;


            _15(keep_tag AND !in_script AND !in_style) {
                browser_content[b_idx++] = '<';
                continue; 
            }
            
            in_tag = _128;
            
            _15(i + 3 < browser_download_len) {
                _15(t1 EQ 's' AND t2 EQ 'c' AND t3 EQ 'r' AND t4 EQ 'i') in_script = _128;
                _15(t1 EQ 's' AND t2 EQ 't' AND t3 EQ 'y' AND t4 EQ 'l') in_style = _128;
                _15(t1 EQ '/' AND t2 EQ 's' AND t3 EQ 'c' AND t4 EQ 'r') in_script = _86;
                _15(t1 EQ '/' AND t2 EQ 's' AND t3 EQ 't' AND t4 EQ 'y') in_style = _86;
            }
            continue;
        }
        _15(c EQ '>') {
            _15(in_tag) {
                in_tag = _86;
                continue;
            }
        }
        
        _15(!in_tag AND !in_script AND !in_style) {
            _15(c EQ '&') {
                _44 found_semi = _86;
                _39(_43 j = 1; j < 8; j++) {
                    _15(i + j < browser_download_len AND browser_download_buffer[i+j] EQ ';') {
                        i += j;
                        found_semi = _128;
                        browser_content[b_idx++] = ' ';
                        _37;
                    }
                }
                _15(!found_semi) browser_content[b_idx++] = c;
            } _41 {
                _15(c >= 32 OR c EQ '\n' OR c EQ '\r' OR c EQ '\t') {
                    _15(c EQ '\r') continue;
                    _15(c EQ '\t') c = ' ';
                    
                    _15(c EQ ' ' AND b_idx > 0 AND browser_content[b_idx-1] EQ ' ') continue;
                    _15(c EQ '\n' AND b_idx > 0 AND browser_content[b_idx-1] EQ '\n') {
                        _15(b_idx > 1 AND browser_content[b_idx-2] EQ '\n') continue;
                    }
                    
                    browser_content[b_idx++] = c;
                    _15(b_idx >= 65530) _37;
                }
            }
        }
    }
    browser_content[b_idx] = 0;
}

/// BARE METAL STRING HELPER
/// ========================================================
bool str_starts(const char* str, const char* prefix) {
    while (*prefix != 0) {
        if (*str == 0 || *str != *prefix) {
            return false; /// Passt nicht!
        }
        str++;
        prefix++;
    }
    return true; /// Treffer! Der Text fängt genau so an.
}
/// ==========================================
/// APP ID 8: DIE 2-in-1 WEB & NETWORK ENGINE
/// ==========================================
_43 browser_view = 0; /// 0 = Web Browser, 1 = LAN Configurator
_43 browser_state = 0;
_30 browser_url[512] = "home"; 
_30 browser_last_url[512] = "home";
_43 browser_url_idx = 4;
_30 browser_content[65536] = "<br><br><br><h1>       MEIN-OS HTML6 BROWSER</h1><br><p>             Welcome to the Next Generation Web.</p><br><br><form action=\"http://wiby.me/\"><input name=\"q\"><input type=\"submit\" value=\"SEARCH\"></form><br><br><br><p>         <a href=\"http://wiby.me/\">Powered by Wiby Search</a>&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"http://www.google.com/\">Google</a></p>";
/// Variablen für die Config-Ansicht
_43 netcfg_state = 0;
_30 netcfg_ip[64] = "192.168.14.100"; /// <--- DAS ist die IP für deinen Laptop!
_43 netcfg_ip_idx = 14;
_30 netcfg_content[2048] = "LAN PARTY MODE: STATIC IP CONFIGURATION\n\nENTER DESIRED IP AND PRESS APP TO SET YOUR MACHINE IP.";
/// Netzwerk-Variablen aus dem Kernel
extern _30 ip_address[32]; 
extern _30 gateway_ip[32];
extern _184 router_mac[6];
extern _30 mac_str[24];
//_172 _30 cmd_status[256];
/// BARE METAL FIX: Fehlende Brücken für den Browser!
_172 _50 DrawChar(_43 x, _43 y, _30 c, _89 col, _44 bold);
_172 _50 str_cpy(_30* d, _71 _30* s);
_172 _50 str_cat(_30* dest, _71 _30* src);
_172 _43 str_len(_71 _30* s);
_172 volatile char last_app_key;

/// ==========================================
/// BARE METAL FIX: EXTERNE BRÜCKEN ZUM KERNEL
/// ==========================================
_172 _43 mouse_x;
_172 _43 mouse_y;
_172 _44 mouse_just_pressed;
_172 _44 mouse_down;
_172 _44 key_new;
_172 _184 key_scancode;

_172 _89 frame;
_172 _43 input_cooldown;
_172 _44 blocked;
_172 _43 os2_win_z[20];
#define win_z os2_win_z

/// Grafik & Sound (Aus kernel_main.cpp)
_172 _50 TextC(_43 cp, _43 y, _71 _30* s, _89 col, _44 bold);
_172 _50 Text(_43 x, _43 y, _71 _30* s, _89 col, _44 bold);
_172 _50 DrawRoundedRect(_43 x, _43 y, _43 rw, _43 rh, _43 r, _89 c);
_172 _50 DrawRoundWindow(_43 cx_p, _43 cy_p, _43 r, _89 color);
_172 _44 is_over_rect(_43 mx, _43 my, _43 x, _43 y, _43 w, _43 h);
_172 _50 int_to_str(_43 n, _30* s);
_172 _50 play_sound(_89 n_freq, _43 duration);
_172 _50 play_freq(_89 f);
/// Emulator
/// ==========================================
/// DIE CHIP-8 VIRTUAL MACHINE (UNIVERSAL EMULATOR)
/// ==========================================
// 1. Deklarationen vorziehen, damit der Compiler sie kennt
void c8_step();
extern "C" void c8_reset();
void c8_render(_43 x, _43 y, _43 s) { /* Rendering Logik hier */ }
// 2. VM Interface Struktur
struct VM_INTERFACE {
    void (*reset)();
    void (*step)();
    void (*render)(_43 x, _43 y, _43 scale);
    _30 name[32];
};
VM_INTERFACE chip8_vm = { c8_reset, c8_step, c8_render, "CHIP-8" };
VM_INTERFACE* current_vm = &chip8_vm;
/// ==========================================
/// DIE CHIP-8 VIRTUAL MACHINE (UNIVERSAL EMULATOR)
/// ==========================================
// A. EXTERNS (Damit der Compiler weiß, dass diese Funktionen existieren)
extern _50 sys_print(const char* text);
extern bool disk_read_auto(uint32_t lba, uint64_t target_ram_addr);

// C. GLOBALE VARIABLEN (Hier gehören c8_ram, c8_pc, etc. hin!)
unsigned char c8_ram[4096];
unsigned char c8_v[16];
unsigned short c8_i, c8_pc;
unsigned short c8_stack[16];
unsigned char c8_sp, c8_dt, c8_st;
unsigned char c8_gfx[64 * 32];
unsigned char c8_key[16];
_44 c8_draw;
_43 c8_state = 0;
_89 c8_rand_seed = 12345;         /// Eigener Zufallsgenerator für die VM
// In arcade.cpp muss das exakt so stehen:
extern "C" void c8_reset() { 
	for (int i=0; i<4096; i++) c8_ram[i] = 0;
	for (int i=0; i<16; i++) { c8_v[i] = 0; c8_key[i] = 0; c8_stack[i] = 0; }
	for (int i=0; i<64*32; i++) c8_gfx[i] = 0;
	c8_pc = 0x200; c8_i = 0; c8_sp = 0; c8_dt = 0; c8_st = 0;
}

/// Die CHIP-8 Schriftart (0-F) fest in der Hardware verankert
unsigned char c8_font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/// Ein hartcodiertes Test-Programm: Das originale IBM Logo aus den 70ern!
unsigned char c8_rom_ibm[] = {
    0x00, 0xE0, 0xA2, 0x2A, 0x60, 0x0C, 0x61, 0x08, 0xD0, 0x1F, 0x70, 0x09, 0xA2, 0x39, 0xD0, 0x1F, 
    0xA2, 0x48, 0x70, 0x08, 0xD0, 0x1F, 0x70, 0x04, 0xA2, 0x57, 0xD0, 0x1F, 0x70, 0x08, 0xA2, 0x66, 
    0xD0, 0x1F, 0x70, 0x08, 0xA2, 0x75, 0xD0, 0x1F, 0x12, 0x28, 0xFF, 0x00, 0xFF, 0x00, 0x3C, 0x00, 
    0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x38, 0x00, 0x3F, 
    0x00, 0x3F, 0x00, 0x38, 0x00, 0xFF, 0x00, 0xFF, 0x80, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 
    0x80, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0xF8, 0x00, 0xFC, 0x00, 0x3E, 0x00, 0x3F, 0x00, 0x3B, 
    0x00, 0x39, 0x00, 0xF8, 0x00, 0xF8, 0x03, 0x00, 0x07, 0x00, 0x0F, 0x00, 0xBF, 0x00, 0xFB, 0x00, 
    0xF3, 0x00, 0xE3, 0x00, 0x43, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0xE0, 0x00, 0xE0, 
    0x00, 0x80
};

/// ==========================================
/// VM CPU CORE: Führt genau EINEN Befehl aus!
/// ==========================================
void c8_step() {
    /// 1. Fetch (Hole 2 Bytes aus dem Speicher)
    unsigned short opcode = (c8_ram[c8_pc] << 8) | c8_ram[c8_pc + 1];
    c8_pc += 2;
    
    /// 2. Decode (Zerlege den Befehl)
    unsigned short x = (opcode & 0x0F00) >> 8;
    unsigned short y = (opcode & 0x00F0) >> 4;
    unsigned short nnn = opcode & 0x0FFF;
    unsigned char nn = opcode & 0x00FF;
    unsigned char n = opcode & 0x000F;

    /// 3. Execute (Maschinensprache übersetzen)
    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) { /// Clear Screen
                for (int i = 0; i < 64 * 32; i++) c8_gfx[i] = 0;
                c8_draw = 1;
            } else if (opcode == 0x00EE) { /// Return from subroutine
                c8_pc = c8_stack[--c8_sp];
            }
            break;
        case 0x1000: c8_pc = nnn; break; /// JMP
        case 0x2000: c8_stack[c8_sp++] = c8_pc; c8_pc = nnn; break; /// CALL
        case 0x3000: if (c8_v[x] == nn) c8_pc += 2; break;
        case 0x4000: if (c8_v[x] != nn) c8_pc += 2; break;
        case 0x5000: if (c8_v[x] == c8_v[y]) c8_pc += 2; break;
        case 0x6000: c8_v[x] = nn; break; /// SET
        case 0x7000: c8_v[x] += nn; break; /// ADD
        case 0x8000:
            switch (n) {
                case 0: c8_v[x] = c8_v[y]; break;
                case 1: c8_v[x] |= c8_v[y]; break;
                case 2: c8_v[x] &= c8_v[y]; break;
                case 3: c8_v[x] ^= c8_v[y]; break;
                case 4: { unsigned short sum = c8_v[x] + c8_v[y]; c8_v[0xF] = (sum > 255) ? 1 : 0; c8_v[x] = sum & 0xFF; } break;
                case 5: c8_v[0xF] = (c8_v[x] > c8_v[y]) ? 1 : 0; c8_v[x] -= c8_v[y]; break;
                case 6: c8_v[0xF] = c8_v[x] & 0x1; c8_v[x] >>= 1; break;
                case 7: c8_v[0xF] = (c8_v[y] > c8_v[x]) ? 1 : 0; c8_v[x] = c8_v[y] - c8_v[x]; break;
                case 0xE: c8_v[0xF] = (c8_v[x] & 0x80) ? 1 : 0; c8_v[x] <<= 1; break;
            }
            break;
        case 0x9000: if (c8_v[x] != c8_v[y]) c8_pc += 2; break;
        case 0xA000: c8_i = nnn; break; /// Set Index
        case 0xB000: c8_pc = nnn + c8_v[0]; break;
        case 0xC000: /// Random (Eigene Formel, da rand() bare-metal fehlt)
            c8_rand_seed = c8_rand_seed * 1664525 + 1013904223;
            c8_v[x] = ((c8_rand_seed >> 16) & 0xFF) & nn;
            break;
        case 0xD000: { /// Draw Sprite an Position (x,y)
            unsigned char px = c8_v[x] % 64;
            unsigned char py = c8_v[y] % 32;
            c8_v[0xF] = 0;
            for (int row = 0; row < n; row++) {
                unsigned char sprite = c8_ram[c8_i + row];
                for (int col = 0; col < 8; col++) {
                    if ((sprite & (0x80 >> col)) != 0) {
                        int index = ((py + row) % 32) * 64 + ((px + col) % 64);
                        if (c8_gfx[index] == 1) c8_v[0xF] = 1; /// Kollision!
                        c8_gfx[index] ^= 1;
                    }
                }
            }
            c8_draw = 1;
        } break;
        case 0xE000:
            if (nn == 0x9E) { if (c8_key[c8_v[x]]) c8_pc += 2; }
            else if (nn == 0xA1) { if (!c8_key[c8_v[x]]) c8_pc += 2; }
            break;
        case 0xF000:
            switch (nn) {
                case 0x07: c8_v[x] = c8_dt; break;
                case 0x0A: { /// Wait for Key
                    bool pressed = false;
                    for(int k=0; k<16; k++) if(c8_key[k]) { c8_v[x] = k; pressed = true; break; }
                    if(!pressed) c8_pc -= 2; /// Bleibe hier, bis Taste gedrückt
                } break;
                case 0x15: c8_dt = c8_v[x]; break;
                case 0x18: c8_st = c8_v[x]; break;
                case 0x1E: c8_i += c8_v[x]; break;
                case 0x29: c8_i = c8_v[x] * 5 + 0x50; break; /// Lade Font
                
                /// ==========================================
                /// BARE METAL FIX: MEMORY PROTECTION!
                /// Schützt den Kernel vor HTML-Müll oder Bugs
                /// ==========================================
                case 0x33: /// BCD Umwandlung
                    if (c8_i < 4094) { // Darf nicht über 4095 hinaus schreiben!
                        c8_ram[c8_i] = c8_v[x] / 100;
                        c8_ram[c8_i + 1] = (c8_v[x] / 10) % 10;
                        c8_ram[c8_i + 2] = c8_v[x] % 10;
                    }
                    break;
                case 0x55: 
                    for (int i = 0; i <= x; i++) {
                        // Die Bitmaske & 0x0FFF zwingt den Index zwischen 0 und 4095
                        c8_ram[(c8_i + i) & 0x0FFF] = c8_v[i]; 
                    }
                    break;
                case 0x65: 
                    for (int i = 0; i <= x; i++) {
                        c8_v[i] = c8_ram[(c8_i + i) & 0x0FFF]; 
                    }
                    break;
            }
            break;
    }
}
_44 load_rom_from_disk(const char* filename, _89 target_addr) {
    sys_print("SCANNING DISK FOR ROM...");
    // 1. Suche die Datei im Dateisystem (Wir nehmen mal LBA 1002 als Root-Dir an)
    uint32_t buf_dir = 0x00901000; 
    disk_read_auto(1002, buf_dir); 
    CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
    _43 found_lba = 0;
    _43 found_size = 0;
    // 2. Dateisystem durchsuchen nach *.BIN
    for (int i = 0; i < 8; i++) {
        if (dir[i].type == 1) { // Typ 1 = Datei
            _44 is_bin = _86; // Standard: false
            
            // Wir suchen den Punkt '.' in den ersten 8 Zeichen
            for(int k=0; k<8; k++) {
                if(dir[i].filename[k] == '.') {
                    // Prüfen, ob nach dem Punkt B-I-N kommt
                    if ((dir[i].filename[k+1] == 'B' || dir[i].filename[k+1] == 'b') &&
                        (dir[i].filename[k+2] == 'I' || dir[i].filename[k+2] == 'i') &&
                        (dir[i].filename[k+3] == 'N' || dir[i].filename[k+3] == 'n')) {
                        is_bin = _128; // Treffer!
                    }
                    break; // Punkt gefunden, Schleife für diesen Namen beenden
                }
            }

            if (is_bin) {
                // TREFFER! Wir haben eine .BIN Datei gefunden!
                found_lba = dir[i].start_lba;
                found_size = dir[i].file_size;
                break; // Nimm die erste gefundene .BIN Datei
            }
        }
    }
    if (found_lba == 0) {
        sys_print("ERROR: ROM FILE NOT FOUND!");
        return _86;
    }
    // 3. ROM Sektorweise laden
    sys_print("LOADING ROM DATA...");
    
    /// BARE METAL FIX: Verhindere RAM-Overflow!
    /// Ein CHIP-8 ROM darf maximal 3584 Bytes groß sein (4096 RAM - 512 Offset)
    if (found_size > 3584) {
        found_size = 3584; // Knallhart abschneiden!
        sys_print("WARNING: ROM TOO LARGE, TRUNCATED!");
    }
    
    _43 sectors = (found_size + 511) / 512;
    for (_43 s = 0; s < sectors; s++) {
        disk_read_auto(found_lba + s, target_addr + (s * 512));
    }
    sys_print("ROM LOADED SUCCESSFULLY.");
    return _128;
}

/// ==========================================
/// SPIEL-STATUS-VARIABLEN (Isoliert im Arcade-Modul!)
/// ==========================================
/// --- PONG ---
_43 pong_state = 0; 
_43 pong_mode = 0; 
_43 pong_diff = 1; 
_43 pong_p1_y = 100, pong_p2_y = 100;
_43 pong_ball_x = 200, pong_ball_y = 150;
_43 pong_vel_x = 5, pong_vel_y = 3;
_43 pong_score1 = 0, pong_score2 = 0;

/// --- BLOBBY VOLLEY ---
_43 bv_state = 0, bv_mode = 0, bv_diff = 1;
_43 bv_p1_x = 50, bv_p1_y = 200, bv_p1_vy = 0;
_43 bv_p2_x = 350, bv_p2_y = 200, bv_p2_vy = 0;
_43 bv_ball_x = 100, bv_ball_y = 50, bv_ball_vx = 0, bv_ball_vy = 0;
_43 bv_score1 = 0, bv_score2 = 0;
_43 bv_touches = 0, bv_last_touch = 0;

/// --- COSMOS DEFENDER ---
_43 hs_state = 0; // 0 = radar, 1 = settings
_43 hs_phase = 0; // 0-4 Celestial Bow Phase
_43 hs_radar_count = 0;
_43 hs_selected_target = -1;
_43 hs_paused = 0;
_43 hs_auto_suppression = 0;
_30 hs_spectator_input[64] = {0};
_43 hs_spectator_idx = 0;
_43 hs_crawlers_active = 3;
_30 hs_log[10][64]; // Omni-Crawler Log
_43 hs_log_lines = 0;
struct DetectedIP {
    _184 ip[4];
    _30 threat[16];
    _43 hits;
    _44 isFriend;
    _30 proto[8];
    _30 country[4];
};
DetectedIP hs_radar[100];

void hs_add_log(const char* msg) {
    if (hs_log_lines >= 10) {
        for(int i = 1; i < 10; i++) {
            str_cpy(hs_log[i-1], hs_log[i]);
        }
        hs_log_lines = 9;
    }
    str_cpy(hs_log[hs_log_lines], (char*)msg);
    hs_log_lines++;
}

#ifdef __linux__
std::mutex hs_data_mutex;

void FetchGeoInfo_Linux(std::string ip_str) {
    if (ip_str.find("127.") == 0 || ip_str.find("192.168.") == 0) return;
    
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return;
    
    struct hostent* h = gethostbyname("ip-api.com");
    if (!h) { close(s); return; }
    
    struct sockaddr_in sa;
    sa.sin_port = htons(80);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = *((unsigned long*)h->h_addr_list[0]);
    
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) { close(s); return; }
    
    std::string req = std::string("GET /json/") + ip_str + "?fields=countryCode,isp HTTP/1.1\r\nHost: ip-api.com\r\nConnection: close\r\n\r\n";
    send(s, req.c_str(), req.length(), 0);
    
    char buf[4096];
    std::string res = "";
    int bR;
    while ((bR = recv(s, buf, 4096, 0)) > 0) {
        res.append(buf, bR);
    }
    close(s);
    
    std::string country = "?";
    size_t pos = res.find("\"countryCode\":\"");
    if (pos != std::string::npos) {
        pos += 15;
        size_t end = res.find("\"", pos);
        if (end != std::string::npos) country = res.substr(pos, end - pos);
    }
    
    std::lock_guard<std::mutex> lock(hs_data_mutex);
    unsigned int ip1=0, ip2=0, ip3=0, ip4=0;
    sscanf(ip_str.c_str(), "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4);
    for(int i=0; i<hs_radar_count; i++) {
        if(hs_radar[i].ip[0] == ip1 && hs_radar[i].ip[1] == ip2 && 
           hs_radar[i].ip[2] == ip3 && hs_radar[i].ip[3] == ip4) {
            str_cpy(hs_radar[i].country, (char*)country.c_str());
            break;
        }
    }
}

void hs_add_to_radar(const char* ip_str, const char* proto) {
    std::lock_guard<std::mutex> lock(hs_data_mutex);

    unsigned int ip1=0, ip2=0, ip3=0, ip4=0;
    sscanf(ip_str, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4);

    for(int i=0; i<hs_radar_count; i++) {
        if(hs_radar[i].ip[0] == ip1 && hs_radar[i].ip[1] == ip2 &&
           hs_radar[i].ip[2] == ip3 && hs_radar[i].ip[3] == ip4) {
            hs_radar[i].hits++;
            str_cpy(hs_radar[i].proto, (char*)proto);
            if (hs_radar[i].isFriend == 0 && !str_starts(hs_radar[i].threat, "BLOCKED")) {
                if (hs_radar[i].hits > 20) {
                    str_cpy(hs_radar[i].threat, "CRITICAL");
                    if (hs_auto_suppression) {
                        char cmd[128];
                        sprintf(cmd, "iptables -A INPUT -s %u.%u.%u.%u -j DROP", ip1, ip2, ip3, ip4);
#ifdef __linux__
                        system(cmd);
#endif
                        str_cpy(hs_radar[i].threat, "BLOCKED");
                        hs_add_log("SYSTEM: AUTO-SUPPRESSED TARGET!");
                        hs_phase = 1; 
                    }
                } else {
                    str_cpy(hs_radar[i].threat, "SAFE");
                }
            }
            return;
        }
    }

    if (hs_radar_count < 100) {
        hs_radar[hs_radar_count].ip[0] = ip1;
        hs_radar[hs_radar_count].ip[1] = ip2;
        hs_radar[hs_radar_count].ip[2] = ip3;
        hs_radar[hs_radar_count].ip[3] = ip4;
        hs_radar[hs_radar_count].hits = 1;
        
        int isF = 0;
        if (ip1 == 8 && ip2 == 8) isF = 1;
        if (ip1 == 1 && ip2 == 1 && ip3 == 1 && ip4 == 1) isF = 1;
        if (ip1 == 192 && ip2 == 168) isF = 1;
        if (ip1 == 10) isF = 1;
        if (ip1 == 172 && ip2 >= 16 && ip2 <= 31) isF = 1;
        if (ip1 == 142 && ip2 == 250) isF = 1;
        if (ip1 == 142 && ip2 == 251) isF = 1;
        
        hs_radar[hs_radar_count].isFriend = isF;
        str_cpy(hs_radar[hs_radar_count].proto, (char*)proto);
        str_cpy(hs_radar[hs_radar_count].threat, "SAFE");
        str_cpy(hs_radar[hs_radar_count].country, "?");
        hs_radar_count++;   hs_radar_count++;
        
        char log_buf[128];
        str_cpy(log_buf, "TARGET ACQUIRED: ");
        str_cat(log_buf, (char*)ip_str);
        hs_add_log(log_buf);
        
        std::thread(FetchGeoInfo_Linux, std::string(ip_str)).detach();
    }
}

void hs_sniffer_thread() {
    hs_add_log("SYSTEM: OMNI-CRAWLER INITIALIZING...");
    
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sock < 0) {
        hs_add_log("ERROR: RAW SOCKET FAILED (RUN AS ROOT)");
        return;
    }
    hs_add_log("SYSTEM: RAW SOCKET ESTABLISHED.");
    
    char buffer[65536];
    while (true) {
        int data_size = recvfrom(sock, buffer, 65536, 0, NULL, NULL);
        if (data_size > 14) { 
            struct iphdr *iph = (struct iphdr*)(buffer + 14); 
            
            char sIP[32];
            sprintf(sIP, "%u.%u.%u.%u", 
                iph->saddr & 0xFF, (iph->saddr >> 8) & 0xFF, 
                (iph->saddr >> 16) & 0xFF, (iph->saddr >> 24) & 0xFF);
            
            if (sIP[0] != '1' || sIP[1] != '2' || sIP[2] != '7') { 
                if (!hs_paused) {
                    const char* proto = "OTH";
                    if (iph->protocol == IPPROTO_TCP) proto = "TCP";
                    else if (iph->protocol == IPPROTO_UDP) proto = "UDP";
                    else if (iph->protocol == IPPROTO_ICMP) proto = "ICMP";
                    
                    hs_add_to_radar(sIP, proto);
                }
            }
        }
    }
}

void hs_crawler_thread() {
#ifdef __linux__
    hs_add_log("SYSTEM: OMNI-CRAWLER STARTED...");
    while(1) {
        if (!hs_paused) {
            for(int i=1; i<255; i++) {
                char cmd[128];
                sprintf(cmd, "ping -c 1 -W 1 192.168.14.%d > /dev/null 2>&1", i);
                if (system(cmd) == 0) {
                    char ip_buf[32];
                    sprintf(ip_buf, "192.168.14.%d", i);
                    hs_add_to_radar(ip_buf, "ICMP");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
#endif
}

bool hs_thread_started = false;
extern "C" void hs_start_sniffer() {
    if (!hs_thread_started) {
        hs_thread_started = true;
        std::thread(hs_sniffer_thread).detach();
        std::thread(hs_crawler_thread).detach();
    }
}
#else
extern "C" void hs_start_sniffer() {}
#endif


/// ========================================================
/// BARE METAL TERMINAL (Der Text-Puffer)
/// ========================================================
char term_buffer[15][64];  /// Speicher für 15 Zeilen mit max 64 Zeichen
int term_line = 0;         /// Aktuelle Zeile
/// ========================================================
void clear_screen() {
    /// Wir leeren einfach unseren Speicher (nicht den Monitor!)
    for(int i = 0; i < 15; i++) term_buffer[i][0] = 0; 
    term_line = 0;
}

void sys_print(const char* text) {
    /// Wenn das Terminal voll ist (15 Zeilen erreicht) -> SCROLLEN!
    if (term_line >= 15) {
        for(int i = 1; i < 15; i++) {
            str_cpy(term_buffer[i-1], term_buffer[i]); /// Alles eins hoch schieben
        }
        term_line = 14; /// Unten Platz machen
    }
    
    /// Neuen Text in die aktuelle unterste Zeile kopieren
    str_cpy(term_buffer[term_line], (char*)text);
    term_line++;
}         
_172 _30 cmd_status[32];          

_50 run_cosmos_script(char* file_buffer, int file_size) {
    char current_line[128];
    int line_pos = 0;

    str_cpy(cmd_status, "RUNNING SCRIPT...");

    /// Gehe jedes Zeichen der geladenen Datei durch
    for (int i = 0; i < file_size; i++) {
        char c = file_buffer[i];

        /// Wenn die Zeile zu Ende ist (\n) oder das Dateiende erreicht ist
        if (c == '\n' || i == file_size - 1) {
            
            /// Letztes Zeichen noch mitnehmen, falls kein Umbruch am Ende war
            if (i == file_size - 1 && c != '\n' && c != '\r') {
                if (line_pos < 127) current_line[line_pos++] = c;
            }
            
            current_line[line_pos] = 0; /// String sauber abschließen

            /// ====================================================
            /// DER BEFEHLS-PARSER (Die komplette Cosmos Matrix!)
            /// ====================================================
            
            /// 1. BATCH-Style: ECHO (Text ausgeben)
            if (str_starts(current_line, "ECHO ")) {
                sys_print(&current_line[5]); 
            }
            /// 2. BATCH-Style: CLS (Bildschirm leeren)
            else if (str_starts(current_line, "CLS")) {
                clear_screen(); 
            }
            /// 3. OS-Befehl: TITLE
            else if (str_starts(current_line, "TITLE ")) {
                str_cpy(cmd_status, &current_line[6]);
            }
            /// 4. INFO: SYSINFO
            else if (str_starts(current_line, "SYSINFO")) {
                sys_print("OS   : COSMOS V2.0 64-BIT");
                sys_print("ARCH : X86_64 BARE METAL");
                sys_print("BOOT : LEGACY BIOS / PXE");
            }
            /// 5. INFO: MEM (RAM Status)
            else if (str_starts(current_line, "MEM")) {
                sys_print("RAM  : 8192 MB TOTAL");
                sys_print("USED : 14 MB (KERNEL)");
                sys_print("FREE : 8178 MB AVAILABLE");
            }
            /// 6. INFO: NET (Netzwerk Status)
            else if (str_starts(current_line, "NET")) {
                sys_print("ETH0 : INTEL PRO/1000 E1000");
                sys_print("MAC  : B8:AE:ED:00:11:22");
                sys_print("STAT : TX=1 RX=0 (WAITING)");
            }
            /// 7. INFO: HELP (Die Befehlsübersicht)
            else if (str_starts(current_line, "HELP")) {
                sys_print("AVAILABLE COMMANDS:");
                sys_print(" SYSINFO, MEM, NET, DIR, CLS");
                sys_print(" MKDIR [NAME] (CREATE FOLDER)");
                sys_print(" ECHO [TEXT]  (PRINT TEXT)");
            }
            /// 8. HARDWARE SATA: MKDIR
            else if (str_starts(current_line, "MKDIR ")) {
                sys_print("SPINNING UP SATA DRIVE...");
                uint32_t buf_dir = 0x00901000; 
                ahci_read_sectors(0, 1002, 4, buf_dir);
                for(volatile int w=0; w<1500000; w++) __asm__ volatile("nop");
                
                CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
                bool found = false;
                for (int i = 0; i < 8; i++) {
                    if (dir[i].type == 0) { 
                        dir[i].type = 2; 
                        dir[i].file_size = 0; dir[i].start_lba = 0;
                        for(int n=0; n<11; n++) dir[i].filename[n] = 0;
                        char* new_name = &current_line[6];
                        for(int n=0; n<10 && new_name[n] != 0 && new_name[n] != '\r'; n++) {
                            dir[i].filename[n] = new_name[n];
                        }
                        ahci_write_sectors(0, 1002, 4, buf_dir);
                        for(volatile int w=0; w<1500000; w++) __asm__ volatile("nop");
                        
                        char msg[64]; str_cpy(msg, "HDD WRITE OK: "); str_cat(msg, dir[i].filename);
                        sys_print(msg);
                        found = true; break;
                    }
                }
                if (!found) sys_print("ERROR: SATA ROOT DIR FULL!");
            }
            /// 9. HARDWARE SATA: DIR
            else if (str_starts(current_line, "DIR")) {
                sys_print("READING PHYSICAL SATA DRIVE...");
                uint32_t buf_dir = 0x00901000;
                ahci_read_sectors(0, 1002, 4, buf_dir);
                for(volatile int w=0; w<1500000; w++) __asm__ volatile("nop");
                
                sys_print("VOLUME: HARD DISK 0 (CFS V2)");
                sys_print("-------------------------");
                CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
                int count = 0;
                for (int i = 0; i < 8; i++) {
                    if (dir[i].type != 0) { 
                        char entry[64];
                        if (dir[i].type == 1) str_cpy(entry, " <FILE>  ");
                        else if (dir[i].type == 2) str_cpy(entry, " <DIR>   ");
                        else str_cpy(entry, " <?>     ");
                        str_cat(entry, dir[i].filename);
                        sys_print(entry);
                        count++;
                    }
                }
                if (count == 0) sys_print(" DRIVE IS EMPTY.");
                sys_print("-------------------------");
            }
            /// 10. UNBEKANNTER BEFEHL (Dein Fallback!)
            else if (current_line[0] != 0 && current_line[0] != '\r') {
                sys_print("UNKNOWN COMMAND OR BAD SYNTAX.");
            }
		}
	}
}
/// ==========================================
/// DIE PONG ENGINE (Ausgelagert aus win->id 11)
/// ==========================================
_50 run_pong_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _43 inner_h = wh - 40; /// (inner_w wurde hier gelöscht)
    _43 py = wy + 40;
    
    _15(pong_state EQ 0) {
        TextC(wx + ww/2, py + 20, "PONG OMNIVERSE", 0x00FF00, _128);
        /// --- MODUS AUSWAHL ---
        TextC(wx + ww/2, py + 60, "SELECT MODE", 0xFFFFFF, _128);
        DrawRoundedRect(wx + ww/2 - 110, py + 80, 100, 25, 3, pong_mode EQ 0 ? 0x00AA00 : 0x555555);
        TextC(wx + ww/2 - 60, py + 87, "1 PLAYER", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 110, py + 80, 100, 25)) pong_mode = 0;
        DrawRoundedRect(wx + ww/2 + 10, py + 80, 100, 25, 3, pong_mode EQ 1 ? 0x00AA00 : 0x555555);
        TextC(wx + ww/2 + 60, py + 87, "2 PLAYERS", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 + 10, py + 80, 100, 25)) pong_mode = 1;
        
        /// --- SCHWIERIGKEIT ---
        _15(pong_mode EQ 0) {
            TextC(wx + ww/2, py + 130, "CPU DIFFICULTY", 0xFFFFFF, _128);
            DrawRoundedRect(wx + ww/2 - 120, py + 150, 70, 25, 3, pong_diff EQ 0 ? 0x00AA00 : 0x555555);
            TextC(wx + ww/2 - 85, py + 157, "EASY", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 120, py + 150, 70, 25)) pong_diff = 0;
            DrawRoundedRect(wx + ww/2 - 35, py + 150, 70, 25, 3, pong_diff EQ 1 ? 0x00AA00 : 0x555555);
            TextC(wx + ww/2, py + 157, "NORMAL", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 35, py + 150, 70, 25)) pong_diff = 1;
            DrawRoundedRect(wx + ww/2 + 50, py + 150, 70, 25, 3, pong_diff EQ 2 ? 0x00AA00 : 0x555555);
            TextC(wx + ww/2 + 85, py + 157, "HARD", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 + 50, py + 150, 70, 25)) pong_diff = 2;
        } _41 {
            TextC(wx + ww/2, py + 150, "P1: MOUSE  |  P2: ARROWS (UP/DOWN)", 0xAAAAAA, _128);
        }
        
        /// --- START BUTTON ---
        DrawRoundedRect(wx + ww/2 - 60, py + 190, 120, 30, 5, 0xAA0000);
        TextC(wx + ww/2, py + 197, "START GAME", 0xFFFFFF, _128);
        _15(input_cooldown EQ 0 AND ((mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 60, py + 190, 120, 30)) OR (key_new AND (key_scancode EQ 0x1C OR key_scancode EQ 0x39)))) {
            pong_state = 1; pong_score1 = 0; pong_score2 = 0;
            pong_ball_x = ww/2; pong_ball_y = inner_h/2;
            pong_vel_x = 5; pong_vel_y = 3; 
            input_cooldown = 10;
            key_new = _86; /// Consume key
        }
    } _41 _15(pong_state EQ 2) {
        TextC(wx + ww/2, py + 80, pong_score1 >= 10 ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!", 0x00FF00, _128);
        TextC(wx + ww/2, py + 140, "CLICK TO RETURN TO MENU", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, wx, py, ww, inner_h)) {
            pong_state = 0; input_cooldown = 10;
        }
    } _41 {
        /// Spielfeld & Physik
        DrawRoundedRect(wx+10, py, ww-20, inner_h-10, 0, 0x000000);
        _39(_43 i=0; i<inner_h-10; i+=20) DrawRoundedRect(wx+ww/2-2, py+i, 4, 10, 0, 0x555555);
        _30 s1[10], s2[10]; int_to_str(pong_score1, s1); int_to_str(pong_score2, s2);
        Text(wx+ww/2-40, py+20, s1, 0x00FF00, _128);
        Text(wx+ww/2+30, py+20, s2, 0x00FF00, _128);
        Text(wx+20, py+15, "[ ESC ] TO ABORT", 0x444444, _86);
        _15(key_new AND key_scancode EQ 0x01) { pong_state = 0; }
        
        _15(win_z[19] EQ 11) {
            pong_ball_x += pong_vel_x;
            pong_ball_y += pong_vel_y;
            _15(pong_ball_y <= 5) { pong_ball_y = 5; pong_vel_y = -pong_vel_y; play_freq(800); }
            _41 _15(pong_ball_y >= inner_h-25) { pong_ball_y = inner_h-25; pong_vel_y = -pong_vel_y; play_freq(800); }
            _41 { play_freq(0); }
            
            pong_p1_y = mouse_y - py - 20; 
            _15(pong_p1_y < 0) pong_p1_y = 0; 
            _15(pong_p1_y > inner_h-50) pong_p1_y = inner_h-50;
            
            _15(pong_mode EQ 0) {
                _43 speed = 4; _15(pong_diff EQ 0) speed = 2; _15(pong_diff EQ 2) speed = 6; 
                _15(pong_ball_y > pong_p2_y + 20) pong_p2_y += speed;
                _15(pong_ball_y < pong_p2_y + 20) pong_p2_y -= speed;
            } _41 {
                _15(key_new) {
                    _15(key_scancode EQ 0x48) pong_p2_y -= 15;
                    _15(key_scancode EQ 0x50) pong_p2_y += 15; 
                }
            }
            
            _15(pong_p2_y < 0) pong_p2_y = 0; 
            _15(pong_p2_y > inner_h-50) pong_p2_y = inner_h-50;
            
            _15(pong_ball_x <= 30 AND pong_ball_y + 10 >= pong_p1_y AND pong_ball_y <= pong_p1_y + 40 AND pong_vel_x < 0) {
                pong_ball_x = 30; pong_vel_x = -pong_vel_x;
                _15(pong_vel_x < 14) pong_vel_x++;
                _15(pong_ball_y < pong_p1_y + 12) pong_vel_y -= 2;      
                _41 _15(pong_ball_y > pong_p1_y + 28) pong_vel_y += 2;  
                play_sound(1200, 2);
            }
            _15(pong_ball_x >= ww-40 AND pong_ball_y + 10 >= pong_p2_y AND pong_ball_y <= pong_p2_y + 40 AND pong_vel_x > 0) {
                pong_ball_x = ww-40; pong_vel_x = -pong_vel_x;
                _15(pong_vel_x > -14) pong_vel_x--;
                _15(pong_ball_y < pong_p2_y + 12) pong_vel_y -= 2; 
                _41 _15(pong_ball_y > pong_p2_y + 28) pong_vel_y += 2;
                play_sound(1200, 2);
            }
            
            _15(pong_ball_x < 0 OR pong_ball_x > 2000) { pong_score2++; pong_ball_x=ww/2; pong_ball_y=inner_h/2; play_sound(200, 20); }
            _15(pong_ball_x > ww) { pong_score1++; pong_ball_x=ww/2; pong_ball_y=inner_h/2; play_sound(200, 20); }
            _15(pong_score1 >= 10 OR pong_score2 >= 10) pong_state = 2;
        }
        
        DrawRoundedRect(wx+20, py + pong_p1_y, 10, 40, 2, 0x00FF00); 
        DrawRoundedRect(wx+ww-30, py + pong_p2_y, 10, 40, 2, 0xFF0000); 
        DrawRoundWindow(wx + pong_ball_x, py + pong_ball_y, 6, 0xFFFFFF); 
    }
}

/// ==========================================
/// DIE BLOBBY VOLLEY ENGINE (Ausgelagert aus win->id 12)
/// ==========================================
_50 run_blobby_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _43 inner_h = wh - 40; /// (inner_w wurde hier gelöscht)
    _43 py = wy + 40;
    _43 floor_y = inner_h - 20; 
    
    _15(bv_state EQ 0) {
        TextC(wx + ww/2, py + 20, "BLOBBY VOLLEY BARE METAL", 0x00FF00, _128);
        TextC(wx + ww/2, py + 60, "SELECT MODE", 0xFFFFFF, _128);
        DrawRoundedRect(wx + ww/2 - 110, py + 80, 100, 25, 3, bv_mode EQ 0 ? 0x00AA00 : 0x555555);
        TextC(wx + ww/2 - 60, py + 87, "1 PLAYER", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 110, py + 80, 100, 25)) bv_mode = 0;
        DrawRoundedRect(wx + ww/2 + 10, py + 80, 100, 25, 3, bv_mode EQ 1 ? 0x00AA00 : 0x555555);
        TextC(wx + ww/2 + 60, py + 87, "2 PLAYERS", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 + 10, py + 80, 100, 25)) bv_mode = 1;
        
        _15(bv_mode EQ 0) {
            TextC(wx + ww/2, py + 130, "CPU DIFFICULTY", 0xFFFFFF, _128);
            DrawRoundedRect(wx + ww/2 - 120, py + 150, 70, 25, 3, bv_diff EQ 0 ? 0x00AA00 : 0x555555); TextC(wx + ww/2 - 85, py + 157, "EASY", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 120, py + 150, 70, 25)) bv_diff = 0;
            DrawRoundedRect(wx + ww/2 - 35, py + 150, 70, 25, 3, bv_diff EQ 1 ? 0x00AA00 : 0x555555); TextC(wx + ww/2, py + 157, "NORMAL", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 35, py + 150, 70, 25)) bv_diff = 1;
            DrawRoundedRect(wx + ww/2 + 50, py + 150, 70, 25, 3, bv_diff EQ 2 ? 0x00AA00 : 0x555555); TextC(wx + ww/2 + 85, py + 157, "HARD", 0xFFFFFF, _128);
            _15(mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 + 50, py + 150, 70, 25)) bv_diff = 2;
        } _41 { 
            TextC(wx + ww/2, py + 150, "P1: MOUSE+CLICK | P2: ARROWS", 0xAAAAAA, _128); 
        }
        
        DrawRoundedRect(wx + ww/2 - 60, py + 200, 120, 30, 5, 0xAA0000); TextC(wx + ww/2, py + 207, "START MATCH", 0xFFFFFF, _128);
        _15(input_cooldown EQ 0 AND ((mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 60, py + 200, 120, 30)) OR (key_new AND (key_scancode EQ 0x1C OR key_scancode EQ 0x39)))) {
            bv_state = 1; bv_score1 = 0; bv_score2 = 0;
            bv_ball_x = ww/4; bv_ball_y = 30; bv_ball_vx = 0; bv_ball_vy = 0;
            bv_touches = 0; bv_last_touch = 0;
            input_cooldown = 10;
            key_new = _86; /// Consume key
        }
    } _41 _15(bv_state EQ 2) {
        TextC(wx + ww/2, py + 80, bv_score1 >= 10 ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!", 0x00FF00, _128);
        DrawRoundedRect(wx + ww/2 - 50, py + 140, 100, 30, 3, 0x555555); TextC(wx + ww/2, py + 148, "MENU", 0xFFFFFF, _128);
        _15(mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, wx + ww/2 - 50, py + 140, 100, 30)) { bv_state = 0; input_cooldown = 10; }
    } _41 {
        /// Strand & Himmel
        DrawRoundedRect(wx+10, py, ww-20, inner_h-10, 0, 0x4488FF); 
        DrawRoundedRect(wx+10, py+floor_y, ww-20, 10, 0, 0x00AA00); 
        DrawRoundedRect(wx+ww/2-3, py+floor_y-70, 6, 70, 0, 0xEEEEEE); 
        _30 s1[10], s2[10]; int_to_str(bv_score1, s1); int_to_str(bv_score2, s2);
        Text(wx+ww/2-40, py+20, s1, 0xFFFFFF, _128); Text(wx+ww/2+30, py+20, s2, 0xFFFFFF, _128);
        
        _15(key_new AND key_scancode EQ 0x01) { bv_state = 0; } 
        
        /// Physik
        _15(win_z[19] EQ 12) {
            bv_ball_vy += 1; 
            _15(bv_ball_vy > 12) bv_ball_vy = 12; 
            bv_ball_x += bv_ball_vx;
            bv_ball_y += bv_ball_vy;
            
            bv_p1_x = mouse_x - wx - 20; 
            _15(bv_p1_x < 10) bv_p1_x = 10;
            _15(bv_p1_x > ww/2 - 45) bv_p1_x = ww/2 - 45; 
            _15(mouse_just_pressed AND bv_p1_y >= floor_y - 30) bv_p1_vy = -14; 
            bv_p1_vy += 1; bv_p1_y += bv_p1_vy; 
            _15(bv_p1_y > floor_y - 30) { bv_p1_y = floor_y - 30; bv_p1_vy = 0; }
            
            _15(bv_mode EQ 0) {
                _43 speed = 4; _15(bv_diff EQ 1) speed = 6; _15(bv_diff EQ 2) speed = 9;
                _43 target_x = bv_ball_x + 15;
                _15(bv_ball_x > ww/2) {
                    _15(bv_ball_vx > 0 AND bv_ball_x > bv_p2_x) {
                        target_x = bv_ball_x + 40; 
                    } _41 {
                        _43 dist_y = (floor_y - 30) - bv_ball_y;
                        _15(dist_y > 0) {
                            _43 frames = dist_y / 8; 
                            target_x = bv_ball_x + (bv_ball_vx * frames) + ((bv_diff EQ 2) ? 20 : 15);
                        }
                        _15(target_x < bv_p2_x - 20) { target_x = target_x - 20; }
                    }
                } _41 { target_x = ww*3/4; }
                
                _15(target_x > ww-40) target_x = ww-40;
                _15(target_x < ww/2 + 10) target_x = ww/2 + 10;
                
                _43 move_back = speed; _43 move_fwd  = speed;
                _15(bv_diff EQ 2) {
                    _15(bv_ball_x > bv_p2_x) move_back = 25; 
                    _15(target_x < bv_p2_x - 20) move_fwd = 20; 
                }
                
                _15(bv_p2_x + 20 < target_x - 15) { bv_p2_x += move_back; } 
                _41 _15(bv_p2_x + 20 > target_x + 15) { bv_p2_x -= move_fwd;  } 
                _41 { bv_p2_x = target_x - 20; }
                
                _15(bv_ball_x > bv_p2_x - 20 AND bv_ball_x < bv_p2_x + 60) { 
                    _15(bv_ball_y > floor_y - 120 AND bv_ball_y < floor_y - 40 AND bv_p2_y >= floor_y - 30 AND bv_ball_vy > 0) {
                        _15(bv_diff >= 1 OR (frame % 2) EQ 0) bv_p2_vy = -14;
                    }
                }
            } _41 {
                _15(key_new) {
                    _15(key_scancode EQ 0x4B) bv_p2_x -= 15; 
                    _15(key_scancode EQ 0x4D) bv_p2_x += 15; 
                    _15(key_scancode EQ 0x48 AND bv_p2_y >= floor_y - 30) bv_p2_vy = -14; 
                }
            }
            
            _15(bv_p2_x < ww/2 + 5) bv_p2_x = ww/2 + 5;
            _15(bv_p2_x > ww - 50) bv_p2_x = ww - 50;
            bv_p2_vy += 1; bv_p2_y += bv_p2_vy; 
            _15(bv_p2_y > floor_y - 30) { bv_p2_y = floor_y - 30; bv_p2_vy = 0; }
            
            /// Wände & Decke
            _15(bv_ball_x < 15) { bv_ball_x = 15; bv_ball_vx = -bv_ball_vx; play_freq(800); }
            _15(bv_ball_x > ww-15) { bv_ball_x = ww-15; bv_ball_vx = -bv_ball_vx; play_freq(800); }
            _15(bv_ball_y < 15) { bv_ball_y = 15; bv_ball_vy = 2; }
            
            /// Netz-Kollision
            _15(bv_ball_x > ww/2-10 AND bv_ball_x < ww/2+10 AND bv_ball_y > floor_y-70) {
                _15(bv_ball_y < floor_y-65 AND bv_ball_vy > 0) { bv_ball_vy = -8; } 
                _41 { bv_ball_vx = -bv_ball_vx; bv_ball_x += bv_ball_vx*2; } 
                play_freq(600);
            }
            
            /// Ball trifft Blobs
            _15(bv_ball_vy > 0) {
                _15(bv_ball_y+10 > bv_p1_y AND bv_ball_y-10 < bv_p1_y+30 AND bv_ball_x > bv_p1_x-10 AND bv_ball_x < bv_p1_x+50) {
                    bv_ball_vy = -14; 
                    bv_ball_vx = (bv_ball_x - (bv_p1_x + 20)) / 3;
                    _15(bv_last_touch NEQ 1) bv_touches = 0; 
                    bv_last_touch = 1; bv_touches++;
                    _15(bv_touches > 3) { 
                        bv_score2++; bv_ball_x = ww*3/4; bv_ball_y = 30; bv_ball_vx = 0; bv_ball_vy = 0; 
                        bv_touches = 0; bv_last_touch = 0; play_sound(200, 20); 
                    } _41 play_sound(1200, 2);
                }
                
                _15(bv_ball_y+10 > bv_p2_y AND bv_ball_y-10 < bv_p2_y+30 AND bv_ball_x > bv_p2_x-10 AND bv_ball_x < bv_p2_x+50) {
                    bv_ball_vy = -14; 
                    bv_ball_vx = (bv_ball_x - (bv_p2_x + 20)) / 3;
                    _15(bv_ball_vx >= -2 AND bv_ball_vx <= 2) {
                        _15(bv_diff EQ 2) bv_ball_vx = -10; 
                        _15(bv_diff EQ 1) bv_ball_vx = -7;  
                        _15(bv_diff EQ 0) bv_ball_vx = -4;  
                    }
                    _15(bv_last_touch NEQ 2) bv_touches = 0; 
                    bv_last_touch = 2; bv_touches++;
                    _15(bv_touches > 3) { 
                        bv_score1++; bv_ball_x = ww/4; bv_ball_y = 30; bv_ball_vx = 0; bv_ball_vy = 0; 
                        bv_touches = 0; bv_last_touch = 0; play_sound(200, 20); 
                    } _41 play_sound(1200, 2);
                }
            }
            
            /// Punkt!
            _15(bv_ball_y > floor_y) {
                _15(bv_ball_x < ww/2) { bv_score2++; bv_ball_x = ww/4; }
                _41 { bv_score1++; bv_ball_x = ww*3/4; }
                bv_ball_y = 30; bv_ball_vx = 0; bv_ball_vy = 0;
                bv_touches = 0; bv_last_touch = 0; 
                play_sound(200, 20);
            }
            _15(bv_score1 >= 10 OR bv_score2 >= 10) bv_state = 2;
            _41 play_freq(0); 
        }
        
        /// Render Blobs & Ball
        DrawRoundedRect(wx + bv_p1_x, py + bv_p1_y, 40, 30, 15, 0x00FF00); 
        DrawRoundedRect(wx + bv_p2_x, py + bv_p2_y, 40, 30, 15, 0xFF0000); 
        DrawRoundWindow(wx + bv_p1_x + 25, py + bv_p1_y + 10, 4, 0xFFFFFF); DrawRoundWindow(wx + bv_p1_x + 27, py + bv_p1_y + 10, 2, 0x000000);
        DrawRoundWindow(wx + bv_p2_x + 15, py + bv_p2_y + 10, 4, 0xFFFFFF); DrawRoundWindow(wx + bv_p2_x + 13, py + bv_p2_y + 10, 2, 0x000000);
        DrawRoundWindow(wx + bv_ball_x, py + bv_ball_y, 10, 0xFFFFFF); 
    }
}

_184 browser_decode_buffer[3000000];
_44 browser_is_tba_decoded = 0;

struct __attribute__((packed)) TBAHeader {
    _30 magic[4];
    _89 fileCount;    
    _89 indexOffset;
    _182 mandalaSeed;
    _182 purityScore;
};

struct __attribute__((packed)) TBAFileEntry {
    _30 name[512];           
    _94 offset;         
    _94 originalSize;   
    _94 compressedSize; 
    _184 isCompressed;   
    _184 isPureStream;
    _184 padding[6];
};

extern _50 decompress_tba(_184* src, _43 src_len, _184* dst, _43 originalSize);

#include "font16.h"
extern _50 PutAlpha(_43 x, _43 y, _89 c);

_43 DrawHtmlChar(_43 x, _43 y, _30 c, _89 col, _44 bold) {
    if (c < 32 || c > 126) return 0;
    if (c == 32) return (bold ? 6 : 5); // Fixed width for space
    const unsigned char* glyph = font16[c - 32];
    _43 max_x = 0;
    for (_43 iy = 0; iy < 16; iy++) {
        for (_43 ix = 0; ix < 12; ix++) {
            _43 alpha = glyph[iy * 12 + ix];
            if (alpha > 40) {
                if (ix > max_x) max_x = ix;
            }
            if (alpha > 128) {
                Put(x + ix, y + iy, col);
                if (bold) Put(x + ix + 1, y + iy, col);
            } else if (alpha > 40) {
                PutAlpha(x + ix, y + iy, col);
                if (bold) PutAlpha(x + ix + 1, y + iy, col);
            }
        }
    }
    return max_x + (bold ? 3 : 2); // Add a small gap between letters
}

_30 link_urls[64][128];
_43 link_rects[64][4]; // x, y, w, h
_43 num_links = 0;

_30 input_names[16][32];
_30 input_values[16][128];
_43 input_rects[16][4];
_43 num_inputs = 0;
_43 active_input_id = -1;
_30 form_action[128] = "";

_43 _mscript_parse_arg(_30* a, int mx, int my) {
    if (a[0]=='M' && a[1]=='X') return mx;
    if (a[0]=='M' && a[1]=='Y') return my;
    if (a[0]=='0' && (a[1]=='x'||a[1]=='X')) {
        int val=0;
        for(int i=2; a[i]; i++) {
            val *= 16;
            if(a[i]>='0' && a[i]<='9') val += a[i]-'0';
            else if(a[i]>='a' && a[i]<='f') val += a[i]-'a'+10;
            else if(a[i]>='A' && a[i]<='F') val += a[i]-'A'+10;
        }
        return val;
    }
    int val=0, sign=1, i=0;
    if(a[0]=='-') { sign=-1; i++; }
    for(; a[i]; i++) {
        if(a[i]>='0' && a[i]<='9') val = val*10 + (a[i]-'0');
    }
    return val*sign;
}

_43 render_html(_43 x, _43 y, _43 max_w, _43 max_y, _71 _30* html) {
    if (!html) return y;
    num_links = 0;
    num_inputs = 0;
    form_action[0] = 0;
    _43 ox = x;
    _89 col = 0x000000;
    _44 bold = 0;
    _44 in_tag = 0;
    _30 tag_buf[128];
    _43 tag_idx = 0;
    _44 need_measure = 1;
    
    _43 form_start_y = 0;
    
    while (*html) {
        if (y > max_y) return y;
        
        if (!in_tag && need_measure) {
            _43 next_w = 0;
            _71 _30* peek = html;
            while (*peek && *peek != ' ' && *peek != '\n' && *peek != '\r' && *peek != '\t') {
                if (*peek == '<') {
                    while (*peek && *peek != '>') peek++;
                    if (*peek == '>') peek++;
                    continue;
                }
                next_w += (bold ? 7 : 6);
                peek++;
            }
            if (x != ox && x - ox + next_w >= max_w) {
                y += (bold?22:18); 
                x = ox;
            }
            need_measure = 0;
        }
        
        if (*html == '<') {
            in_tag = 1;
            tag_idx = 0;
            html++;
            continue;
        }
        if (*html == '>') {
            in_tag = 0;
            tag_buf[tag_idx] = 0;
            
            _44 is_block = 0;
            _44 is_br = 0;

            if (str_starts(tag_buf, "h1") || str_starts(tag_buf, "H1") || str_starts(tag_buf, "h2") || str_starts(tag_buf, "H2")) { bold = 1; col = 0x0000FF; is_block = 1; }
            else if (str_starts(tag_buf, "/h") || str_starts(tag_buf, "/H")) { bold = 0; is_block = 1; }
            else if (str_starts(tag_buf, "p") || str_starts(tag_buf, "P") || str_starts(tag_buf, "/p") || str_starts(tag_buf, "/P")) { is_block = 1; }
            else if (str_starts(tag_buf, "br") || str_starts(tag_buf, "BR")) { is_block = 1; is_br = 1; }
            else if (str_starts(tag_buf, "div") || str_starts(tag_buf, "DIV")) { is_block = 1; }
            else if (str_starts(tag_buf, "/div") || str_starts(tag_buf, "/DIV")) { is_block = 1; }
            else if (str_starts(tag_buf, "li") || str_starts(tag_buf, "LI")) { 
                if (x != ox) { y += 18; x = ox; }
                x = ox + 15; 
                DrawRoundedRect(ox + 5, y + 4, 5, 5, 2, 0x000000); 
                need_measure = 1; 
            }
            else if (str_starts(tag_buf, "ul") || str_starts(tag_buf, "UL") || str_starts(tag_buf, "tr") || str_starts(tag_buf, "TR") || str_starts(tag_buf, "table") || str_starts(tag_buf, "TABLE") || str_starts(tag_buf, "pre") || str_starts(tag_buf, "PRE")) { 
                is_block = 1; 
            }
            
            if (is_block) {
                col = 0x000000; /// HARTER FARB-RESET!
                
                if (is_br) {
                    if (x != ox) { y += 18; x = ox; }
                    else { y += 9; } /// Halber Abstand (9px statt 18px)
                } else {
                    if (x != ox) { 
                        y += 18; x = ox; 
                        if (str_starts(tag_buf, "/p") || str_starts(tag_buf, "/P")) {
                            y += 4; 
                        }
                    }
                }
                need_measure = 1;
            }
            
            if (str_starts(tag_buf, "a ") || str_starts(tag_buf, "A ") || str_starts(tag_buf, "a>") || str_starts(tag_buf, "A>")) { 
                col = 0xFF0000; /// BARE METAL FIX: Links werden zwingend rot!
                if (num_links < 64) {
                    _43 u_idx = 0;
                    _43 t_len = str_len(tag_buf);
                    _44 found_href = _86;
                    
                    for (_43 i = 1; i < t_len - 4; i++) {
                        if ((tag_buf[i]=='h' || tag_buf[i]=='H') && 
                            (tag_buf[i+1]=='r' || tag_buf[i+1]=='R') && 
                            (tag_buf[i+2]=='e' || tag_buf[i+2]=='E') && 
                            (tag_buf[i+3]=='f' || tag_buf[i+3]=='F') && 
                            tag_buf[i+4]=='=') {
                            
                            _43 s = i + 5;
                            char quote_char = 0;
                            
                            while(tag_buf[s] == ' ') s++;
                            if (tag_buf[s] == '"' || tag_buf[s] == '\'') {
                                quote_char = tag_buf[s];
                                s++;
                            }
                            
                            while (tag_buf[s] && u_idx < 127) {
                                if (quote_char != 0 && tag_buf[s] == quote_char) break; 
                                if (quote_char == 0 && (tag_buf[s] == ' ' || tag_buf[s] == '>')) break; 
                                link_urls[num_links][u_idx++] = tag_buf[s++];
                            }
                            found_href = _128;
                            break;
                        }
                    }
                    if (found_href) {
                        link_urls[num_links][u_idx] = 0;
                        link_rects[num_links][0] = x;
                        link_rects[num_links][1] = y;
                        link_rects[num_links][2] = 0;
                        link_rects[num_links][3] = 16;
                    }
                }
            } 
            else if (str_starts(tag_buf, "/a") || str_starts(tag_buf, "/A")) { 
                col = 0x000000; /// Nach dem Link wieder schwarzen Text zeichnen
                if (num_links < 64) {
                    _43 width = x - link_rects[num_links][0];
                    if (x < link_rects[num_links][0] || width > max_w) width = max_w - link_rects[num_links][0] + ox;
                    if (width < 5) width = 5; /// Klick-Bereich auch bei leeren Tags sichern
                    link_rects[num_links][2] = width;
                    num_links++;
                }
            }
            else if (str_starts(tag_buf, "img ") || str_starts(tag_buf, "IMG ")) {
                _43 u_idx = 0; _30 src_url[256] = {0}; _43 t_len = str_len(tag_buf); _44 found_src = _86;
                for (_43 i = 1; i < t_len - 4; i++) {
                    if ((tag_buf[i]=='s' || tag_buf[i]=='S') && (tag_buf[i+1]=='r' || tag_buf[i+1]=='R') && (tag_buf[i+2]=='c' || tag_buf[i+2]=='C') && tag_buf[i+3]=='=') {
                        _43 s = i + 4; char quote_char = 0;
                        while(tag_buf[s] == ' ') s++;
                        if (tag_buf[s] == '"' || tag_buf[s] == '\'') { quote_char = tag_buf[s]; s++; }
                        while(tag_buf[s] == ' ') s++; /// Extra Trim für schmutziges HTML!
                        
                        while (tag_buf[s] && u_idx < 255) {
                            if (quote_char != 0 && tag_buf[s] == quote_char) break; 
                            if (quote_char == 0 && (tag_buf[s] == ' ' || tag_buf[s] == '>')) break; 
                            src_url[u_idx++] = tag_buf[s++];
                        }
                        found_src = _128; break;
                    }
                }
                src_url[u_idx] = 0;

                /// BARE METAL FIX: Kugelsicherer Filter gegen Base64 und Javascript
                _44 is_valid_url = _128;
                if (str_starts(src_url, "data:") || str_starts(src_url, "DATA:") || 
                    str_starts(src_url, "javascript:") || src_url[0] == 0) {
                    is_valid_url = _86;
                }

                if (auto_img_state == 3 && browser_stb_img != 0) {
                    /// BILD IST FERTIG -> ALS MINIATUR ZEICHNEN!
                    _43 img_w = browser_stb_w; _43 img_h = browser_stb_h;
                    
                    /// Miniatur-Größe festlegen (Max 150x150 Pixel)
                    _43 max_draw_w = 150; 
                    if (max_draw_w > (max_w - (x - ox))) max_draw_w = max_w - (x - ox);
                    _43 max_draw_h = 150; 
                    
                    _43 draw_w = img_w; _43 draw_h = img_h; _43 scale = 1000;
                    if (img_w > max_draw_w || img_h > max_draw_h) {
                        _43 scale_w = (img_w * 1000) / max_draw_w; _43 scale_h = (img_h * 1000) / max_draw_h;
                        scale = (scale_w > scale_h) ? scale_w : scale_h; 
                        draw_w = (img_w * 1000) / scale; draw_h = (img_h * 1000) / scale;
                    }
                    if (x != ox) { y += 22; x = ox; } 
                    
                    DrawRoundedRect(x - 2, y - 2, draw_w + 4, draw_h + 4, 2, 0x555555);
                    for (_43 iy = 0; iy < draw_h; iy++) {
                        _43 draw_y = y + iy; if (draw_y > max_y) continue;
                        _43 sy = (iy * scale) / 1000; if (sy >= img_h) sy = img_h - 1;
                        _43 row_offset = sy * img_w * 3;
                        for (_43 ix = 0; ix < draw_w; ix++) {
                            if (x + ix > max_w + ox) continue;
                            _43 sx = (ix * scale) / 1000; if (sx >= img_w) sx = img_w - 1;
                            _43 p_idx = row_offset + (sx * 3);
                            _89 color = (browser_stb_img[p_idx] << 16) | (browser_stb_img[p_idx+1] << 8) | browser_stb_img[p_idx+2];
                            Put(x + ix, draw_y, color);
                        }
                    }
                    y += draw_h + 10; x = ox; need_measure = 1; 
                } else {
                    if (x - ox + 70 >= max_w) { y += (bold?22:18); x = ox; }
                    
                    if (!is_valid_url) {
                        /// Base64-Müll wird als grauer Kasten ignoriert
                        DrawRoundedRect(x, y-2, 30, 15, 2, 0x888888);
                        x += 35;
                    } else {
                        /// Echtes Bild! Zeige Placeholder-Rechteck
                        _89 box_col = 0x8800AA; // Lila = noch nicht geladen
                        if (found_src && link_urls[63][0] != 0 && str_starts(link_urls[63], src_url)) {
                            // Dieses Bild ist gerade aktiv
                            if (auto_img_state == 2) box_col = 0x0088AA; // Blau = lädt
                            else if (auto_img_state == 3) box_col = 0x005500; // Grün = fertig
                        }
                        DrawRoundedRect(x, y-2, 50, 15, 2, box_col); 
                        DrawHtmlChar(x+5, y-2, 'I', 0xFFFFFF, 0); DrawHtmlChar(x+12, y-2, 'M', 0xFFFFFF, 0); DrawHtmlChar(x+19, y-2, 'G', 0xFFFFFF, 0);
                        
                        /// NUR bei Maus-Klick auf diesen Platzhalter den Download starten
                        if (found_src && mouse_just_pressed &&
                            is_over_rect(mouse_x, mouse_y, x, y-2, 50, 15) &&
                            (auto_img_state == 0 || auto_img_state == 3)) {
                            str_cpy(link_urls[63], src_url);
                            auto_img_state = 1; 
                        }
                        x += 55;
                    }
                    need_measure = 1;
                }
            }
            else if (str_starts(tag_buf, "form") || str_starts(tag_buf, "FORM")) {
                form_start_y = y;
                y += 10; x = ox; need_measure = 1;
                _43 a_idx = 0;
                _43 t_len = str_len(tag_buf);
                for (_43 i = 1; i < t_len - 6; i++) {
                    if ((tag_buf[i]=='a' || tag_buf[i]=='A') && (tag_buf[i+1]=='c' || tag_buf[i+1]=='C') && tag_buf[i+6]=='=') {
                        _43 s = i + 7;
                        if (tag_buf[s] == '"') s++;
                        while (tag_buf[s] && tag_buf[s] != '"' && tag_buf[s] != ' ' && a_idx < 127) {
                            form_action[a_idx++] = tag_buf[s++];
                        }
                        break;
                    }
                }
                form_action[a_idx] = 0;
            }
            else if (str_starts(tag_buf, "/form") || str_starts(tag_buf, "/FORM")) {
                if (form_start_y > 0) {
                    _43 h = y - form_start_y + 18;
                    DrawRoundedRect(ox - 5, form_start_y - 5, max_w - ox + 10, 2, 0, 0x888888);
                    DrawRoundedRect(ox - 5, form_start_y - 5 + h, max_w - ox + 10, 2, 0, 0x888888);
                    DrawRoundedRect(ox - 5, form_start_y - 5, 2, h, 0, 0x888888);
                    DrawRoundedRect(ox - 5 + max_w - ox + 8, form_start_y - 5, 2, h, 0, 0x888888);
                    form_start_y = 0;
                }
                y += 18; x = ox; need_measure = 1;
            }
            else if (str_starts(tag_buf, "input") || str_starts(tag_buf, "INPUT")) {
                _44 is_submit = _86;
                _44 is_hidden = _86;
                _30 name_buf[32] = {0};
                _30 val_buf[128] = {0};
                _43 t_len = str_len(tag_buf);
                for (_43 i = 5; i < t_len - 4; i++) {
                    if ((tag_buf[i]=='t' || tag_buf[i]=='T') && tag_buf[i+4]=='=') {
                        _43 s = i + 5;
                        if (tag_buf[s] == '"') s++;
                        if ((tag_buf[s]=='s' || tag_buf[s]=='S') && (tag_buf[s+1]=='u' || tag_buf[s+1]=='U')) {
                            is_submit = _128;
                        }
                        if ((tag_buf[s]=='h' || tag_buf[s]=='H') && (tag_buf[s+1]=='i' || tag_buf[s+1]=='I')) {
                            is_hidden = _128;
                        }
                    }
                    if ((tag_buf[i]=='n' || tag_buf[i]=='N') && (tag_buf[i+1]=='a' || tag_buf[i+1]=='A') && tag_buf[i+4]=='=') {
                        _43 s = i + 5;
                        if (tag_buf[s] == '"') s++;
                        _43 n_idx = 0;
                        while (tag_buf[s] && tag_buf[s] != '"' && tag_buf[s] != ' ' && n_idx < 31) {
                            name_buf[n_idx++] = tag_buf[s++];
                        }
                        name_buf[n_idx] = 0;
                    }
                    if ((tag_buf[i]=='v' || tag_buf[i]=='V') && (tag_buf[i+1]=='a' || tag_buf[i+1]=='A') && tag_buf[i+5]=='=') {
                        _43 s = i + 6;
                        if (tag_buf[s] == '"') s++;
                        _43 v_idx = 0;
                        while (tag_buf[s] && tag_buf[s] != '"' && tag_buf[s] != ' ' && v_idx < 127) {
                            val_buf[v_idx++] = tag_buf[s++];
                        }
                        val_buf[v_idx] = 0;
                    }
                }
                if (is_submit) {
                    if (x - ox >= max_w) { y += (bold?22:18); x = ox; }
                    DrawRoundedRect(x, y-2, 60, 20, 2, 0x0055AA);
                    _43 old_x = x;
                    x += 5;
                    x += DrawHtmlChar(x, y, 'S', 0xFFFFFF, 1);
                    x += DrawHtmlChar(x, y, 'e', 0xFFFFFF, 1);
                    x += DrawHtmlChar(x, y, 'a', 0xFFFFFF, 1);
                    x += DrawHtmlChar(x, y, 'r', 0xFFFFFF, 1);
                    x += DrawHtmlChar(x, y, 'c', 0xFFFFFF, 1);
                    x += DrawHtmlChar(x, y, 'h', 0xFFFFFF, 1);
                    if (num_inputs < 16) {
                        input_rects[num_inputs][0] = old_x;
                        input_rects[num_inputs][1] = y-2;
                        input_rects[num_inputs][2] = 60;
                        input_rects[num_inputs][3] = 20;
                        str_cpy(input_names[num_inputs], "SUBMIT");
                        num_inputs++;
                    }
                    x = old_x + 65;
                } else if (is_hidden) {
                    if (num_inputs < 16) {
                        input_rects[num_inputs][0] = 0;
                        input_rects[num_inputs][1] = 0;
                        input_rects[num_inputs][2] = 0;
                        input_rects[num_inputs][3] = 0;
                        str_cpy(input_names[num_inputs], name_buf);
                        _15(input_values[num_inputs][0] == 0) str_cpy(input_values[num_inputs], val_buf);
                        num_inputs++;
                    }
                } else {
                    if (x - ox + 150 >= max_w) { y += (bold?22:18); x = ox; }
                    DrawRoundedRect(x, y-2, 150, 20, 2, 0xDDDDDD);
                    if (num_inputs < 16) {
                        input_rects[num_inputs][0] = x;
                        input_rects[num_inputs][1] = y-2;
                        input_rects[num_inputs][2] = 150;
                        input_rects[num_inputs][3] = 20;
                        str_cpy(input_names[num_inputs], name_buf);
                        _15(input_values[num_inputs][0] == 0) str_cpy(input_values[num_inputs], val_buf);
                        _43 tx = x + 4;
                        _30* val = input_values[num_inputs];
                        while (*val) {
                            tx += DrawHtmlChar(tx, y, *val, 0x000000, 0);
                            val++;
                        }
                        if (active_input_id == num_inputs && (frame / 20) % 2 == 0) {
                            DrawHtmlChar(tx, y, '_', 0x000000, 1);
                        }
                        num_inputs++;
                    }
                    x += 155;
                }
            }
            
            html++;
            continue;
        } // Ende von if (*html == '>')
        
        if (in_tag) {
            if (tag_idx < 127) tag_buf[tag_idx++] = *html;
            html++;
            continue;
        }
        
        if (*html == ' ' || *html == '\n' || *html == '\r' || *html == '\t') { 
            x += DrawHtmlChar(x, y, ' ', col, bold);
            need_measure = 1;
            html++; 
            continue; 
        }
        
        if (x - ox >= max_w) { y += (bold?22:18); x = ox; }
        _43 char_w = DrawHtmlChar(x, y, *html, col, bold);
        x += char_w;
        html++;
    }
    
    return y;
}

_50 TextWrapped(_43 x, _43 y, _43 max_w, _43 max_y, _71 _30* s, _89 col, _44 bold) {
    _15(!s) _96; _43 ox = x;
    _114(*s) { 
        _15(y > max_y) _96;
        _15(*s EQ '\n') { y += 15; x = ox; s++; continue; }
        _15(x - ox >= max_w) { y += 15; x = ox; }
        DrawChar(x,y,*s++,col,bold); x+=(bold?7:6); 
    } 
}

static _43 last_browser_download_len = 0;
static _43 browser_idle_frames = 0;

_43 browser_scroll_y = 0;
_43 browser_max_scroll_y = 0;
_44 browser_scroll_drag = 0;
_43 browser_scroll_drag_start_my = 0;
_43 browser_scroll_drag_start_sy = 0;

extern _43 clip_min_x;
extern _43 clip_min_y;
extern _43 clip_max_x;
extern _43 clip_max_y;

_50 run_browser_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _43 py = wy + 40;
    
    /// BARE METAL FIX: Timeout & Direct Image Interceptor!
    if (browser_download_complete == 0) {
        if (browser_download_len > 0 && browser_download_len != last_browser_download_len) {
            last_browser_download_len = browser_download_len;
            browser_idle_frames = 0;
        } else {
            browser_idle_frames++;
            if (browser_idle_frames > 90) { 
                browser_download_complete = 1;
                if (auto_img_state != 2) {
                    
                    /// MAGIC BYTE SCANNER
                    _43 payload_start = 0;
                    for (_43 i=0; i<browser_download_len-4; i++) {
                        if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' && browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') { payload_start = i + 4; break; }
                    }
                    
                    _44 is_direct_img = 0;
                    if (payload_start > 0) {
                        for (_43 k = payload_start; k < payload_start + 128 && k < browser_download_len - 4; k++) {
                            if (browser_download_buffer[k] == 0x89 && browser_download_buffer[k+1] == 'P' && browser_download_buffer[k+2] == 'N' && browser_download_buffer[k+3] == 'G') { payload_start = k; is_direct_img = 1; break; }
                            if (browser_download_buffer[k] == 0xFF && browser_download_buffer[k+1] == 0xD8 && browser_download_buffer[k+2] == 0xFF) { payload_start = k; is_direct_img = 1; break; }
                            if (browser_download_buffer[k] == 'G' && browser_download_buffer[k+1] == 'I' && browser_download_buffer[k+2] == 'F' && browser_download_buffer[k+3] == '8') { payload_start = k; is_direct_img = 1; break; }
                            if (browser_download_buffer[k] == 'B' && browser_download_buffer[k+1] == 'M') { payload_start = k; is_direct_img = 1; break; }
                        }
                    }
                    
                    if (is_direct_img) {
                        /// DECODER LOCK: Verhindert das 60FPS Dauer-Neuladen bei Direkt-Bildern!
                        if (browser_is_tba_decoded == 0) {
                            int comp; if (browser_stb_img != 0) free_image(browser_stb_img);
                            browser_stb_img = decode_image(&browser_download_buffer[payload_start], browser_download_len - payload_start, (int*)&browser_stb_w, (int*)&browser_stb_h, &comp, 3);
                            str_cpy(browser_content, ""); 
                            browser_is_tba_decoded = 1; /// Bild ist fertig, nicht nochmal decodieren!
                        }
                    } else {
                        /// SCHUTZSCHILD: Lösche das Bild NUR, wenn wir keine Miniatur anzeigen wollen!
                        if (auto_img_state == 0) {
                            if (browser_stb_img != 0) { free_image(browser_stb_img); browser_stb_img = 0; }
                        }
                        if (browser_download_len == 0) {
                            str_cpy(browser_content, "<br><br><h1>ERROR: CONNECTION FAILED</h1><br><p>The server did not respond or closed the connection.</p><p>If you tried an HTTPS site, it might require modern encryption (TLS 1.3) not supported by MeinOS.</p>");
                        } else {
                            parse_html();
                        }
                    }
                }
            }
        }
    }

    _15(browser_view EQ 0) {
        DrawRoundedRect(wx+5, py-5, ww-10, 35, 5, 0x222222);

        _44 home_hov = is_over_rect(mouse_x, mouse_y, wx+10, py, 60, 25);
        DrawRoundedRect(wx+10, py, 60, 25, 4, home_hov ? 0x444444 : 0x333333);
        Text(wx+23, py+6, "HOME", 0xFFFFFF, _128);

        _44 back_hov = is_over_rect(mouse_x, mouse_y, wx+75, py, 25, 25);
        DrawRoundedRect(wx+75, py, 25, 25, 4, back_hov ? 0x444444 : 0x333333);
        Text(wx+83, py+6, "<", 0xFFFFFF, _128);

        DrawRoundedRect(wx+105, py, ww-175, 25, 4, 0x111111);
        _43 text_offset = 0;
        if (str_starts(browser_url, "https://") || str_starts(browser_url, "HTTPS://")) {
            DrawRoundedRect(wx+114, py+7, 6, 8, 3, 0xFFD700);
            DrawRoundedRect(wx+116, py+9, 2, 4, 0, 0x111111);
            DrawRoundedRect(wx+112, py+12, 10, 8, 2, 0xFFD700);
            text_offset = 18;
        }
        Text(wx+110 + text_offset, py+6, browser_url, 0x00FFCC, _128);
        
        _15(!blocked AND (frame / 20) % 2 EQ 0) DrawChar(wx+110 + text_offset +(browser_url_idx*7), py+6, '_', 0x00FFCC, _128);
        
        _15(key_new AND win_z[19] EQ 8) { 
            _30 c = last_app_key; 
            
            _15(active_input_id >= 0) {
                _43 v_len = str_len(input_values[active_input_id]);
                _15(c EQ '\b' OR c EQ 8) {
                    _15(v_len > 0) input_values[active_input_id][v_len - 1] = 0;
                } _41 _15(c >= 32 AND c <= 126) {
                    _15(v_len < 126) { input_values[active_input_id][v_len] = c; input_values[active_input_id][v_len + 1] = 0; }
                } _41 _15(c EQ '\n' OR c EQ 13) {
                    auto_img_state = 0; 
                    _30 next_url[128];
                    _15(form_action[0] == 0) str_cpy(form_action, "/search");
                    str_cpy(next_url, form_action); str_cat(next_url, "?");
                    _44 first_arg = _128;
                    _39(_43 j = 0; j < num_inputs; j++) {
                        _15(!str_starts(input_names[j], "SUBMIT")) {
                            _15(!first_arg) str_cat(next_url, "&");
                            _15(input_names[j][0] == 0) str_cpy(input_names[j], "q");
                            str_cat(next_url, input_names[j]); str_cat(next_url, "="); str_cat(next_url, input_values[j]); first_arg = _86;
                        }
                    }
                    _15(next_url[0] == '/') {
                        _30 base_dom[64]; _43 d_idx = 0; _43 start_idx = 0;
                        _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                        _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                        _39(_43 k=0; k<start_idx; k++) { base_dom[d_idx++] = browser_url[k]; }
                        _39(_43 k=start_idx; k<63; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; base_dom[d_idx++] = browser_url[k]; }
                        base_dom[d_idx] = 0; str_cpy(browser_url, base_dom); str_cat(browser_url, next_url);
                    } _41 { str_cpy(browser_url, next_url); }
                    browser_url_idx = str_len(browser_url);
                    _30 query_dom[64]; _43 _qidx = 0; _43 start_idx = 0;
                    _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                    _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                    _39(_43 k=start_idx; k<63+start_idx; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; query_dom[_qidx++] = browser_url[k]; }
                    query_dom[_qidx] = 0; _89 target_ip = ip_str_to_u32(query_dom);
                    
                    browser_is_tba_decoded = 0; browser_stb_tried = 0;
                    if (browser_stb_img != 0) { free_image(browser_stb_img); browser_stb_img = 0; }
                    
                    _15(target_ip EQ 0) { str_cpy(browser_content, "RESOLVING DNS...\n"); send_dns_query(query_dom); } 
                    _41 { str_cpy(browser_content, "CONNECTING TO HOST...\n"); _182 tp=80; if(str_starts(browser_url,"HTTPS://") || str_starts(browser_url,"https://")) tp=443; send_tcp_syn(target_ip, tp); browser_state = 1; }
                    input_cooldown = 20; active_input_id = -1;
                }
            } _41 {
                _15(c EQ '\b' OR c EQ 8) { _15(browser_url_idx > 0) browser_url[--browser_url_idx] = 0; } 
                _41 _15(c >= 32 AND c <= 126) { _15(browser_url_idx < 60) { browser_url[browser_url_idx++] = c; browser_url[browser_url_idx] = 0; } } 
            }
        }

        _44 go_hov = is_over_rect(mouse_x, mouse_y, wx+ww-60, py, 50, 25);
        DrawRoundedRect(wx+ww-60, py, 50, 25, 4, go_hov ? 0x00AAFF : 0x0088DD); Text(wx+ww-43, py+6, "GO", 0xFFFFFF, _128);
        
        _44 click_go = (input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND go_hov);
        _44 enter_go = (key_new AND win_z[19] EQ 8 AND active_input_id EQ -1 AND (last_app_key EQ '\n' OR last_app_key EQ 13));
        _44 back_go  = (input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND back_hov);

        _15((input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND home_hov)) {
            auto_img_state = 0; 
            str_cpy(browser_last_url, browser_url); str_cpy(browser_url, "home"); browser_url_idx = 4;
            str_cpy(browser_content, "<br><br><br><h1>       COSMOS BROWSER</h1><br><p>      -       <a href=\"https://www.file-upload.net/download-15601301/music.WAV.html\">TESTrom</a> - -       <a href=\"https://github.com/DPGuide/CosmOS-Bare-Metal/blob/main/testfolder/test.bmp\">TESTbmp</a> - <input type=\"submit\" value=\"SEARCH\"></form><br><br><br><p>         <a href=\"http://wiby.me/\">Powered by Wiby Search</a>&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"http://www.google.com/\">Google</a> - <a href=\"http://theoldnet.com/\">OldNet</a></p>");
        }
        _41 _15(click_go OR enter_go OR back_go) {
            auto_img_state = 0; 
            _15(back_go) { _30 temp_url[512]; str_cpy(temp_url, browser_url); str_cpy(browser_url, browser_last_url); str_cpy(browser_last_url, temp_url); browser_url_idx = str_len(browser_url); } _41 { str_cpy(browser_last_url, browser_url); }
            
            _15(str_starts(browser_url, "home") || str_starts(browser_url, "HOME")) {
                str_cpy(browser_content, "<br><br><br><h1>       MEIN-OS HTML6 BROWSER</h1><br><p>      -       <a href=\"https://www.file-upload.net/download-15601301/music.WAV.html\">TESTrom</a> - -       <a href=\"https://github.com/DPGuide/CosmOS-Bare-Metal/blob/main/testfolder/test.bmp\">TESTbmp</a> - <input type=\"submit\" value=\"SEARCH\"></form><br><br><br><p>         <a href=\"http://wiby.me/\">Powered by Wiby Search</a>&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"http://www.google.com/\">Google</a> - <a href=\"http://theoldnet.com/\">OldNet</a></p>");
                return; 
            }
            
            browser_is_tba_decoded = 0; browser_stb_tried = 0;
            if (browser_stb_img != 0) { free_image(browser_stb_img); browser_stb_img = 0; }
            
            _30 query_dom[64]; _43 _qidx = 0; _43 start_idx = 0;
            _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
            _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
            _39(_43 k=start_idx; k<63+start_idx; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; query_dom[_qidx++] = browser_url[k]; }
            query_dom[_qidx] = 0; _89 target_ip = ip_str_to_u32(query_dom);
            _15(target_ip EQ 0) { str_cpy(browser_content, "RESOLVING DNS...\n"); send_dns_query(query_dom); } 
            _41 { str_cpy(browser_content, "CONNECTING TO HOST...\n"); _182 tp=80; if(str_starts(browser_url,"HTTPS://") || str_starts(browser_url,"https://")) tp=443; send_tcp_syn(target_ip, tp); browser_state = 1; }
            input_cooldown = 20;
        } _41 _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked) {
            _44 clicked_input = _86;
            _39(_43 i = 0; i < num_inputs; i++) {
                _15(is_over_rect(mouse_x, mouse_y, input_rects[i][0], input_rects[i][1], input_rects[i][2], input_rects[i][3])) {
                    _15(str_starts(input_names[i], "SUBMIT")) {
                        auto_img_state = 0; 
                        _30 next_url[128];
                        _15(form_action[0] == 0) str_cpy(form_action, "http://wiby.me/");
                        str_cpy(next_url, form_action); str_cat(next_url, "?"); _44 first_arg = _128;
                        _39(_43 j = 0; j < num_inputs; j++) {
                            _15(!str_starts(input_names[j], "SUBMIT")) {
                                _15(!first_arg) str_cat(next_url, "&");
                                _15(input_names[j][0] == 0) str_cpy(input_names[j], "q");
                                str_cat(next_url, input_names[j]); str_cat(next_url, "="); str_cat(next_url, input_values[j]); first_arg = _86;
                            }
                        }
                        _15(next_url[0] == '/') {
                            _30 base_dom[64]; _43 d_idx = 0; _43 start_idx = 0;
                            _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                            _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                            _39(_43 k=0; k<start_idx; k++) { base_dom[d_idx++] = browser_url[k]; }
                            _39(_43 k=start_idx; k<63; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; base_dom[d_idx++] = browser_url[k]; }
                            base_dom[d_idx] = 0; str_cpy(browser_url, base_dom); str_cat(browser_url, next_url);
                        } _41 { str_cpy(browser_url, next_url); }
                        browser_url_idx = str_len(browser_url);
                        _30 query_dom[64]; _43 _qidx = 0; _43 start_idx = 0;
                        _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                        _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                        _39(_43 k=start_idx; k<63+start_idx; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; query_dom[_qidx++] = browser_url[k]; }
                        query_dom[_qidx] = 0; _89 target_ip = ip_str_to_u32(query_dom);
                        
                        browser_is_tba_decoded = 0; browser_stb_tried = 0;
                        if (browser_stb_img != 0) { free_image(browser_stb_img); browser_stb_img = 0; }
                        
                        _15(target_ip EQ 0) { str_cpy(browser_content, "RESOLVING DNS...\n"); send_dns_query(query_dom); } 
                        _41 { str_cpy(browser_content, "CONNECTING TO HOST...\n"); _182 tp=80; if(str_starts(browser_url,"HTTPS://") || str_starts(browser_url,"https://")) tp=443; send_tcp_syn(target_ip, tp); browser_state = 1; }
                        input_cooldown = 20;
                    } _41 { active_input_id = i; }
                    clicked_input = _128; break;
                }
            }
            
            _15(!clicked_input) {
                active_input_id = -1;
                _39(_43 i = 0; i < num_links; i++) {
                    _15(is_over_rect(mouse_x, mouse_y, link_rects[i][0], link_rects[i][1], link_rects[i][2], link_rects[i][3])) {
                        auto_img_state = 0; 
                        str_cpy(browser_last_url, browser_url);
                        
                        _15(str_starts(link_urls[i], "http://") || str_starts(link_urls[i], "https://") || str_starts(link_urls[i], "HTTP://") || str_starts(link_urls[i], "HTTPS://")) {
                            str_cpy(browser_url, link_urls[i]);
                        } _41 _15(str_starts(link_urls[i], "microsoft-edge:") || str_starts(link_urls[i], "data:") || str_starts(link_urls[i], "javascript:") || str_starts(link_urls[i], "mailto:")) {
                            str_cpy(browser_content, "ERROR: UNSUPPORTED PROTOCOL.\n"); break;
                        } _41 _15(link_urls[i][0] == '/' && link_urls[i][1] == '/') {
                            str_cpy(browser_url, "http:"); str_cat(browser_url, link_urls[i]);
                        } _41 _15(link_urls[i][0] == '/') {
                            _30 base_dom[64]; _43 d_idx = 0; _43 start_idx = 0;
                            _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                            _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                            _39(_43 k=0; k<start_idx; k++) { base_dom[d_idx++] = browser_url[k]; }
                            _39(_43 k=start_idx; k<63; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; base_dom[d_idx++] = browser_url[k]; }
                            base_dom[d_idx] = 0; str_cpy(browser_url, base_dom); str_cat(browser_url, link_urls[i]);
                        } _41 {
                            _30 base_dom[128]; _43 d_idx = 0;
                            _39(_43 k=0; k<127; k++) { _15(browser_url[k] == 0 || browser_url[k] == '?') break; base_dom[d_idx++] = browser_url[k]; }
                            base_dom[d_idx] = 0;
                            _15(link_urls[i][0] != '?') { _15(base_dom[d_idx-1] != '/') { base_dom[d_idx] = '/'; base_dom[d_idx+1] = 0; } }
                            str_cpy(browser_url, base_dom); str_cat(browser_url, link_urls[i]);
                        }
                        
                        browser_url_idx = str_len(browser_url);
                        _30 query_dom[64]; _43 _qidx = 0; _43 start_idx = 0;
                        _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                        _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                        _39(_43 k=start_idx; k<63+start_idx; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; query_dom[_qidx++] = browser_url[k]; }
                        query_dom[_qidx] = 0; _89 target_ip = ip_str_to_u32(query_dom);
                        
                        browser_is_tba_decoded = 0; browser_stb_tried = 0;
                        if (browser_stb_img != 0) { free_image(browser_stb_img); browser_stb_img = 0; }
                        
                        // Reset download state for the new page
                        browser_download_complete = 0;
                        browser_download_len = 0;
                        last_browser_download_len = 0;
                        browser_idle_frames = 0;
                        browser_scroll_y = 0;
                        
                        _15(target_ip EQ 0) { str_cpy(browser_content, "RESOLVING DNS...\n"); send_dns_query(query_dom); } 
                        _41 { str_cpy(browser_content, "CONNECTING TO HOST...\n"); _182 tp=80; if(str_starts(browser_url,"HTTPS://") || str_starts(browser_url,"https://")) tp=443; send_tcp_syn(target_ip, tp); browser_state = 1; }
                        input_cooldown = 20; break;
                    }
                }
            }
        }

		DrawRoundedRect(wx+10, py+35, ww-20, wh-85, 3, 0xFFFFFF);
        
        /// ==========================================
        /// BROWSER SAVE TO DISK BUTTON
        /// ==========================================
        if (browser_download_complete == 1 && browser_download_len > 0 && auto_img_state == 0) {
            _43 btn_save_x = wx + ww - 150;
            _43 btn_save_y = py + 45;
            _44 save_hov = is_over_rect(mouse_x, mouse_y, btn_save_x, btn_save_y, 120, 25);
            
            DrawRoundedRect(btn_save_x, btn_save_y, 120, 25, 4, save_hov ? 0x00AA55 : 0x008800);
            Text(btn_save_x + 10, btn_save_y + 6, "SAVE TO DISK", 0xFFFFFF, _128);
            
            if (input_cooldown == 0 && mouse_just_pressed && !blocked && save_hov) {
                extern int selected_drive_idx;
                if (selected_drive_idx == -1) {
                    str_cpy(browser_content, "<br><br><h1>ERROR: NO DRIVE MOUNTED!</h1><br><p>Open Disk Manager to mount first!</p>");
                } else {
                    // RUFT DEINEN SAVE-AS DIALOG AUF!
                    open_browser_save_as();
                }
                input_cooldown = 20;
            }
        }
        
        /// BARE METAL FIX: ASYNCHRONER BACKGROUND FETCHER (SECURE)
        if (auto_img_state == 1 && browser_download_complete == 1) {
            _30 img_target[256]; _30* src = link_urls[63];
            while (*src == ' ') src++; 

            if (str_starts(src, "data:") || str_starts(src, "DATA:") || str_len(src) < 3) {
                auto_img_state = 3; 
            } else {
                if (str_starts(src, "http://") || str_starts(src, "https://") || str_starts(src, "HTTP://") || str_starts(src, "HTTPS://")) { 
                    str_cpy(img_target, src); 
                } else if (src[0] == '/' && src[1] == '/') { 
                    str_cpy(img_target, "http:"); str_cat(img_target, src); 
                } else if (src[0] == '/') {
                    _30 base_dom[64]; _43 d_idx = 0; 
                    _43 start_idx = 0;
                    if (str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                    else if (str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                    for(_43 k=0; k<start_idx; k++) base_dom[d_idx++] = browser_url[k];
                    for(_43 k=start_idx; k<63; k++) { 
                        if(browser_url[k]==0 || browser_url[k]=='/') break; 
                        base_dom[d_idx++] = browser_url[k]; 
                    }
                    base_dom[d_idx] = 0;
                    str_cpy(img_target, base_dom); 
                    str_cat(img_target, src);
                } else { 
                    _30 base_dom[64]; _43 d_idx = 0; 
                    _43 start_idx = 0;
                    if (str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
                    else if (str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
                    for(_43 k=0; k<start_idx; k++) base_dom[d_idx++] = browser_url[k];
                    for(_43 k=start_idx; k<63; k++) { 
                        if(browser_url[k]==0 || browser_url[k]=='/') break; 
                        base_dom[d_idx++] = browser_url[k]; 
                    }
                    base_dom[d_idx] = 0;
                    str_cpy(img_target, base_dom); 
                    str_cat(img_target, "/"); 
                    str_cat(img_target, src); 
                }
                str_cpy(image_download_url, img_target);
                browser_download_complete = 0; 
                browser_download_len = 0; 
                last_browser_download_len = 0; 
                browser_idle_frames = 0;
                
                _30 q_dom[64]; _43 qidx = 0; 
                _43 scheme_idx = 0;
                if (str_starts(img_target, "HTTP://") || str_starts(img_target, "http://")) scheme_idx = 7;
                else if (str_starts(img_target, "HTTPS://") || str_starts(img_target, "https://")) scheme_idx = 8;
                
                for(_43 k=scheme_idx; k<63+scheme_idx; k++) { 
                    if(img_target[k]==0 || img_target[k]=='/') break; 
                    q_dom[qidx++] = img_target[k]; 
                }
                q_dom[qidx] = 0; 
                
                _89 target_ip = ip_str_to_u32(q_dom);
                
                str_cpy(link_urls[62], browser_url);
                str_cpy(browser_url, img_target);
                browser_url_idx = str_len(browser_url);
                
                if (target_ip == 0) send_dns_query(q_dom); else { _182 tp=80; if(str_starts(browser_url,"HTTPS://") || str_starts(browser_url,"https://")) tp=443; send_tcp_syn(target_ip, tp); browser_state = 1; }
                auto_img_state = 2; 
            }
        }
        
        /// Wenn das Bild im Hintergrund fertig geladen wurde: Decodieren!
        if (auto_img_state == 2 && browser_download_complete == 1) {
            _43 payload_start = 0;
            for (_43 i=0; i<browser_download_len-4; i++) {
                if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' && browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') { payload_start = i + 4; break; }
            }
            if (payload_start > 0) {
                _43 real_start = payload_start;
                for (_43 k = payload_start; k < payload_start + 128 && k < browser_download_len - 4; k++) {
                    if (browser_download_buffer[k] == 0x89 && browser_download_buffer[k+1] == 'P' && browser_download_buffer[k+2] == 'N' && browser_download_buffer[k+3] == 'G') { real_start = k; break; }
                    if (browser_download_buffer[k] == 0xFF && browser_download_buffer[k+1] == 0xD8 && browser_download_buffer[k+2] == 0xFF) { real_start = k; break; }
                    if (browser_download_buffer[k] == 'G' && browser_download_buffer[k+1] == 'I' && browser_download_buffer[k+2] == 'F' && browser_download_buffer[k+3] == '8') { real_start = k; break; }
                    if (browser_download_buffer[k] == 'B' && browser_download_buffer[k+1] == 'M') { real_start = k; break; }
                }
                payload_start = real_start;

                int comp; if (browser_stb_img != 0) free_image(browser_stb_img);
                browser_stb_img = decode_image(&browser_download_buffer[payload_start], browser_download_len - payload_start, (int*)&browser_stb_w, (int*)&browser_stb_h, &comp, 3);
            }
            if (payload_start > 0) {
                int comp; if (browser_stb_img != 0) free_image(browser_stb_img);
                browser_stb_img = decode_image(&browser_download_buffer[payload_start], browser_download_len - payload_start, (int*)&browser_stb_w, (int*)&browser_stb_h, &comp, 3);

            }
            auto_img_state = 3; 
            str_cpy(browser_url, link_urls[62]);
            browser_url_idx = str_len(browser_url);
        }
		// --- BARE METAL DOWNLOAD FIX ---
        static _44 download_saved = _86;
        if (browser_download_complete == 1 && browser_download_len > 0 && !download_saved) {
            // Ist es eine .BIN Datei? (Wir prüfen die URL)
            _44 is_bin_url = _86;
            _43 ulen = str_len(browser_url);
            if (ulen > 4 && (browser_url[ulen-3] == 'b' || browser_url[ulen-3] == 'B') &&
                            (browser_url[ulen-2] == 'i' || browser_url[ulen-2] == 'I') &&
                            (browser_url[ulen-1] == 'n' || browser_url[ulen-1] == 'N')) {
                is_bin_url = _128;
            }

            if (is_bin_url) {
                // Finde den Anfang der echten Daten (HTTP Header überspringen)
                _43 payload_start = 0;
                for (_43 i=0; i<browser_download_len-4; i++) {
                    if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' && 
                        browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') { 
                        payload_start = i + 4; 
                        break; 
                    }
                }
                
                if (payload_start > 0) {
                    _43 file_size = browser_download_len - payload_start;
                    unsigned long long ram_source = (unsigned long long)&browser_download_buffer[payload_start];
                    
                    // HIER NUR NOCH AUFRUFEN! (Die Deklaration steht ja jetzt oben)
                    kernel_save_download(ram_source, file_size, "DL_APP.BIN");
                    
                    // Bildschirmausgabe im Browser
                    str_cpy(browser_content, "<br><h1>DOWNLOAD COMPLETE!</h1><br><p>Saved as DL_APP.BIN to Disk Manager.</p>");
                }
            }
            download_saved = _128; // Nur einmal speichern
        }
        
        // Reset Flag bei neuem Download
        if (browser_download_complete == 0) download_saved = _86;
        // -------------------------------

        if (browser_download_complete == 0 && browser_download_len > 0 && auto_img_state == 0) {
            _30 wait_msg[64]; str_cpy(wait_msg, "DOWNLOADING... ");
            _30 b_len_str[12]; int_to_str(browser_download_len, b_len_str);
            str_cat(wait_msg, b_len_str); str_cat(wait_msg, " BYTES");
            TextWrapped(wx+15, py+40, ww-30, wy+wh-60, wait_msg, 0x000000, _128);
        } else if (browser_stb_img != 0 && browser_content[0] == 0 && auto_img_state == 0) {
            /// VOLLBILD-RENDERER
            _43 max_draw_w = ww - 30; _43 max_draw_h = wh - 80;
            _43 img_w = browser_stb_w; _43 img_h = browser_stb_h;
            _43 draw_w = img_w; _43 draw_h = img_h; _43 scale = 1000;
            if (img_w > max_draw_w || img_h > max_draw_h) {
                _43 scale_w = (img_w * 1000) / max_draw_w; _43 scale_h = (img_h * 1000) / max_draw_h;
                scale = (scale_w > scale_h) ? scale_w : scale_h; 
                draw_w = (img_w * 1000) / scale; draw_h = (img_h * 1000) / scale;
            }
            _43 cx = wx + 15 + (max_draw_w - draw_w)/2; _43 cy = py + 40 + (max_draw_h - draw_h)/2;
            DrawRoundedRect(cx - 2, cy - 2, draw_w + 4, draw_h + 4, 2, 0x555555);
            for (_43 iy = 0; iy < draw_h; iy++) {
                _43 draw_y = cy + iy; if (draw_y > wy + wh - 10) continue;
                _43 sy = (iy * scale) / 1000; if (sy >= img_h) sy = img_h - 1;
                _43 row_offset = sy * img_w * 3;
                for (_43 ix = 0; ix < draw_w; ix++) {
                    if (cx + ix > wx + ww - 10) continue;
                    _43 sx = (ix * scale) / 1000; if (sx >= img_w) sx = img_w - 1;
                    _43 p_idx = row_offset + (sx * 3);
                    _89 color = (browser_stb_img[p_idx] << 16) | (browser_stb_img[p_idx+1] << 8) | browser_stb_img[p_idx+2];
                    Put(cx + ix, draw_y, color);
                }
            }
        } else {
            clip_min_x = wx + 10; clip_max_x = wx + ww - 30; 
            clip_min_y = py + 35; clip_max_y = py + wh - 40; 
            
            _43 final_y = render_html(wx+15, py+40 - browser_scroll_y, ww-45, 999999, browser_content);
            
            clip_min_x = 0; clip_min_y = 0; clip_max_x = 4000; clip_max_y = 4000;
            
            _43 content_h = final_y - (py + 40 - browser_scroll_y);
            _43 view_h = (py + wh - 40) - (py + 35);
            
            if (content_h > view_h) {
                browser_max_scroll_y = content_h - view_h + 20;
                _43 sb_x = wx + ww - 20; _43 sb_y = py + 35; _43 sb_w = 10; _43 sb_h = view_h;
                _43 thumb_h = (view_h * view_h) / content_h; if (thumb_h < 20) thumb_h = 20;
                _43 thumb_y = sb_y + (browser_scroll_y * (sb_h - thumb_h)) / browser_max_scroll_y;
                
                if (!blocked) {
                    if (mouse_just_pressed && is_over_rect(mouse_x, mouse_y, sb_x, sb_y, sb_w, sb_h)) {
                        browser_scroll_drag = 1;
                        if (mouse_y < thumb_y || mouse_y > thumb_y + thumb_h) {
                            browser_scroll_y = ((mouse_y - sb_y - thumb_h/2) * browser_max_scroll_y) / (sb_h - thumb_h);
                            thumb_y = sb_y + (browser_scroll_y * (sb_h - thumb_h)) / browser_max_scroll_y;
                        }
                        browser_scroll_drag_start_my = mouse_y;
                        browser_scroll_drag_start_sy = browser_scroll_y;
                    }
                }
                
                if (!mouse_down) browser_scroll_drag = 0;
                if (browser_scroll_drag) {
                    _43 dy = mouse_y - browser_scroll_drag_start_my;
                    browser_scroll_y = browser_scroll_drag_start_sy + (dy * browser_max_scroll_y) / (sb_h - thumb_h);
                }
                
                if ((int)browser_scroll_y < 0) browser_scroll_y = 0;
                if (browser_scroll_y > browser_max_scroll_y) browser_scroll_y = browser_max_scroll_y;
                
                thumb_y = sb_y + (browser_scroll_y * (sb_h - thumb_h)) / browser_max_scroll_y;
                
                DrawRoundedRect(sb_x, sb_y, sb_w, sb_h, 3, 0x333333);
                DrawRoundedRect(sb_x, thumb_y, sb_w, thumb_h, 3, 0x888888);
            } else {
                browser_max_scroll_y = 0; browser_scroll_y = 0;
            }
        }
    } _41 {
        DrawRoundedRect(wx+10, py, ww-130, 25, 3, 0xEEEEEE);
        Text(wx+15, py+8, netcfg_ip, 0x000000, _128);
        _15(!blocked AND (frame / 20) % 2 EQ 0) DrawChar(wx+15+(netcfg_ip_idx*7), py+8, '_', 0x000000, _128);
        _15(key_new AND win_z[19] EQ 8) { 
            _30 c = last_app_key; 
            _15(c EQ '\b' OR c EQ 8) { _15(netcfg_ip_idx > 0) netcfg_ip[--netcfg_ip_idx] = 0; } 
            _41 _15((c >= '0' AND c <= '9') OR c EQ '.') { _15(netcfg_ip_idx < 15) { netcfg_ip[netcfg_ip_idx++] = c; netcfg_ip[netcfg_ip_idx] = 0; } } 
        }
        _44 app_hov = is_over_rect(mouse_x, mouse_y, wx+ww-60, py, 50, 25);
        DrawRoundedRect(wx+ww-60, py, 50, 25, 3, app_hov ? 0x00AAFF : 0x0055AA); Text(wx+ww-50, py+8, "APP", 0xFFFFFF, _128);
        _44 back_hov = is_over_rect(mouse_x, mouse_y, wx+ww-115, py, 50, 25);
        DrawRoundedRect(wx+ww-115, py, 50, 25, 3, back_hov ? 0x888888 : 0x555555); Text(wx+ww-105, py+8, "BCK", 0xFFFFFF, _128);
        _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked) {
            _15(app_hov) { str_cpy(netcfg_content, "CONFIGURING NETWORK...\nSENDING GRATUITOUS ARP PING..."); apply_static_ip(netcfg_ip); input_cooldown = 20; }
            _15(back_hov) { browser_view = 0; input_cooldown = 20; }
        }
        DrawRoundedRect(wx+10, py+35, ww-20, wh-85, 3, 0xFFFFFF); Text(wx+15, py+40, netcfg_content, 0x000000, _86);
    }
}

_50 run_holyspirit(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _43 py = wy + 40;
    
    // Draw Background (Glass / Transparent Dark Grey)
    DrawGlassRect(wx, py, ww, wh-40, 0, 0x1A1A1A);
    
    // Draw Title Header
    TextC(wx+ww/2, py+10, "COSMOS DEFENDER", 0x00FFBB, _128);

    // PAUSE Button
    _44 p_hov = is_over_rect(mouse_x, mouse_y, wx+ww-100, py+10, 80, 20);
    DrawRoundedRect(wx+ww-100, py+10, 80, 20, 2, hs_paused ? 0xAA0000 : 0x00AA00);
    Text(wx+ww-80, py+15, hs_paused ? "RESUME" : "PAUSE", 0xFFFFFF, _128);
    _15(p_hov AND mouse_just_pressed AND !blocked AND input_cooldown EQ 0) {
        hs_paused = !hs_paused;
        input_cooldown = 15;
    }
    
    // AUTO DEFENSE Toggle
    _44 a_hov = is_over_rect(mouse_x, mouse_y, wx+ww-240, py+10, 130, 20);
    DrawRoundedRect(wx+ww-240, py+10, 130, 20, 2, hs_auto_suppression ? 0x00AA00 : 0xAA0000);
    Text(wx+ww-230, py+15, hs_auto_suppression ? "AUTO DEFENSE: ON" : "AUTO DEFENSE: OFF", 0xFFFFFF, _128);
    _15(a_hov AND mouse_just_pressed AND !blocked AND input_cooldown EQ 0) {
        hs_auto_suppression = !hs_auto_suppression;
        input_cooldown = 15;
    }
    
    _15(hs_state EQ 0) {
        // Radar Animation
        _43 cx = wx + 150;
        _43 cy = py + 120;
        
        hs_phase++; _15(hs_phase > 40) hs_phase = 0;
        
        DrawRoundWindow(cx, cy, 60, 0x002200);
        DrawRoundWindow(cx, cy, 40, 0x004400);
        DrawRoundWindow(cx, cy, 20 + (hs_phase/2), 0x00FF00);
        
        TextC(cx, cy+70, "OMNI-CRAWLER RADAR", 0x00FFBB, _128);
        
        // Target list
        _43 tx = wx + 300;
        Text(tx, py+50, "ACTIVE TARGETS:", 0x00FF00, _128);
        _39(int i=0; i<hs_radar_count && i<5; i++) {
            _44 hov = is_over_rect(mouse_x, mouse_y, tx, py+70+(i*15)-2, 200, 15);
            _15(hov) {
                DrawRoundedRect(tx-2, py+70+(i*15)-2, 200, 15, 2, 0x222222);
                _15(!blocked AND mouse_just_pressed AND input_cooldown EQ 0) {
                    hs_selected_target = i;
                    input_cooldown = 10;
                }
            }
            _15(hs_selected_target EQ i) {
                DrawRoundedRect(tx-2, py+70+(i*15)-2, 200, 15, 2, 0x444444);
            }
            
            _30 out_buf[64];
            str_cpy(out_buf, "> ");
            
            _30 ip_str[16];
            int_to_str(hs_radar[i].ip[0], ip_str); str_cat(out_buf, ip_str); str_cat(out_buf, ".");
            int_to_str(hs_radar[i].ip[1], ip_str); str_cat(out_buf, ip_str); str_cat(out_buf, ".");
            int_to_str(hs_radar[i].ip[2], ip_str); str_cat(out_buf, ip_str); str_cat(out_buf, ".");
            int_to_str(hs_radar[i].ip[3], ip_str); str_cat(out_buf, ip_str);
            
            str_cat(out_buf, " [");
            str_cat(out_buf, hs_radar[i].proto);
            str_cat(out_buf, "] [");
            str_cat(out_buf, hs_radar[i].country);
            str_cat(out_buf, "] HITS:");
            
            _30 hits_str[8];
            int_to_str(hs_radar[i].hits, hits_str);
            str_cat(out_buf, hits_str);
            
            _89 txt_col = hs_radar[i].isFriend ? 0x00FF00 : 0xFF4444;
            if (str_starts(hs_radar[i].threat, "BLOCKED")) txt_col = 0x555555;
            Text(tx, py+70+(i*15), out_buf, txt_col, _86);
        }
        
        // Action Panel
        _15(hs_selected_target >= 0 AND hs_selected_target < hs_radar_count) {
            _43 px = wx + 520;
            _43 py_act = py + 50;
            DrawRoundedRect(px, py_act, 120, 100, 4, 0x222222);
            Text(px+10, py_act+10, "ACTION:", 0xFFFFFF, _128);
            
            _30 ip_str[32];
            int_to_str(hs_radar[hs_selected_target].ip[0], ip_str); str_cat(ip_str, ".");
            _30 tmp[8];
            int_to_str(hs_radar[hs_selected_target].ip[1], tmp); str_cat(ip_str, tmp); str_cat(ip_str, ".");
            int_to_str(hs_radar[hs_selected_target].ip[2], tmp); str_cat(ip_str, tmp); str_cat(ip_str, ".");
            int_to_str(hs_radar[hs_selected_target].ip[3], tmp); str_cat(ip_str, tmp);
            
            _44 is_blocked = str_starts(hs_radar[hs_selected_target].threat, "BLOCKED");
            
            _44 btn_block_hov = is_over_rect(mouse_x, mouse_y, px+10, py_act+30, 100, 20);
            DrawRoundedRect(px+10, py_act+30, 100, 20, 2, btn_block_hov ? 0xAA0000 : 0x550000);
            Text(px+20, py_act+35, is_blocked ? "UNBLOCK" : "BLOCK", 0xFFFFFF, _128);
            
            _15(btn_block_hov AND mouse_just_pressed AND !blocked AND input_cooldown EQ 0) {
                _30 cmd[128];
                _15(is_blocked) {
                    str_cpy(cmd, "iptables -D INPUT -s "); str_cat(cmd, ip_str); str_cat(cmd, " -j DROP");
#ifdef __linux__
                    system(cmd);
#endif
                    str_cpy(hs_radar[hs_selected_target].threat, "SAFE");
                    hs_add_log("SYSTEM: TARGET UNBLOCKED!");
                } _41 {
                    str_cpy(cmd, "iptables -A INPUT -s "); str_cat(cmd, ip_str); str_cat(cmd, " -j DROP");
#ifdef __linux__
                    system(cmd);
#endif
                    str_cpy(hs_radar[hs_selected_target].threat, "BLOCKED");
                    hs_add_log("SYSTEM: TARGET BLOCKED!");
                }
                input_cooldown = 15;
            }
            
            _44 is_safe = hs_radar[hs_selected_target].isFriend;
            _44 btn_safe_hov = is_over_rect(mouse_x, mouse_y, px+10, py_act+60, 100, 20);
            DrawRoundedRect(px+10, py_act+60, 100, 20, 2, btn_safe_hov ? 0x00AA00 : 0x005500);
            Text(px+25, py_act+65, is_safe ? "UNSAFE" : "SAFE", 0xFFFFFF, _128);
            
            _15(btn_safe_hov AND mouse_just_pressed AND !blocked AND input_cooldown EQ 0) {
                _15(is_safe) {
                    hs_radar[hs_selected_target].isFriend = 0;
                    hs_add_log("SYSTEM: TARGET UNSAFE!");
                } _41 {
                    hs_radar[hs_selected_target].isFriend = 1;
                    str_cpy(hs_radar[hs_selected_target].threat, "SAFE");
                    hs_add_log("SYSTEM: TARGET SAFELISTED!");
                }
                input_cooldown = 15;
            }
        }

        // Spectator Mode Input
        _43 spec_y = py + 155;
        Text(wx+15, spec_y, "SPECTATOR (IP/DOM):", 0xFFFFFF, _86);
        DrawRoundedRect(wx+160, spec_y-5, 200, 20, 2, 0x111111);
        Text(wx+165, spec_y-2, hs_spectator_input, 0x00FFBB, _128);
        _15(!blocked AND (frame / 20) % 2 EQ 0) DrawChar(wx+165+(hs_spectator_idx*7), spec_y-2, '_', 0x00FFBB, _128);
        
        _15(!blocked AND key_new) {
           _30 c = last_app_key;
           _15(c EQ '\b' OR c EQ 8) { _15(hs_spectator_idx > 0) hs_spectator_input[--hs_spectator_idx] = 0; }
           _41 _15(c >= 32 AND c <= 126 AND hs_spectator_idx < 60) { hs_spectator_input[hs_spectator_idx++] = c; hs_spectator_input[hs_spectator_idx] = 0; }
        }
        
        _44 btn_spec_hov = is_over_rect(mouse_x, mouse_y, wx+370, spec_y-5, 120, 20);
        DrawRoundedRect(wx+370, spec_y-5, 120, 20, 2, btn_spec_hov ? 0x00AAFF : 0x0055AA);
        Text(wx+380, spec_y-2, "RESOLVE & WATCH", 0xFFFFFF, _86);
        
        _15(btn_spec_hov AND mouse_just_pressed AND !blocked AND input_cooldown EQ 0) {
            _15(hs_spectator_idx > 0) {
                hs_add_log("SYSTEM: RESOLVING SPECTATOR...");
                std::string to_res = hs_spectator_input;
                std::thread([to_res]() {
#ifdef __linux__
                    struct hostent* h = gethostbyname(to_res.c_str());
                    if (h) {
                        struct in_addr* addr = (struct in_addr*)h->h_addr;
                        char* ip_str = inet_ntoa(*addr);
                        hs_add_to_radar(ip_str, "SPEC");
                    } else {
                        hs_add_log("SYSTEM: RESOLVE FAILED!");
                    }
#endif
                }).detach();
                hs_spectator_idx = 0;
                hs_spectator_input[0] = 0;
            }
            input_cooldown = 15;
        }
        
        // Log Viewer
        DrawRoundedRect(wx+10, py+180, ww-20, wh-230, 2, 0x111111);
        _39(int i=0; i<hs_log_lines; i++) {
            Text(wx+15, py+185+(i*12), hs_log[i], 0x00AAFF, _86);
        }
    }
}
_50 run_network_configurator(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _43 py = wy + 40;
    
    DrawRoundedRect(wx+10, py, ww-90, 25, 3, 0xEEEEEE);
    Text(wx+15, py+8, netcfg_ip, 0x000000, _128);
    
    _15(!blocked AND (frame / 20) % 2 EQ 0) {
        DrawChar(wx+15+(netcfg_ip_idx*7), py+8, '_', 0x000000, _128);
    }
    
    /// Tastatur nur abfangen, wenn Configurator (ID 9) im Fokus ist!
    _15(key_new AND win_z[19] EQ 9) { 
        _30 c = last_app_key; 
        _15(c EQ '\b' OR c EQ 8) { 
            _15(netcfg_ip_idx > 0) { 
                netcfg_ip_idx--; 
                netcfg_ip[netcfg_ip_idx] = 0; 
            } 
        } 
        _41 _15((c >= '0' AND c <= '9') OR c EQ '.') { 
            _15(netcfg_ip_idx < 15) { 
                netcfg_ip[netcfg_ip_idx++] = c; 
                netcfg_ip[netcfg_ip_idx] = 0; 
            } 
        } 
    }

    _44 go_hov = is_over_rect(mouse_x, mouse_y, wx+ww-70, py, 60, 25);
    DrawRoundedRect(wx+ww-70, py, 60, 25, 3, go_hov ? 0x00AAFF : 0x0055AA);
    Text(wx+ww-58, py+8, "APPLY", 0xFFFFFF, _128);
    
    _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND go_hov) {
        str_cpy(netcfg_content, "CONFIGURING NETWORK INTERFACE...\n");
        str_cat(netcfg_content, "IP: ");
        str_cat(netcfg_content, netcfg_ip);
        str_cat(netcfg_content, "\nSUBNET: 255.255.255.0\nGATEWAY: 192.168.14.14\n\nSENDING GRATUITOUS ARP PING...");
        
        /// Deine Bare Metal Funktion aus der net.cpp aufrufen!
        apply_static_ip(netcfg_ip);
        
        netcfg_state = 1;
        input_cooldown = 20;
    }

    DrawRoundedRect(wx+10, py+35, ww-20, wh-85, 3, 0xFFFFFF);
    Text(wx+15, py+40, netcfg_content, 0x000000, _86);
}
// ========================================================
// DIE UNIVERSAL-WRAPPER (Greifen auf c8_step etc. zu)
// ========================================================
void c8_load_rom(unsigned long long ram_addr, int size) { }

void c8_run_frame() { 
    for (int z = 0; z < 10; z++) c8_step();
    if (c8_dt > 0) c8_dt--;
    if (c8_st > 0) { c8_st--; /* play_freq(440); */ } 
}

void c8_render(_43 wx, _43 wy, _43 ww, _43 wh) { 
    _43 avail_w = 0; if (ww > 20) avail_w = ww - 20; 
    _43 avail_h = 0; if (wh > 70) avail_h = wh - 70;
    _43 scale_x = avail_w / 64;
    _43 scale_y = avail_h / 32;
    _43 scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;   
    if (scale > 20) scale = 20;
    
    _43 draw_w = 64 * scale;
    _43 draw_h = 32 * scale;
    _43 off_x = wx; if (ww > draw_w) off_x += (ww - draw_w) / 2;
    _43 off_y = wy + 30; if (avail_h > draw_h) off_y += (avail_h - draw_h) / 2;
    
    DrawRoundedRect(off_x - 2, off_y - 2, draw_w + 4, draw_h + 4, 0, 0x333333);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 64; x++) {
            if (c8_gfx[y * 64 + x] == 1) {
                DrawRoundedRect(off_x + (x * scale), off_y + (y * scale), scale, scale, 0, 0x00FF00);
            }
        }
    }
}

void c8_input(int scancode, bool is_pressed) { }

// --- GAMEBOY DUMMYS ---
void gb_reset() { }
void gb_load_rom(unsigned long long ram_addr, int size) { }
void gb_run_frame() { }
void gb_render(_43 wx, _43 wy, _43 ww, _43 wh) { }
void gb_input(int scancode, bool is_pressed) { }

// ========================================================
// MASCHINEN REGISTRIEREN
// ========================================================
VirtualMachine emu_chip8 = {
    "CHIP-8 HYPERVISOR", 
    ".BIN", 
    c8_reset, 
    c8_load_rom, 
    c8_run_frame, 
    c8_render, 
    c8_input
};

VirtualMachine emu_gameboy = {
    "NINTENDO GAMEBOY", 
    ".GB ", 
    gb_reset, 
    gb_load_rom, 
    gb_run_frame, 
    gb_render, 
    gb_input
};

VirtualMachine* active_vm = 0;

// ========================================================
// AB HIER KOMMT DEINE: _50 run_emulator_engine(...)
// ========================================================
/// ==========================================
/// EMULATOR ENGINE
/// ==========================================
_50 run_emulator_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked) {
    _44 is_titlebar = is_over_rect(mouse_x, mouse_y, wx, wy, ww, 30);
    _44 emulator_blocked = blocked || is_titlebar; 

    _43 py = wy + 40;
    DrawRoundedRect(wx, py, ww, wh - 40, 0, 0x111112); 
    
    // Wenn keine Maschine geladen ist ODER Status 0 ist: Zeige Standby!
    if (active_vm == 0 || c8_state == 0) {
        TextC(wx + ww/2, py + 80, "UNIVERSAL HYPERVISOR", 0x00FFBB, _128);
        TextC(wx + ww/2, py + 110, "SYSTEM READY.", 0xFFFFFF, _128);
        TextC(wx + ww/2, py + 160, "PLEASE OPEN A ROM", 0x888888, _128);
        TextC(wx + ww/2, py + 185, "USING THE DISK MANAGER", 0x888888, _128);
        
        static unsigned int blink_counter = 0;
        blink_counter++;
        if ((blink_counter % 60) < 30) {
            DrawRoundedRect(wx + ww/2 - 5, py + 220, 10, 10, 0, 0x00FF00);
        }
    } else {
        /// --- EINE MASCHINE LÄUFT ---
        
        // Dynamischer Header (zeigt den Namen der aktiven Konsole an!)
        TextC(wx + ww/2, py + 10, active_vm->system_name, 0x00FFBB, _128);
        TextC(wx + ww/2, py + 25, "RUNNING... [ESC] TO POWER OFF", 0x444444, _86);
        
        if (key_new && key_scancode == 0x01) { 
            c8_state = 0; 
            active_vm = 0;
}
        
        // 1. Zyklen berechnen
        if (!emulator_blocked) {
            active_vm->run_frame();
        }
        
        // 2. Bild zeichnen
        active_vm->render(wx, py, ww, wh - 40);
    }
}

/// ==========================================
/// TBA ARCHIVE UNPACKER (READ FROM CFS)
/// ==========================================
_44 load_rom_from_tba(const char* target_rom_name, _89 target_addr) {
    sys_print("MOUNTING ROMS.TBA...\n");
    
    // 1. Suche ROMS.TBA im Disk-Manager Root
    uint32_t buf_dir = 0x00901000; 
    disk_read_auto(1002, buf_dir); 
    CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
    
    _43 tba_lba = 0;
    for (int i = 0; i < 28; i++) {
        if (dir[i].type == 1 && str_starts(dir[i].filename, "ROMS.TBA")) {
            tba_lba = dir[i].start_lba;
            break;
        }
    }
    
    if (tba_lba == 0) {
        sys_print("ERROR: ROMS.TBA NOT FOUND ON DISK!\n");
        return _86;
    }
    
    // 2. Lade den TBA Header (Erster Sektor der Datei)
    uint64_t ram_buffer = 0x08000000; // Freier RAM Bereich für Datei-Operationen
    disk_read_auto(tba_lba, ram_buffer);
    
    TBAHeader* header = (TBAHeader*)ram_buffer;
    if (header->magic[0] != 'T' || header->magic[1] != 'B' || header->magic[2] != 'A') {
        sys_print("ERROR: INVALID TBA ARCHIVE MAGIC!\n");
        return _86;
    }
    
    // 3. Lade das Inhaltsverzeichnis (Index) des Archivs
    // Der Index beginnt bei byte offset "header->indexOffset"
    _43 idx_sec = header->indexOffset / 512;
    _43 idx_off = header->indexOffset % 512;
    
    // Wir laden einfach mal 10 Sektoren Index in den RAM (reicht für viele ROMs)
    for(int s=0; s<10; s++) disk_read_auto(tba_lba + idx_sec + s, ram_buffer + (s * 512));
    
    TBAFileEntry* entries = (TBAFileEntry*)(ram_buffer + idx_off);
    uint64_t t_off = 0;
    uint64_t t_size = 0;
    
    // 4. Suche die Wunsch-ROM im Archiv
    for (uint32_t i = 0; i < header->fileCount; i++) {
        if (str_starts(entries[i].name, target_rom_name)) {
            t_off = entries[i].offset;
            t_size = entries[i].originalSize;
            break;
        }
    }
    
    if (t_size == 0) {
        sys_print("ERROR: ROM NOT IN ARCHIVE!\n");
        return _86;
    }
    
    sys_print("ROM FOUND! EXTRACTING...\n");
    
    // 5. ROM aus dem Archiv in den Emulator-RAM laden
    _43 rom_sec = t_off / 512;
    _43 rom_off = t_off % 512;
    _43 sec_count = (t_size + rom_off + 511) / 512;
    
    uint64_t extract_buf = 0x08100000;
    for (_43 s = 0; s < sec_count; s++) {
        disk_read_auto(tba_lba + rom_sec + s, extract_buf + (s * 512));
    }
    
    unsigned char* rom_src = (unsigned char*)(extract_buf + rom_off);
    unsigned char* dest = (unsigned char*)target_addr;
    
    for (_43 i = 0; i < t_size; i++) {
        if (i < 3584) { // Schutzschalter (Max CHIP-8 Größe)
            dest[i] = rom_src[i];
        }
    }
    
    sys_print("ROM MOUNTED INTO CHIP-8!\n");
    return _128;
}
