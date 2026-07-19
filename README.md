# MeinOS

MeinOS is a lightweight operating system and Win32 compatibility layer built on top of SDL2. It is capable of running Windows executables directly in Linux without WINE, including complex applications like Rufus.

We have successfully implemented Direct3D 9 Textures & Lighting Engine and  (Win32 Multi-Threading & Virtual Registry Engine!

TODO>

32-Bit EXE Support via "WoW64 Heaven's Gate" (Native CPU Transition)
Background: Is WoW64 an Emulator?
To answer your question: No, WoW64 on normal PCs is not a software emulator! Because you have an Intel/AMD (x86_64) processor, the physical CPU chip already knows how to execute 32-bit instructions natively.

WoW64 doesn't translate instructions one-by-one in software (which would be slow and take months to program ourselves). Stattdessen nutzt WoW64 einen Trick namens "Heaven's Gate":

Bevor Windows in die 32-Bit EXE springt, ändert es ein spezielles CPU-Segment-Register (CS auf 0x23). Dadurch schaltet die physische CPU in den 32-Bit-Kompatibilitätsmodus!
Die 32-Bit EXE läuft dann mit voller nativer Geschwindigkeit direkt auf dem Prozessor.
Wenn die 32-Bit EXE eine Windows-API (wie IsDebuggerPresent) aufruft, springt sie in einen kleinen Assembly-Block (Thunk), der die CPU per "Far Jump" wieder in den 64-Bit-Modus (CS = 0x33) zurückschaltet.
Dann wird deine 64-Bit C++ Funktion in kernel_main_linux.cpp ausgeführt, das Ergebnis in 32-Bit übersetzt, die CPU wieder in den 32-Bit-Modus geschaltet, und das Programm läuft weiter.
Our Challenge
You mentioned "we would have to build something ourselves". Writing a software CPU emulator from scratch for x86 (with all SSE, FPU, and thousands of instructions) is a massive, multi-month project.

Instead, we can build our own WoW64 layer (Heaven's Gate) directly into MeinOS!

Proposed Changes
We will implement a native 32-bit execution bridge in Assembly within MeinOS:

1. wow64_bridge.S (New Assembly File)
We will write a small Assembly file containing:

jump_to_32bit: A function that takes a 32-bit Entry Point and a 32-bit Stack Pointer, saves the 64-bit registers, and uses a Far Return (lret) to transition the CPU to 32-bit mode (CS=0x23) and start the EXE.
wow64_api_thunk: The target for all Windows API calls from the 32-bit PE. It will transition the CPU back to 64-bit mode (CS=0x33), read the 32-bit arguments from the stack, and call a C++ dispatcher function.
2. pe_loader.h Updates
Detect if the PE is 32-bit (Magic == 0x10B).
Allocate a 32-bit compatible Stack (under 4GB) for the EXE.
For 32-bit EXEs, when resolving APIs (IAT), we will not write the direct 64-bit C++ function address (which a 32-bit program can't jump to anyway). Instead, we write a pointer to a dynamically generated 32-bit Assembly stub (the WoW64 Thunk) that calls wow64_api_thunk.
3. kernel_main_linux.cpp Updates
Modify linux_exe_thread to check if magic == 0x10B.
If 32-bit, call jump_to_32bit(entry_point, stack32_ptr).
Implement wow64_dispatch(uint32_t api_id, uint32_t* stack_args) which routes the 32-bit API calls to our existing 64-bit API stubs (like meinos_IsDebuggerPresent).
