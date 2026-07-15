.intel_syntax noprefix
movabs rax, 0x1122334455667788
movabs r11, 0x1122334455667788
mov [r11], rax
movabs rax, 0x1122334455667788
jmp rax
