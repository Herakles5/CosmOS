#pragma once
#include "schneider_lang.h"

/// ==========================================
/// UNIVERSAL HYPERVISOR / EMULATION ABSTRACTION
/// ==========================================

struct VirtualMachine {
    const char* system_name;       
    const char* file_extension;    
    void (*reset)();                                         
    void (*load_rom)(unsigned long long ram_addr, int size); 
    void (*run_frame)();                                     
    void (*render)(_43 wx, _43 wy, _43 ww, _43 wh);
    void (*handle_input)(int scancode, bool is_pressed);     
};

// External declarations (extern)
extern VirtualMachine emu_chip8;
extern VirtualMachine emu_gameboy;
extern VirtualMachine* active_vm;

/// Kernel to Arcade Engine bridge
_172 _50 run_pong_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
_172 _50 run_blobby_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
/// Mini Web Browser engine
_172 _50 run_browser_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);

/// Status variables for applications window (Reset triggers)
_172 _43 pong_state;
_172 _43 bv_state;