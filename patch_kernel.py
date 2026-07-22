import sys

with open('kernel_main_linux.cpp', 'r') as f:
    content = f.read()

# Replace 1: API Table
old1 = """        if (str_iequals(func_name, "GetProcessHeap")) return (uint64_t)meinos_GetProcessHeap;
        if (str_iequals(func_name, "HeapAlloc")) return (uint64_t)meinos_HeapAlloc;
        if (str_iequals(func_name, "HeapFree")) return (uint64_t)meinos_HeapFree;
        if (str_iequals(func_name, "InitializeCriticalSectionEx")) return (uint64_t)meinos_InitializeCriticalSectionEx;"""
new1 = """        if (str_iequals(func_name, "GetProcessHeap")) return (uint64_t)meinos_GetProcessHeap;
        if (str_iequals(func_name, "HeapAlloc")) return (uint64_t)meinos_HeapAlloc;
        if (str_iequals(func_name, "HeapFree")) return (uint64_t)meinos_HeapFree;
        if (str_iequals(func_name, "HeapSize")) return (uint64_t)meinos_HeapSize;
        if (str_iequals(func_name, "HeapReAlloc")) return (uint64_t)meinos_HeapReAlloc;
        if (str_iequals(func_name, "HeapCreate")) return (uint64_t)meinos_HeapCreate;
        if (str_iequals(func_name, "HeapDestroy")) return (uint64_t)meinos_HeapDestroy;
        if (str_iequals(func_name, "InitializeCriticalSectionEx")) return (uint64_t)meinos_InitializeCriticalSectionEx;"""

if old1 in content:
    content = content.replace(old1, new1)

# Replace 2: Stack Walk
old2 = """        if (rip == 0) {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x0. Probable cause: Missing function '%s'\\n", last_proc_name);
        } else {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x%llx. SAFELY CAUGHT.\\n", (unsigned long long)rip);
        }"""
new2 = """        if (rip == 0) {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x0. Probable cause: Missing function '%s'\\n", last_proc_name);
        } else {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x%llx. SIG: %d, ADDR: %p. SAFELY CAUGHT.\\n", (unsigned long long)rip, sig, si->si_addr);
        }"""

if old2 in content:
    content = content.replace(old2, new2)

old3 = """        printf("[SYS] RSP: 0x%llx, RBP: 0x%llx, RSI: 0x%llx, RDI: 0x%llx\\n",
            (unsigned long long)uc->uc_mcontext.gregs[REG_RSP],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RBP],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RSI],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RDI]);

        siglongjmp(jump_env, 1);"""
new3 = """        printf("[SYS] RSP: 0x%llx, RBP: 0x%llx, RSI: 0x%llx, RDI: 0x%llx\\n",
            (unsigned long long)uc->uc_mcontext.gregs[REG_RSP],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RBP],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RSI],
            (unsigned long long)uc->uc_mcontext.gregs[REG_RDI]);
            
        uint32_t current_ebp = (uint32_t)uc->uc_mcontext.gregs[REG_RBP];
        printf("[SYS] STACK WALK:\\n");
        for (int i = 0; i < 10; i++) {
            if (current_ebp < 0x40000000 || current_ebp > 0x4FFFFFFF) break; // Not a valid stack address
            uint32_t* ebp_ptr = (uint32_t*)current_ebp;
            uint32_t next_ebp = ebp_ptr[0];
            uint32_t ret_addr = ebp_ptr[1];
            uint32_t arg1 = ebp_ptr[2];
            uint32_t arg2 = ebp_ptr[3];
            uint32_t arg3 = ebp_ptr[4];
            uint32_t arg4 = ebp_ptr[5];
            printf("  Frame %d: EBP=0x%x, RET=0x%x, ARGS=(0x%x, 0x%x, 0x%x, 0x%x)\\n", i, current_ebp, ret_addr, arg1, arg2, arg3, arg4);
            current_ebp = next_ebp;
        }

        siglongjmp(jump_env, 1);"""

if old3 in content:
    content = content.replace(old3, new3)

# Replace 4: wow64_dispatch
old4 = """    extern "C" uint64_t wow64_dispatch(uint32_t api_id, uint32_t* stack_ptr, uint32_t* out_pop_bytes, wow64_context* ctx) {
        const char* name = wow64_api_name_table[api_id] ? wow64_api_name_table[api_id] : "???";
        uint64_t target = wow64_api_table[api_id];
        uint32_t arg_bytes = wow64_arg_bytes_table[api_id];
        printf("[WOW64] %s (args=%u) EBX=0x%lx\\n", name, arg_bytes, ctx ? (unsigned long)ctx->rbx : 0);
        fflush(stdout);"""
new4 = """    extern "C" uint64_t wow64_dispatch(uint32_t api_id, uint32_t* stack_ptr, uint32_t* out_pop_bytes, wow64_context* ctx) {
        const char* name = wow64_api_name_table[api_id] ? wow64_api_name_table[api_id] : "???";
        uint64_t target = wow64_api_table[api_id];
        uint32_t arg_bytes = wow64_arg_bytes_table[api_id];
        printf("[WOW64] %s (args=%u) EBX=0x%lx\\n", name, arg_bytes, ctx ? (unsigned long)ctx->rbx : 0);
        printf("  [ARGS]: ");
        for (uint32_t i = 0; i < arg_bytes / 4; i++) {
            printf("0x%x ", stack_ptr[i+1]);
        }
        printf("\\n");
        fflush(stdout);"""

if old4 in content:
    content = content.replace(old4, new4)

# Replace 5: GetModuleHandleW
old5 = """extern "C" WIN_ABI void* meinos_GetModuleHandleW(const uint16_t* lpModuleName) {
    if (!lpModuleName) return (void*)meinos_main_exe_base;
    return (void*)0;
}"""
new5 = """extern "C" WIN_ABI void* meinos_GetModuleHandleW(const uint16_t* lpModuleName) {
    if (!lpModuleName) return (void*)meinos_main_exe_base;
    
    // Dump string
    printf("[SYS] GetModuleHandleW requested for: ");
    for (int i = 0; i < 32 && lpModuleName[i]; i++) {
        printf("%c", (char)lpModuleName[i]);
    }
    printf("\\n");
    
    return (void*)0;
}"""

if old5 in content:
    content = content.replace(old5, new5)

with open('kernel_main_linux.cpp', 'w') as f:
    f.write(content)

