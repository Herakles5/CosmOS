# MeinOS

MeinOS is a lightweight operating system and Win32 compatibility layer built on top of SDL2. It is capable of running Windows executables directly in Linux without WINE, including complex applications like Rufus.

We have successfully implemented Direct3D 9 Textures & Lighting Engine and  (Win32 Multi-Threading & Virtual Registry Engine!

TODO>

PE32 (32-Bit x86) Launcher Engine Checklist
[x] Module 1: PE32 (x86 32-Bit) Header & Relocation Fixes
[x] Correct PE32 Optional Header data directory offsets in pe_loader.h
[x] Update 32-bit low virtual memory mmap allocation (MAP_32BIT / <4GB) in pe_loader.h
[x] Verify 32-bit base relocation algorithm in pe_loader.h
[x] Module 2: 32-Bit Win32 API Thunk Bridge & TEB 32 Setup
[x] Add 32-bit TEB/PEB allocation in kernel_main_linux.cpp
[x] Implement init_teb32 parameter bridge in kernel_main_linux.cpp
[x] Update PE launcher dispatcher for PE32 vs PE32+
