import re

def patch_file(in_name, out_name, is_os2=False):
    with open(in_name, 'r', encoding='utf-8') as f:
        code = f.read()

    if is_os2:
        code = code.replace('extern "C" void main(BootInfo* boot_info) {', 'extern "C" void kernel_main64(BootInfo* boot_info) {')
    else:
        # Patch execute_kernel to call kernel_main64
        exec_orig = '''    /// Echter Bare-Metal-Sprung zur 64-Bit Trampoline
    void (*kernel_entry)(BootInfo*) = (void (*)(BootInfo*))target;
    kernel_entry(info_ptr);'''
        exec_repl = '''#ifndef __linux__
    /// Echter Bare-Metal-Sprung zur 64-Bit Trampoline
    void (*kernel_entry)(BootInfo*) = (void (*)(BootInfo*))target;
    kernel_entry((BootInfo*)info_ptr);
#else
    extern "C" void kernel_main64(BootInfo*);
    kernel_main64((BootInfo*)info_ptr);
#endif'''
        code = code.replace(exec_orig, exec_repl)

    pci_scan_block = '''    /// 1. Puffer komplett nullen (RAM Müll vernichten!)
    mem_set(hw_storage, 0, 256);
    mem_set(hw_net, 0, 256);
    mem_set(hw_gpu, 0, 256);

    /// 2. Die EINZIGE Scan-Schleife ausführen (pci.cpp)
    pci_scan_all(); 
    pci_scan_advanced();
	check_incoming();
	system_init_usb();'''

    pci_scan_replacement = '''#ifndef __linux__
    /// 1. Puffer komplett nullen (RAM Müll vernichten!)
    mem_set(hw_storage, 0, 256);
    mem_set(hw_net, 0, 256);
    mem_set(hw_gpu, 0, 256);

    /// 2. Die EINZIGE Scan-Schleife ausführen (pci.cpp)
    pci_scan_all(); 
    pci_scan_advanced();
	check_incoming();
	system_init_usb();
#else
    check_incoming();
#endif'''
    code = code.replace(pci_scan_block, pci_scan_replacement)

    with open('defines.txt') as f:
        defines_block = f.read() if is_os2 else ""

    includes = f'''#include "boot_info.h"
#ifdef __linux__
#include <stdio.h>
#include <cpuid.h>
{defines_block}
#endif'''
    code = code.replace('#include "boot_info.h"', includes)

    if not is_os2:
        cpu_brand_orig = '''void get_cpu_brand() { 
    uint32_t a, b, c, d;
    uint32_t write_idx = 0;
    
    cpuid(0x80000000, &a, &b, &c, &d);'''

        cpu_brand_repl = '''void get_cpu_brand() { 
#ifdef __linux__
    unsigned int a, b, c, d;
    __cpuid(0x80000000, a, b, c, d);
    if(a < 0x80000004) { str_cpy(cpu_brand,"GENERIC X86"); return; }
    char* s = cpu_brand;
    for(unsigned int i=0x80000002; i<=0x80000004; i++){
        __cpuid(i, a, b, c, d);
        *(uint32_t*)s=a; s+=4; *(uint32_t*)s=b; s+=4; *(uint32_t*)s=c; s+=4; *(uint32_t*)s=d; s+=4;
    }
    // Trim spaces
    int write_idx=0, space_count=0;
    for(int i=0; cpu_brand[i]; i++){
        if(cpu_brand[i]==' ') {
            if(space_count==0 && write_idx>0) { cpu_brand[write_idx++] = ' '; space_count++; }
        } else {
            cpu_brand[write_idx++] = cpu_brand[i];
            space_count = 0;
        }
    }
    cpu_brand[write_idx] = 0;
#else
    uint32_t a, b, c, d;
    uint32_t write_idx = 0;
    
    cpuid(0x80000000, &a, &b, &c, &d);'''
        code = code.replace(cpu_brand_orig, cpu_brand_repl)
        code = code.replace('    cpu_brand[write_idx] = 0;\n}', '    cpu_brand[write_idx] = 0;\n#endif\n}')

    task_orig = '''	create_task(sys_idle_task);'''
    task_repl = '''#ifndef __linux__
	create_task(sys_idle_task);
#endif'''
    code = code.replace(task_orig, task_repl)

    idt_orig = '''    /// 1. IDT (Interrupts) SAUBER AUFBAUEN
    /// ==========================================
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt[0];
    
    for(int i = 0; i < 256; i++) set_idt_gate(i, 0);
    
    // BARE METAL FIX: Der Airbag! Fange alle Hardware-Fehler ab!
    for(int i = 0; i < 32; i++) {
        set_idt_gate(i, (uint64_t)dummy_isr);
    }
    
    set_idt_gate(32, (uint64_t)pit_isr);      
    set_idt_gate(33, (uint64_t)keyboard_isr);
	set_idt_gate(0x80, (uint64_t)syscall_isr, 0xEE); // BARE METAL FIX: 0xEE erlaubt der App den Zugriff!	
    set_idt_gate(39, (uint64_t)dummy_isr);
	
    remap_pic();
    init_pit(1000); 
    
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));'''

    idt_repl = '''    /// 1. IDT (Interrupts) SAUBER AUFBAUEN
    /// ==========================================
#ifndef __linux__
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt[0];
    
    for(int i = 0; i < 256; i++) set_idt_gate(i, 0);
    
    // BARE METAL FIX: Der Airbag! Fange alle Hardware-Fehler ab!
    for(int i = 0; i < 32; i++) {
        set_idt_gate(i, (uint64_t)dummy_isr);
    }
    
    set_idt_gate(32, (uint64_t)pit_isr);      
    set_idt_gate(33, (uint64_t)keyboard_isr);
	set_idt_gate(0x80, (uint64_t)syscall_isr, 0xEE); // BARE METAL FIX: 0xEE erlaubt der App den Zugriff!	
    set_idt_gate(39, (uint64_t)dummy_isr);
	
    remap_pic();
    init_pit(1000); 
    
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
#endif'''
    code = code.replace(idt_orig, idt_repl)

    code = code.replace('__asm__ volatile("sti"); /// ZÜNDUNG: Multitasking beginnt!', '#ifndef __linux__\n    __asm__ volatile("sti"); /// ZÜNDUNG: Multitasking beginnt!\n#endif')

    disk_read_orig = '''bool disk_read_auto(uint32_t lba, uint64_t target_ram_addr) {
    _15(selected_drive_idx EQ -1) _96 _86;'''

    disk_read_repl = '''bool disk_read_auto(uint32_t lba, uint64_t target_ram_addr) {
#ifdef __linux__
    FILE* f = fopen("cosmos_disk.img", "rb");
    if (!f) return false;
    fseek(f, lba * 512, SEEK_SET);
    fread((void*)target_ram_addr, 1, 512, f);
    fclose(f);
    return true;
#else
    _15(selected_drive_idx EQ -1) _96 _86;'''

    code = code.replace(disk_read_orig, disk_read_repl)

    code = code.replace('    _96 _86;\n}', '    _96 _86;\n#endif\n}')

    disk_write_orig = '''bool disk_write_auto(uint32_t lba, uint64_t source_ram_addr) {
    _15(selected_drive_idx EQ -1) _96 _86;'''

    disk_write_repl = '''bool disk_write_auto(uint32_t lba, uint64_t source_ram_addr) {
#ifdef __linux__
    FILE* f = fopen("cosmos_disk.img", "r+b");
    if (!f) return false;
    fseek(f, lba * 512, SEEK_SET);
    fwrite((void*)source_ram_addr, 1, 512, f);
    fclose(f);
    return true;
#else
    _15(selected_drive_idx EQ -1) _96 _86;'''
    code = code.replace(disk_write_orig, disk_write_repl)


    mouse_orig = '''        handle_input(); Clear();'''
    mouse_repl = '''#ifndef __linux__
        handle_input();
#else
        extern unsigned char linux_scancode;
        extern uint32_t linux_mx, linux_my;
        extern bool linux_mdown;
        
        mouse_x = linux_mx;
        mouse_y = linux_my;
        if (linux_mdown) {
            if (!mouse_down) { mouse_just_pressed = _128; mouse_down = _128; }
            else { mouse_just_pressed = _86; }
        } else {
            mouse_down = _86;
            mouse_just_pressed = _86;
        }

        key_new = _86;
        if (linux_scancode != 0) {
            unsigned char d = linux_scancode;
            linux_scancode = 0;
            if (d < 0x80) {
                key_new = _128;
                last_key = d;
                if (d == 0x2A || d == 0x36) key_shift = _128;
                
                if (input_cooldown == 0) {
                    _43 target_win = -1;
                    if (d == 0x3B) target_win = 0;
                    if (d == 0x3C) target_win = 1;
                    if (d == 0x3D) target_win = 3;
                    if (d == 0x3E) target_win = 4;
                    if (d == 0x3F) target_win = 5;
                    if (d == 0x40) target_win = 11;
                    if (d == 0x41) target_win = 12;
                    if (target_win != -1) {
                        if (windows[target_win].open) windows[target_win].open = _86;
                        else { windows[target_win].open = _128; windows[target_win].minimized = _86; focus_window(target_win); }
                        input_cooldown = 10;
                    }
                }
            } else {
                if (d == 0xAA || d == 0xB6) key_shift = _86;
            }
        }
#endif
        Clear();'''
    
    if not is_os2:
        code = code.replace(mouse_orig, mouse_repl)
    else:
        # For OS2, the GUI loop might not have Clear() directly after handle_input()
        os2_mouse_orig = '''        handle_input();
        xhci_poll_events_and_mouse();'''
        os2_mouse_repl = '''#ifndef __linux__
        handle_input();
        xhci_poll_events_and_mouse();
#else
        extern unsigned char linux_scancode;
        extern uint32_t linux_mx, linux_my;
        extern bool linux_mdown;
        
        mouse_x = linux_mx;
        mouse_y = linux_my;
        if (linux_mdown) {
            if (!mouse_down) { mouse_just_pressed = _128; mouse_down = _128; }
            else { mouse_just_pressed = _86; }
        } else {
            mouse_down = _86;
            mouse_just_pressed = _86;
        }

        if (linux_scancode != 0) {
            unsigned char d = linux_scancode;
            linux_scancode = 0;
            key_scancode = d;
            key_ready = _128;
        }
#endif'''
        code = code.replace(os2_mouse_orig, os2_mouse_repl)

    # HDA playback
    wav_orig = '''                                        uint64_t wav_ram_addr = 0x0B500000; /// 181 MB mark, safe
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;'''

    wav_repl = '''#ifdef __linux__
                                        extern uint8_t linux_wav_buffer[];
                                        uint64_t wav_ram_addr = (uint64_t)linux_wav_buffer;
#else
                                        uint64_t wav_ram_addr = 0x0B500000; /// 181 MB mark, safe
#endif
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;'''
    code = code.replace(wav_orig, wav_repl)

    virtual_drive_orig = '''void scan_pci_drives(Window* dsk_win) {'''
    virtual_drive_repl = '''void scan_pci_drives(Window* dsk_win) {
#ifdef __linux__
    print_win(dsk_win, "COSMOS LINUX VIRTUAL DISK SCANNER\\n--------------------------\\n");
    FILE* f = fopen("cosmos_disk.img", "rb");
    if (!f) {
        print_win(dsk_win, "CREATING 10MB VIRTUAL DISK...\\n");
        f = fopen("cosmos_disk.img", "wb");
        if (f) {
            uint8_t zero_block[512] = {0};
            for(int i = 0; i < 20480; i++) fwrite(zero_block, 1, 512, f); // 10MB
            fclose(f);
        }
    } else fclose(f);
    
    drives[0].type = 2; // SATA
    drives[0].size_mb = 10;
    drives[0].base_port = 0; 
    const char* m = "LINUX VIRTUAL DISK";
    for(int i=0; i<19; i++) drives[0].model[i] = m[i];
    drive_count = 1;
    selected_drive_idx = 0;
    
    char buf[128];
    str_cpy(buf, "[SYS] FOUND: "); str_cat(buf, (char*)m); str_cat(buf, "\\n");
    print_win(dsk_win, buf);
#endif
'''
    code = code.replace(virtual_drive_orig, virtual_drive_repl)


    fb_orig = '''    _15 (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb       = (_89*)((_89)mbi->fb_addr_low);
        pitch    = mbi->fb_pitch;
        screen_w = mbi->fb_width;
        screen_h = mbi->fb_height;
    } 
    _41 {
        fb = (_89*)0xFD000000; screen_w = 1920; screen_h = 1080; pitch = 1920*4;
    }'''

    fb_repl = '''#ifndef __linux__
    _15 (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb       = (_89*)((_89)mbi->fb_addr_low);
        pitch    = mbi->fb_pitch;
        screen_w = mbi->fb_width;
        screen_h = mbi->fb_height;
    } 
    _41 {
        fb = (_89*)0xFD000000; screen_w = 1920; screen_h = 1080; pitch = 1920*4;
    }
#else
    screen_w = 800;
    screen_h = 600;
    pitch = 800 * 4;
    // fb is already allocated by linux_main.cpp
    // allocate bb as well if not already!
    if (!bb) bb = new _89[1920 * 1080];
#endif'''

    code = code.replace(fb_orig, fb_repl)

    for func, repl in [
        ('_184 inb(_182 p) { _184 r; _33 _192("inb %1, %0":"=a"(r):"Nd"(p)); _96 r; }', '#ifndef __linux__\n_184 inb(_182 p) { _184 r; _33 _192("inb %1, %0":"=a"(r):"Nd"(p)); _96 r; }\n#else\n_184 inb(_182 p) { return 0; }\n#endif'),
        ('_50 outb(_182 p, _184 v) { _33 _192("outb %0, %1"::"a"(v),"Nd"(p)); }', '#ifndef __linux__\n_50 outb(_182 p, _184 v) { _33 _192("outb %0, %1"::"a"(v),"Nd"(p)); }\n#else\n_50 outb(_182 p, _184 v) { }\n#endif'),
        ('_182 inw(_182 p) { _182 r; _33 _192("inw %1, %0":"=a"(r):"Nd"(p)); _96 r; }', '#ifndef __linux__\n_182 inw(_182 p) { _182 r; _33 _192("inw %1, %0":"=a"(r):"Nd"(p)); _96 r; }\n#else\n_182 inw(_182 p) { return 0; }\n#endif'),
        ('_50 outw(_182 p, _182 v) { _33 _192("outw %0, %1"::"a"(v),"Nd"(p)); }', '#ifndef __linux__\n_50 outw(_182 p, _182 v) { _33 _192("outw %0, %1"::"a"(v),"Nd"(p)); }\n#else\n_50 outw(_182 p, _182 v) { }\n#endif'),
        ('_89 inl(_182 p) { _89 r; _33 _192("inl %1, %0":"=a"(r):"Nd"(p)); _96 r; }', '#ifndef __linux__\n_89 inl(_182 p) { _89 r; _33 _192("inl %1, %0":"=a"(r):"Nd"(p)); _96 r; }\n#else\n_89 inl(_182 p) { return 0; }\n#endif'),
        ('_50 outl(_182 p, _89 v) { _33 _192("outl %0, %1"::"a"(v),"Nd"(p)); }', '#ifndef __linux__\n_50 outl(_182 p, _89 v) { _33 _192("outl %0, %1"::"a"(v),"Nd"(p)); }\n#else\n_50 outl(_182 p, _89 v) { }\n#endif'),
        ('_50 insw(_182 p, _50* a, _89 c) { _33 _192("rep; insw" : "+D"(a), "+c"(c) : "d"(p) : "memory"); }', '#ifndef __linux__\n_50 insw(_182 p, _50* a, _89 c) { _33 _192("rep; insw" : "+D"(a), "+c"(c) : "d"(p) : "memory"); }\n#else\n_50 insw(_182 p, _50* a, _89 c) { }\n#endif'),
        ('_50 outsw(_182 p, _71 _50* a, _89 c) { _33 _192("rep; outsw" : "+S"(a), "+c"(c) : "d"(p) : "memory"); }', '#ifndef __linux__\n_50 outsw(_182 p, _71 _50* a, _89 c) { _33 _192("rep; outsw" : "+S"(a), "+c"(c) : "d"(p) : "memory"); }\n#else\n_50 outsw(_182 p, _71 _50* a, _89 c) { }\n#endif'),
        ('_89 mmio_read32(uintptr_t addr) { _96 *(_192 _89*)addr; }', '#ifndef __linux__\n_89 mmio_read32(uintptr_t addr) { _96 *(_192 _89*)addr; }\n#else\n_89 mmio_read32(uintptr_t addr) { return 0; }\n#endif'),
        ('_50 mmio_write32(uintptr_t addr, _89 val) { *(_192 _89*)addr = val; }', '#ifndef __linux__\n_50 mmio_write32(uintptr_t addr, _89 val) { *(_192 _89*)addr = val; }\n#else\n_50 mmio_write32(uintptr_t addr, _89 val) { }\n#endif'),
        ('_50 play_freq(_89 f) { ', '#ifdef __linux__\nextern "C" void play_linux_freq(uint32_t freq);\n#endif\n_50 play_freq(_89 f) { \n#ifdef __linux__\n    play_linux_freq(f);\n    return;\n#endif'),
        ('_50 play_sound(_89 n_freq, _43 duration) { ', '#ifdef __linux__\nextern "C" void play_linux_sound(uint32_t freq, uint32_t duration_ms);\n#endif\n_50 play_sound(_89 n_freq, _43 duration) { \n#ifdef __linux__\n    play_linux_sound(n_freq, duration * 10);\n    return;\n#endif')
    ]:
        code = code.replace(func, repl)

    if is_os2:
        code = code.replace('uint32_t cartridge_start = 0;', 'extern uint32_t cartridge_start;')
        code = code.replace('uint32_t cartridge_end = 0;', 'extern uint32_t cartridge_end;')
        code = code.replace('uint64_t cartridge_start = 0;', 'extern uint64_t cartridge_start;')
        code = code.replace('uint64_t cartridge_end = 0;', 'extern uint64_t cartridge_end;')
        code = code.replace('uint32_t screen_w = 1920;', 'extern uint32_t screen_w;')
        code = code.replace('uint32_t screen_h = 1080;', 'extern uint32_t screen_h;')
        code = code.replace('uint32_t* bb = (uint32_t*)0x15000000;', 'extern uint32_t* bb;')
        code = code.replace('_43 mouse_x = 400, mouse_y = 300;', 'extern _43 mouse_x; extern _43 mouse_y;')
        code = code.replace('_44 mouse_down = _86, mouse_just_pressed = _86;', 'extern _44 mouse_down; extern _44 mouse_just_pressed;')
        
        # Replace hardcoded addresses to use dynamic memory
        code = code.replace('volatile uint32_t* app_overlay_buffer = (uint32_t*)0x0D000000;', 'volatile uint32_t* app_overlay_buffer = new uint32_t[1920*1080];')
        code = code.replace('uint8_t* mft_cache = (uint8_t*)0x0E000000;', 'uint8_t* mft_cache = new uint8_t[10*1024*1024];')
        code = code.replace('uint8_t* comp_buffer = (uint8_t*)0x08000000;', 'uint8_t* comp_buffer = new uint8_t[10*1024*1024];')
        code = code.replace('uint8_t* decomp_buffer = (uint8_t*)0x08A00000;', 'uint8_t* decomp_buffer = new uint8_t[10*1024*1024];')
        
        # Comment out init_heap()
        code = code.replace('init_heap();', '// init_heap();')
        
        # Strip remaining asm volatile because kernel_main.cpp has A LOT of them!
        code = re.sub(r'__asm__\s+volatile\s*\([^;]*\);?', ';', code)
        code = re.sub(r'__asm__\s+_192\s*\([^;]*\);?', ';', code)
    else:
        # In OS1, set bb to nullptr so it gets allocated by new
        code = code.replace('uint32_t* bb = (uint32_t*)0x15000000;', 'uint32_t* bb = nullptr;')
        code = code.replace('_89* bb        = (_89*)0x02000000;', '_89* bb = nullptr;')
        code = code.replace('uint32_t usb_bot_read_sectors(uint32_t dev_addr, uint32_t ep_in, uint32_t ep_out, uint32_t lba, uint8_t count, uint32_t buffer_addr) {', 'uint32_t usb_bot_read_sectors(uint32_t dev_addr, uint32_t ep_in, uint32_t ep_out, uint32_t lba, uint8_t count, uint32_t buffer_addr) {\n#ifdef __linux__\nreturn 1;\n#endif')
        code = code.replace('uint32_t usb_bot_get_capacity(uint32_t dev_addr, uint32_t ep_in, uint32_t ep_out, uint32_t* out_block_size) {', 'uint32_t usb_bot_get_capacity(uint32_t dev_addr, uint32_t ep_in, uint32_t ep_out, uint32_t* out_block_size) {\n#ifdef __linux__\nreturn 0;\n#endif')
        code = code.replace('uint8_t* usb_buffer = (uint8_t*)usb_buffer_addr;', '#ifndef __linux__\n                        uint8_t* usb_buffer = (uint8_t*)usb_buffer_addr;\n#else\n                        uint8_t* usb_buffer = new uint8_t[512];\n#endif')
        
        # Patch KERNEL V2 STARTEN button to work in Linux
        code = code.replace('_15(os2_ram_address NEQ 0) {', '#ifndef __linux__\n                        _15(os2_ram_address NEQ 0) {\n#else\n                        _15(1) {\n#endif')
        
        # Completely replace execute_kernel() for Linux
        orig_execute = """_50 execute_kernel() {
    _15 (os2_ram_address EQ 0) _96;"""
        repl_execute = """_50 execute_kernel() {
#ifdef __linux__
    static BootInfo info_linux;
    info_linux.framebuffer_addr = (_89)(uint64_t)fb;
    info_linux.screen_width = screen_w;
    info_linux.screen_height = screen_h;
    info_linux.framebuffer_pitch = pitch;
    kernel_main64(&info_linux);
    return;
#endif
    _15 (os2_ram_address EQ 0) _96;"""
        code = code.replace(orig_execute, repl_execute)
        
        # Add the extern "C" declaration at the top
        code = code.replace('#include "boot_info.h"', '#include "boot_info.h"\n#ifdef __linux__\nextern "C" void kernel_main64(BootInfo*);\n#endif')
        
    if is_os2:
        code = code.replace('void main(BootInfo* boot_info) {', 'void kernel_main64(BootInfo* boot_info) {')
        code = code.replace('_89* bb        = (_89*)0x02000000;', '_89* bb = nullptr;')
        
        # Disable BSS zeroing in Linux because it destroys the shared memory of OS1 and SDL!
        orig_bss = """    uint8_t* bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }"""
        repl_bss = """#ifndef __linux__
    uint8_t* bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }
#endif"""
        code = code.replace(orig_bss, repl_bss)

    # Apply asm removal to BOTH kernels
    code = re.sub(r'asm\s+volatile\s*\([^;]*\);?', ';', code)
    code = re.sub(r'__asm__\s*\([^;]*\);?', ';', code)
    
    # Disable pci_scan_all and pci_scan_advanced in Linux to prevent hardware scanning crashes
    if is_os2:
        code = code.replace('    pci_scan_all();', '#ifndef __linux__\n    pci_scan_all();\n#endif')
        code = code.replace('    pci_scan_advanced();', '#ifndef __linux__\n    pci_scan_advanced();\n#endif')
        code = code.replace('fb           = (uint32_t*)(uint64_t)boot_info->framebuffer_addr;', '#ifndef __linux__\n    fb           = (uint32_t*)(uint64_t)boot_info->framebuffer_addr;\n#endif')
        code = code.replace('uint64_t* dst = (uint64_t*)fb;', 'if (!fb || !bb) return;\n    uint64_t* dst = (uint64_t*)fb;')

    with open(out_name, 'w', encoding='utf-8') as f:
        f.write(code.replace('random(', 'cosmos_random('))

patch_file('kernel.cpp', 'kernel_linux.cpp', is_os2=False)
patch_file('kernel_main.cpp', 'kernel_main_linux.cpp', is_os2=True)
print("Done generating linux patches for OS1 and OS2")
