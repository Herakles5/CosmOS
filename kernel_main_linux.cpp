#include "stb_image.h"
#include <malloc.h>
#include "d3d9_surreal.h"
/// kernel_main.cpp (Cosmos OS V2 - Windows & Live RTC FIXED)
#include "boot_info.h"
#include <stdint.h>

extern uint8_t global_buf_mbr[4096];
extern uint8_t global_buf_dir[4096];
extern uint8_t global_tmp_dir[4096];
bool dsk_mgr_picker_mode = false;
char dsk_mgr_picked_file[256] = {0};

extern volatile int os_app_x;
extern volatile int os_app_y;
extern volatile int os_app_w;
extern volatile int os_app_h;
#ifdef __linux__
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cpuid.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <string.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <asm/ldt.h>

static __thread sigjmp_buf jump_env;
static __thread bool in_exe_thread = false;
extern "C" {
    uint64_t wow64_api_table[4096];
    int wow64_api_table_count = 0;
    uint32_t wow64_arg_bytes_table[16384];
    char* wow64_api_name_table[16384];
    uint64_t saved_fs_base = 0;
    uint32_t wow64_pop_bytes_global = 0;

#ifndef WIN_ABI
#define WIN_ABI __attribute__((ms_abi, force_align_arg_pointer))
#endif

    uint64_t wow64_dispatch(uint32_t api_id, uint32_t* stack_ptr, uint32_t* out_pop_bytes) {
        const char* name = wow64_api_name_table[api_id] ? wow64_api_name_table[api_id] : "???";
        uint64_t target = wow64_api_table[api_id];
        uint32_t arg_bytes = wow64_arg_bytes_table[api_id];
        uint64_t ebx_val;
        asm volatile("mov %%rbx, %0" : "=r"(ebx_val));
        printf("[WOW64] %s (args=%u) EBX=0x%lx\n", name, arg_bytes, ebx_val);
        fflush(stdout);
        if (out_pop_bytes) *out_pop_bytes = arg_bytes;
        
        uint64_t ret_val = 0;
        // Up to 16 arguments (64 bytes)
        typedef uint64_t (WIN_ABI *func_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
        func_t func = (func_t)target;
        
        // In MS ABI, the first 4 arguments are in RCX, RDX, R8, R9. The rest are on the stack.
        // It's safe to always pass 16 arguments, because the callee will only read what it needs!
        // We just need to make sure we don't read past the end of the 32-bit stack.
        // But stack_ptr points to the 32-bit stack which has at least 1MB of space. Reading past args is safe.
        int len = 0; while(name[len]) len++;
        if ((len == 19 && name[0]=='W' && name[4]=='C') || (len == 19 && name[0]=='M' && name[1]=='u') || (len == 12 && name[0]=='S' && name[1]=='e') || (len == 12 && name[0]=='G' && name[1]=='e')) {
            printf("[WOW64] %s called from ret_addr=0x%x\n", name, stack_ptr[0]);
            
            // Walk the EBP chain!
            // The 32-bit EBP was saved in wow64_bridge.S. We can't access it easily here unless we pass it!
            // But wait! We can just walk the stack upwards and look for return addresses!
            printf("  [STACK SCAN]: ");
            for(int i=1; i<24; i++) {
                uint32_t val = stack_ptr[i];
                if (val > 0x400000 && val < 0x500000) {
                    printf("0x%x ", val);
                }
            }
            printf("\n");
        }

        ret_val = func(
            stack_ptr[1], stack_ptr[2], stack_ptr[3], stack_ptr[4],
            stack_ptr[5], stack_ptr[6], stack_ptr[7], stack_ptr[8],
            stack_ptr[9], stack_ptr[10], stack_ptr[11], stack_ptr[12],
            stack_ptr[13], stack_ptr[14], stack_ptr[15], stack_ptr[16]
        );
        
        printf("[WOW64] %s returned 0x%lx\n", name, ret_val);
        fflush(stdout);
        return ret_val;
        
        return 0;
    }
}
uint64_t glibc_fs_base = 0;
bool is_pe_32bit = false;

// Win32 API stdcall argument byte counts for WoW64 bridge dispatch
uint32_t get_api_arg_bytes(const char* name) {
    if (!name) return 0;
    // Struct for fast lookup
    struct { const char* name; uint32_t bytes; } table[] = {
        // KERNEL32 - Process/Thread
        {"GetCurrentProcess", 0},
        {"GetCurrentProcessId", 0},
        {"GetCurrentThreadId", 0},
        {"GetCurrentThread", 0},
        {"ExitProcess", 4},
        {"TerminateProcess", 8},
        {"CreateProcessW", 40},
        {"CreateProcessA", 40},
        {"CreateThread", 24},
        {"ExitThread", 4},
        {"ResumeThread", 4},
        {"SuspendThread", 4},
        {"GetExitCodeThread", 8},
        {"GetExitCodeProcess", 8},
        {"WaitForSingleObject", 8},
        {"WaitForMultipleObjects", 16},
        {"Sleep", 4},
        {"SleepEx", 8},
        {"SwitchToThread", 0},
        {"GetProcessId", 4},
        {"OpenProcess", 12},
        {"IsProcessorFeaturePresent", 4},
        {"GetStartupInfoW", 4},
        {"GetStartupInfoA", 4},
        {"FlsAlloc", 4},
        {"FlsSetValue", 8},
        {"FlsGetValue", 4},
        {"FlsFree", 4},
        {"TlsAlloc", 0},
        {"TlsSetValue", 8},
        {"TlsGetValue", 4},
        {"TlsFree", 4},
        {"ProcessIdToSessionId", 8},
        {"SetThreadExecutionState", 4},
        // KERNEL32 - Memory
        {"GetProcessHeap", 0},
        {"HeapCreate", 12},
        {"HeapDestroy", 4},
        {"HeapAlloc", 12},
        {"HeapReAlloc", 16},
        {"HeapFree", 12},
        {"HeapSize", 12},
        {"HeapSetInformation", 16},
        {"VirtualAlloc", 16},
        {"VirtualFree", 12},
        {"VirtualProtect", 16},
        {"VirtualQuery", 12},
        {"GlobalAlloc", 8},
        {"GlobalFree", 4},
        {"GlobalLock", 4},
        {"GlobalUnlock", 4},
        {"LocalAlloc", 8},
        {"LocalFree", 4},
        // KERNEL32 - File I/O
        {"CreateFileW", 28},
        {"CreateFileA", 28},
        {"ReadFile", 20},
        {"WriteFile", 20},
        {"CloseHandle", 4},
        {"SetFilePointer", 16},
        {"SetFilePointerEx", 20},
        {"GetFileSize", 8},
        {"GetFileSizeEx", 8},
        {"GetFileType", 4},
        {"FlushFileBuffers", 4},
        {"SetEndOfFile", 4},
        {"DeleteFileW", 4},
        {"DeleteFileA", 4},
        {"MoveFileW", 8},
        {"MoveFileExW", 12},
        {"CopyFileW", 12},
        {"CopyFileExW", 24},
        {"CreateDirectoryW", 8},
        {"CreateDirectoryA", 8},
        {"RemoveDirectoryW", 4},
        {"GetCurrentDirectoryW", 8},
        {"SetCurrentDirectoryW", 4},
        {"GetTempPathW", 8},
        {"GetTempPathA", 8},
        {"GetTempFileNameW", 16},
        {"GetFullPathNameW", 16},
        {"GetFileAttributesW", 4},
        {"GetFileAttributesA", 4},
        {"GetFileAttributesExW", 12},
        {"SetFileAttributesW", 8},
        {"SetFileTime", 16},
        {"GetFileTime", 16},
        {"FindFirstFileW", 8},
        {"FindFirstFileExW", 24},
        {"FindNextFileW", 8},
        {"FindClose", 4},
        {"GetVolumePathNameW", 12},
        {"DuplicateHandle", 28},
        {"SetFileValidData", 8},
        // KERNEL32 - Module
        {"GetModuleHandleW", 4},
        {"GetModuleHandleA", 4},
        {"GetModuleHandleExW", 12},
        {"GetModuleFileNameW", 12},
        {"GetModuleFileNameA", 12},
        {"LoadLibraryW", 4},
        {"LoadLibraryA", 4},
        {"LoadLibraryExW", 12},
        {"LoadLibraryExA", 12},
        {"FreeLibrary", 4},
        {"GetProcAddress", 8},
        // KERNEL32 - String/Locale
        {"MultiByteToWideChar", 24},
        {"WideCharToMultiByte", 32},
        {"GetACP", 0},
        {"GetOEMCP", 0},
        {"GetCPInfo", 8},
        {"IsValidCodePage", 4},
        {"LCMapStringW", 24},
        {"CompareStringW", 24},
        {"CompareStringA", 24},
        {"GetStringTypeW", 16},
        {"GetLocaleInfoW", 16},
        {"GetUserDefaultLCID", 0},
        {"GetUserDefaultLangID", 0},
        {"GetSystemDefaultLangID", 0},
        {"GetUserDefaultUILanguage", 0},
        {"GetThreadLocale", 0},
        {"SetThreadLocale", 4},
        {"GetDateFormatW", 24},
        // KERNEL32 - Sync
        {"InitializeCriticalSection", 4},
        {"InitializeCriticalSectionAndSpinCount", 8},
        {"InitializeCriticalSectionEx", 12},
        {"DeleteCriticalSection", 4},
        {"EnterCriticalSection", 4},
        {"LeaveCriticalSection", 4},
        {"TryEnterCriticalSection", 4},
        {"CreateEventW", 16},
        {"CreateEventA", 16},
        {"SetEvent", 4},
        {"ResetEvent", 4},
        {"CreateMutexW", 12},
        {"CreateMutexA", 12},
        {"ReleaseMutex", 4},
        {"CreateSemaphoreW", 16},
        {"ReleaseSemaphore", 12},
        {"CreateFileMappingW", 24},
        {"MapViewOfFile", 20},
        {"UnmapViewOfFile", 4},
        {"InterlockedIncrement", 4},
        {"InterlockedDecrement", 4},
        {"InterlockedExchange", 8},
        {"InterlockedCompareExchange", 12},
        {"InitializeSListHead", 4},
        // KERNEL32 - Error/Debug
        {"GetLastError", 0},
        {"LCMapStringEx", 36},
        {"SetDefaultDllDirectories", 4},
        {"SetLastError", 4},
        {"RaiseException", 16},
        {"UnhandledExceptionFilter", 4},
        {"SetUnhandledExceptionFilter", 4},
        {"IsDebuggerPresent", 0},
        {"OutputDebugStringW", 4},
        {"OutputDebugStringA", 4},
        {"RtlUnwind", 16},
        // KERNEL32 - Time
        {"GetSystemTimeAsFileTime", 4},
        {"GetSystemTime", 4},
        {"GetLocalTime", 4},
        {"QueryPerformanceCounter", 4},
        {"QueryPerformanceFrequency", 4},
        {"GetTickCount", 0},
        {"GetTickCount64", 0},
        {"SystemTimeToFileTime", 8},
        {"FileTimeToSystemTime", 8},
        {"FileTimeToLocalFileTime", 8},
        {"LocalFileTimeToFileTime", 8},
        {"SystemTimeToTzSpecificLocalTime", 12},
        {"GetTimeZoneInformation", 4},
        {"DosDateTimeToFileTime", 12},
        // KERNEL32 - Console
        {"GetStdHandle", 4},
        {"SetStdHandle", 8},
        {"WriteConsoleW", 20},
        {"GetConsoleCP", 0},
        {"GetConsoleMode", 8},
        // KERNEL32 - Environment/System
        {"GetEnvironmentStringsW", 0},
        {"FreeEnvironmentStringsW", 4},
        {"GetEnvironmentVariableW", 12},
        {"SetEnvironmentVariableW", 8},
        {"GetCommandLineW", 0},
        {"GetCommandLineA", 0},
        {"GetSystemDirectoryW", 8},
        {"GetSystemDirectoryA", 8},
        {"GetWindowsDirectoryW", 8},
        {"GetSystemWow64DirectoryW", 8},
        {"GetComputerNameW", 8},
        {"GetSystemInfo", 4},
        {"GetNativeSystemInfo", 4},
        {"GetVersionExW", 4},
        {"GetVersionExA", 4},
        {"GetVersion", 0},
        {"VerifyVersionInfoW", 16},
        {"VerSetConditionMask", 16},  // Actually returns ULONGLONG, but 3 args on 32-bit
        {"ExpandEnvironmentStringsW", 12},
        // KERNEL32 - Encoding/Pointer
        {"EncodePointer", 4},
        {"DecodePointer", 4},
        {"FormatMessageW", 28},
        {"FormatMessageA", 28},
        // KERNEL32 - Pipe
        {"CreateNamedPipeW", 32},
        {"ConnectNamedPipe", 8},
        {"SetNamedPipeHandleState", 16},
        // KERNEL32 - Misc
        {"GetComputerNameW", 8},
        {"lstrlenA", 4},
        {"lstrlenW", 4},
        {"lstrcmpiW", 8},
        {"lstrcmpW", 8},
        {"CreateMutex", 12},
        {"PostThreadMessageW", 16},
        // USER32
        {"MessageBoxW", 16},
        {"MessageBoxA", 16},
        {"CreateWindowExW", 48},
        {"CreateWindowExA", 48},
        {"DestroyWindow", 4},
        {"ShowWindow", 8},
        {"UpdateWindow", 4},
        {"MoveWindow", 24},
        {"SetWindowPos", 28},
        {"GetWindowRect", 8},
        {"GetClientRect", 8},
        {"SetWindowTextW", 8},
        {"SetWindowTextA", 8},
        {"GetWindowTextW", 12},
        {"GetWindowTextA", 12},
        {"GetWindowTextLengthW", 4},
        {"DefWindowProcW", 16},
        {"DefWindowProcA", 16},
        {"RegisterClassW", 4},
        {"RegisterClassExW", 4},
        {"UnregisterClassW", 8},
        {"GetMessageW", 16},
        {"GetMessageA", 16},
        {"PeekMessageW", 20},
        {"PeekMessageA", 20},
        {"TranslateMessage", 4},
        {"DispatchMessageW", 4},
        {"DispatchMessageA", 4},
        {"PostMessageW", 16},
        {"PostMessageA", 16},
        {"SendMessageW", 16},
        {"SendMessageA", 16},
        {"PostQuitMessage", 4},
        {"SetTimer", 16},
        {"KillTimer", 8},
        {"InvalidateRect", 12},
        {"BeginPaint", 8},
        {"EndPaint", 8},
        {"GetDC", 4},
        {"ReleaseDC", 8},
        {"SetWindowLongW", 12},
        {"GetWindowLongW", 8},
        {"SetWindowLongPtrW", 12},
        {"GetWindowLongPtrW", 8},
        {"EnableWindow", 8},
        {"IsWindowVisible", 4},
        {"IsWindow", 4},
        {"GetParent", 4},
        {"SetParent", 8},
        {"GetDlgItem", 8},
        {"SetDlgItemTextW", 12},
        {"GetDlgItemTextW", 16},
        {"LoadCursorW", 8},
        {"LoadCursorA", 8},
        {"LoadIconW", 8},
        {"LoadIconA", 8},
        {"LoadBitmapW", 8},
        {"GetCursorPos", 4},
        {"GetSystemMetrics", 4},
        {"AdjustWindowRectEx", 16},
        {"GetDesktopWindow", 0},
        {"GetForegroundWindow", 0},
        {"SetForegroundWindow", 4},
        {"GetFocus", 0},
        {"SetFocus", 4},
        {"IsDialogMessageW", 8},
        {"MsgWaitForMultipleObjects", 20},
        {"WaitForInputIdle", 8},
        {"GetMonitorInfoW", 8},
        {"MonitorFromPoint", 12},
        {"MonitorFromWindow", 8},
        {"ScreenToClient", 8},
        {"ClientToScreen", 8},
        {"MapWindowPoints", 16},
        // GDI32
        {"CreateCompatibleDC", 4},
        {"DeleteDC", 4},
        {"SelectObject", 8},
        {"DeleteObject", 4},
        {"GetObjectW", 12},
        {"GetObjectA", 12},
        {"BitBlt", 36},
        {"StretchBlt", 44},
        {"CreateSolidBrush", 4},
        {"SetBkMode", 8},
        {"SetTextColor", 8},
        {"TextOutW", 20},
        // ADVAPI32
        {"RegOpenKeyExW", 20},
        {"RegCloseKey", 4},
        {"RegQueryValueExW", 24},
        {"RegSetValueExW", 24},
        {"RegCreateKeyExW", 36},
        {"RegDeleteKeyW", 8},
        {"RegEnumKeyExW", 32},
        {"RegEnumValueW", 32},
        {"RegQueryInfoKeyW", 48},
        {"OpenProcessToken", 12},
        {"GetTokenInformation", 20},
        {"CheckTokenMembership", 12},
        {"AllocateAndInitializeSid", 44},
        {"FreeSid", 4},
        {"InitializeSecurityDescriptor", 8},
        {"SetSecurityDescriptorDacl", 16},
        {"SetSecurityDescriptorOwner", 12},
        {"SetSecurityDescriptorGroup", 12},
        {"SetEntriesInAclA", 16},
        {"CryptAcquireContextW", 20},
        {"CryptReleaseContext", 8},
        {"CryptCreateHash", 20},
        {"CryptHashData", 16},
        {"CryptGetHashParam", 20},
        {"CryptDestroyHash", 4},
        {"QueryServiceConfigW", 16},
        {"AdjustTokenPrivileges", 24},
        {"LookupPrivilegeValueW", 12},
        {"InitiateSystemShutdownExW", 24},
        {"GetUserNameW", 8},
        {"RegDeleteValueW", 8},
        {"CloseEventLog", 4},
        {"OpenEventLogW", 8},
        {"ReportEventW", 36},
        {"ConvertStringSecurityDescriptorToSecurityDescriptorW", 16},
        {"DecryptFileW", 8},
        {"CreateWellKnownSid", 16},
        {"InitializeAcl", 12},
        {"SetEntriesInAclW", 16},
        {"ChangeServiceConfigW", 44},
        {"CloseServiceHandle", 4},
        {"ControlService", 12},
        {"OpenSCManagerW", 12},
        {"OpenServiceW", 12},
        {"QueryServiceStatus", 8},
        {"SetNamedSecurityInfoW", 24},
        // OLE32
        {"CoInitialize", 4},
        {"CoInitializeEx", 8},
        {"CoUninitialize", 0},
        {"CoCreateInstance", 20},
        {"CoTaskMemFree", 4},
        {"CoTaskMemAlloc", 4},
        {"CLSIDFromProgID", 8},
        {"StringFromGUID2", 12},
        {"CoInitializeSecurity", 36},
        // OLEAUT32
        {"SysAllocString", 4},
        {"SysFreeString", 4},
        {"SysStringLen", 4},
        {"SysAllocStringLen", 8},
        {"VariantInit", 4},
        {"VariantClear", 4},
        // SHELL32
        {"ShellExecuteExW", 4},
        {"SHGetFolderPathW", 20},
        {"CommandLineToArgvW", 8},
        // RPCRT4
        {"UuidCreate", 4},
        // SHLWAPI
        {"PathCombineW", 12},
        {"PathFileExistsW", 4},
        // WS2_32
        {"WSAStartup", 8},
        {"WSACleanup", 0},
        {"WSAGetLastError", 0},
        {"socket", 12},
        {"connect", 12},
        {"send", 16},
        {"recv", 16},
        {"closesocket", 4},
        {"bind", 12},
        {"listen", 8},
        {"accept", 12},
        {"select", 20},
        {"htons", 4},
        {"htonl", 4},
        {"ntohs", 4},
        {"ntohl", 4},
        {"inet_addr", 4},
        {"gethostbyname", 4},
        // WININET
        {"InternetOpenW", 20},
        {"InternetConnectW", 32},
        {"HttpOpenRequestW", 32},
        {"HttpSendRequestW", 20},
        {"InternetReadFile", 16},
        {"InternetCloseHandle", 4},
        // MSVCRT
        {"_beginthreadex", 24},
        {"_time64", 4},
        {"_localtime64_s", 8},
        // newly added
        {"GetCurrentPackageId", 8},
        {"GetModuleHandleExW", 12},
        {"CommandLineToArgvW", 8},
        {"CorExitProcess", 4},
        {NULL, 0}
    };
    for (int i = 0; table[i].name; i++) {
        if (strcmp(name, table[i].name) == 0) return table[i].bytes;
    }
    // Default: assume 0 args (stub/dummy functions)
    printf("[WOW64] WARNING: Unknown arg bytes for '%s', defaulting to 0\n", name);
    return 0;
}

// pe_execute moved down
extern "C" void segfault_handler(int sig, siginfo_t *si, void *arg) {
    static volatile int in_handler = 0;
    ucontext_t *uc = (ucontext_t *)arg;
    uint64_t rip = uc->uc_mcontext.gregs[REG_RIP];
    if (sig == SIGTRAP) {
        dprintf(1, "[SIGTRAP] EIP: 0x%x\n", (uint32_t)rip);
        uc->uc_mcontext.gregs[REG_EFL] &= ~0x100; // Clear TF
        return;
    }

    // Prevent recursive crashes in signal handler
    if (in_handler) {
        // Already in handler - just longjmp out
        if (in_exe_thread) {
            in_handler = 0;
            siglongjmp(jump_env, 1);
        }
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
        return;
    }
    in_handler = 1;

    if (in_exe_thread) {
        char msg[256];
        extern char last_proc_name[128];
        if (rip == 0) {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x0. Probable cause: Missing function '%s'\n", last_proc_name);
        } else {
            snprintf(msg, sizeof(msg), "[SYS] EXE CRASHED AT RIP: 0x%llx. SIG: %d, ADDR: %p. SAFELY CAUGHT.\n", (unsigned long long)rip, sig, si->si_addr);
        }
        printf("%s", msg);

        if (rip != 0) {
            // Safe memory read: check if page is mapped using mincore
            uintptr_t page = rip & ~0xFFF;
            unsigned char vec[1];
            bool readable = (mincore((void*)page, 4096, vec) == 0);
            if (readable) {
                printf("[SYS] Bytes around RIP: ");
                uint8_t* p = (uint8_t*)rip;
                for (int i = 0; i < 32; i++) {
                    printf("%02X ", p[i]);
                }
                printf("\n");
            } else {
                printf("[SYS] Cannot read bytes at RIP (unmapped page 0x%lx)\n", page);
            }

            // Print registers
            printf("[SYS] RAX: 0x%llx, RCX: 0x%llx, RDX: 0x%llx, RBX: 0x%llx\n",
                uc->uc_mcontext.gregs[REG_RAX], uc->uc_mcontext.gregs[REG_RCX],
                uc->uc_mcontext.gregs[REG_RDX], uc->uc_mcontext.gregs[REG_RBX]);
            printf("[SYS] RSP: 0x%llx, RBP: 0x%llx, RSI: 0x%llx, RDI: 0x%llx\n",
                uc->uc_mcontext.gregs[REG_RSP], uc->uc_mcontext.gregs[REG_RBP],
                uc->uc_mcontext.gregs[REG_RSI], uc->uc_mcontext.gregs[REG_RDI]);

            // 32-bit EBP chain stack walk
            uint32_t ebp = (uint32_t)uc->uc_mcontext.gregs[REG_RBP];
            printf("[SYS] STACK WALK:\n");
            for (int frame = 0; frame < 8 && ebp > 0x400000 && ebp < 0x42000000; frame++) {
                uintptr_t ebp_page = ebp & ~0xFFF;
                if (mincore((void*)ebp_page, 4096, vec) != 0) break;
                uint32_t* fp = (uint32_t*)(uintptr_t)ebp;
                uint32_t ret_addr = fp[1];
                uint32_t next_ebp = fp[0];
                printf("  Frame %d: EBP=0x%x, RET=0x%x, ARGS=(0x%x, 0x%x, 0x%x, 0x%x)\n",
                    frame, ebp, ret_addr, fp[2], fp[3], fp[4], fp[5]);
                ebp = next_ebp;
            }
        }
        fflush(stdout);
        in_handler = 0;
        printf("[SYS] THREAD TERMINATED SAFELY.\n");
        siglongjmp(jump_env, 1);
    }
    in_handler = 0;
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}

extern "C" void jump_to_32bit(uint32_t entry_point, uint32_t stack32_ptr);
extern "C" void my_sig_wrapper();
extern void* meinos_alloc32(size_t size);

void* linux_exe_thread(void* arg) {
    uint32_t entry = (uint32_t)(uint64_t)arg;
    in_exe_thread = true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = (void(*)(int, siginfo_t*, void*))my_sig_wrapper;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;

    stack_t altstack;
    altstack.ss_sp = malloc(SIGSTKSZ * 4);
    altstack.ss_size = SIGSTKSZ * 4;
    altstack.ss_flags = 0;
    sigaltstack(&altstack, NULL);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);  // Catch illegal instructions too!
    sigaction(SIGFPE, &sa, NULL);  // Catch floating point exceptions
    sigaction(SIGABRT, &sa, NULL);

    // WINE Magic: Set up dummy TEB & PEB in lower 2GB
    void* teb = meinos_alloc32(4096);
    void* peb = meinos_alloc32(4096);
    *(uint32_t*)((char*)teb + 0x18) = (uint32_t)(uint64_t)teb; // 32-bit TEB->Self
    *(uint32_t*)((char*)teb + 0x30) = (uint32_t)(uint64_t)peb; // 32-bit TEB->PEB
    *(uint32_t*)((char*)peb + 0x08) = 0x400000; // ImageBase

    // 32-bit stack in lower 2GB
    void* stack = meinos_alloc32(1024 * 1024); // 1MB stack
    uint32_t stack_ptr = (uint32_t)(uint64_t)stack + 1024 * 1024 - 4096;

    // Save glibc FS_BASE
    extern uint64_t glibc_fs_base;
    syscall(SYS_arch_prctl, ARCH_GET_FS, &glibc_fs_base);

    // Pass TEB base to wow64_api_thunk so it can restore FS on signals
    extern uint64_t saved_fs_base;
    saved_fs_base = (uint64_t)teb;

    // Set up LDT entry 1 for TEB
    struct user_desc u_info;
    memset(&u_info, 0, sizeof(u_info));
    u_info.entry_number = 1;
    u_info.base_addr = (uint32_t)(uint64_t)teb;
    u_info.limit = 0xFFFFF;
    u_info.seg_32bit = 1;
    u_info.contents = 0; // Data
    u_info.read_exec_only = 0;
    u_info.limit_in_pages = 1;
    u_info.seg_not_present = 0;
    u_info.useable = 1;
    syscall(SYS_modify_ldt, 1, &u_info, sizeof(u_info));

    if (sigsetjmp(jump_env, 1) == 0) {
        jump_to_32bit(entry, stack_ptr);
    } else {
        printf("[SYS] THREAD TERMINATED SAFELY.\n");
    }
    in_exe_thread = false;
    return 0;
}
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#define Cos os2_Cos
#define DrawAeroCursor os2_DrawAeroCursor
#define DrawChar os2_DrawChar
#define DrawDenseGalaxy os2_DrawDenseGalaxy
#define DrawGlassRect os2_DrawGlassRect
#define DrawOrganicPlanet os2_DrawOrganicPlanet
#define DrawRoundedRect os2_DrawRoundedRect
#define Put os2_Put
#define PutAlpha os2_PutAlpha
#define Sin os2_Sin
#define Swap os2_Swap
#define Text os2_Text
#define TextC os2_TextC
#define app_window_active os2_app_window_active
#define bcd2bin os2_bcd2bin
#define byte_to_hex os2_byte_to_hex
#define cmd_status os2_cmd_status
#define cosmos_random os2_cosmos_random
#define cpu_brand os2_cpu_brand
#define debug_print os2_debug_print
#define drag_off_x os2_drag_off_x
#define drag_off_y os2_drag_off_y
#define drag_win os2_drag_win
#define fb os2_fb
#define focus_window os2_focus_window
#define folder_name_idx os2_folder_name_idx
#define frame os2_frame
#define galaxy_expansion os2_galaxy_expansion
#define galaxy_open os2_galaxy_open
#define get_cpu_brand os2_get_cpu_brand
#define handle_input os2_handle_input
#define hda_debug_c0_resp os2_hda_debug_c0_resp
#define heap_head os2_heap_head
#define hw_gpu os2_hw_gpu
#define hw_net os2_hw_net
#define hw_storage os2_hw_storage
#define hw_usb os2_hw_usb
#define init_mouse os2_init_mouse
#define input_cooldown os2_input_cooldown
#define int_sqrt os2_int_sqrt
#define is_over os2_is_over
#define is_over_rect os2_is_over_rect
#define key_scancode os2_key_scancode
#define mirror_count os2_mirror_count
#define mirror_list os2_mirror_list
#define mouse_read os2_mouse_read
#define mouse_right_down os2_mouse_right_down
#define mouse_wait os2_mouse_wait
#define mouse_write os2_mouse_write
#define new_folder_name os2_new_folder_name
#define planets os2_planets
#define read_rtc os2_read_rtc
#define resize_win os2_resize_win
#define rtc_day os2_rtc_day
#define rtc_h os2_rtc_h
#define rtc_m os2_rtc_m
#define rtc_mon os2_rtc_mon
#define rtc_year os2_rtc_year
#define save_filename os2_save_filename
#define save_name_idx os2_save_name_idx
#define save_step os2_save_step
#define stars os2_stars
#define str_cat os2_str_cat
#define str_cpy os2_str_cpy
#define str_equal os2_str_equal
#define str_len os2_str_len
#define str_starts os2_str_starts
#define sys_lang os2_sys_lang
#define sys_theme os2_sys_theme
#define system_reboot os2_system_reboot
#define system_shutdown os2_system_shutdown
#define system_ticks os2_system_ticks
#define user_name os2_user_name
#define win_z os2_win_z
#define windows os2_windows
#define z_blocked os2_z_blocked
#endif

// alloc32 moved down

#include "schneider_lang.h"
#include "cosmos_partition.h"
#include "arcade.h"
#include <stdint.h>

#include <stddef.h>

#include "cosmos_fat32.h"
#include "elf_loader.h"
#include "pe_loader.h"

static uint8_t* heap32_base = nullptr;
static uint32_t heap32_offset = 0;
static pthread_mutex_t heap32_mutex = PTHREAD_MUTEX_INITIALIZER;

void* meinos_alloc32(size_t size) {
    pthread_mutex_lock(&heap32_mutex);
    if (!heap32_base) {
        heap32_base = (uint8_t*)mmap(NULL, 1024*1024*500, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
    }
    size = (size + 15) & ~15;
    if (heap32_offset + size + 16 > 1024*1024*500) {
        pthread_mutex_unlock(&heap32_mutex);
        return nullptr;
    }
    void* ret = heap32_base + heap32_offset;
    *(size_t*)ret = size; // Store the original requested size (aligned)
    heap32_offset += size + 16;
    pthread_mutex_unlock(&heap32_mutex);
    return (uint8_t*)ret + 16;
}

void pe_execute(uint8_t* pe_data, uint32_t pe_sz) {
    uint64_t entry = load_and_resolve_pe(pe_data, pe_sz);
    if (entry) {
        printf("[SYS] EXE READY. STARTING NATIVE TASK...\n");
        extern void* linux_exe_thread(void* arg);
        linux_exe_thread((void*)entry);
    }
}

#ifdef __linux__
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include "cosmos_font.h"
#include <string.h>
#include <stdio.h>
#endif

char linux_file_paths[28][256];

/// CHIP-8 Emulator Variablen von "außen" bekannt machen
extern "C" void c8_reset();
extern unsigned char c8_ram[4096];
extern unsigned short c8_pc;
extern _43 c8_state; // (oder bool/int, je nachdem was _43 bei dir ist)

extern _44 browser_download_complete;
extern _184 browser_download_buffer[3000000];
extern _43 browser_download_len;
extern _43 browser_content_length;
extern _30 browser_url[512];
extern "C" _50 send_dns_query(_71 _30* domain);
extern _50 send_tcp_syn(_89 dest_ip, _182 dest_port);
extern _89 ip_str_to_u32(_71 _30* ip_str);
struct TCPSocket {
    _89 remote_ip;
    _182 local_port;
    _182 remote_port;
    _89 my_seq;
    _89 my_ack;
    _89 remote_seq;
    _43 state; 
};
extern TCPSocket browser_tcp;

_44 pkg_download_active = 0;
_44 pkg_download_execute = 0;
_89 pkg_target_ip = 0;
_182 pkg_target_port = 80;
void* pkg_output_win_ptr = 0;

/// ==========================================
/// BARE METAL FIX: EXTERNE LAUFWERKS-BRÜCKE
/// ==========================================
/// 1. Dem UI erklären, wie ein Laufwerk aussieht
_202 DriveInfo { _43 type; _43 size_mb; _43 base_port; _30 model[41]; };

/// 2. Die globalen Variablen aus den anderen Dateien anzapfen
_172 DriveInfo drives[8];
_172 _43 drive_count;
_172 _89 usb_io_base; 
extern uint64_t global_xhci_base_addr;

/// 3. Die Laufwerks-Treiber dem UI bekannt machen
_172 _44 ahci_read_sectors(_43 port_no, _43 lba, _43 count, _94 buffer_addr);
_172 _44 ahci_write_sectors(_43 port_no, _43 lba, _43 count, _94 buffer_addr);
_172 _44 usb_bot_read_sectors(_43 dev_addr, _43 ep_out, _43 ep_in, _89 lba, _43 num_sectors, _184* buffer);
_172 _43 usb_bot_get_capacity(_43 dev_addr, _43 ep_out, _43 ep_in);
_172 _44 usb_enumerate_device(_43 port_idx, _43 new_address);

/// 4. I/O Befehl für den USB-Scanner im UI
static inline _182 inw(_182 port) {
    return 0;
}
// --- GLOBALE BILDSCHIRM-VARIABLEN ---
extern uint32_t screen_w;
extern uint32_t screen_h;
uint32_t screen_pitch = 1920 * 4; 
uint32_t* fb = nullptr; // Dein echter Framebuffer
extern uint32_t* bb;

extern uint32_t cartridge_start;
extern uint32_t cartridge_end;

bool is_app_running = false;
// --- BARE METAL EXTERNS ---
// Sag der kernel_main.cpp, dass diese Variable in der cosmos_ahci.cpp existiert!
extern uint32_t global_ahci_abar;
struct HBA_PORT; 
extern HBA_PORT* get_active_ahci_port();
extern _44 ahci_read(HBA_PORT* port, _89 startlba, void* target_ram_address);
// disable_nx ist extern "C" - wir nutzen native 64-Bit Typen für saubere Pointer-Casts!
extern "C" void disable_nx_for_app(unsigned long long virtual_addr, unsigned long long size_in_bytes);
/// ==========================================
/// OS2 NATIVE 64-BIT NETWORK SCANNER
/// ==========================================
uint32_t os2_net_bar0 = 0; 
uint16_t os2_net_vendor = 0;
uint16_t os2_net_device = 0;

uint32_t os2_pci_read(uint32_t bus, uint32_t slot, uint32_t func, uint32_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    return 0;
}

extern void map_mmio_64(uint64_t phys_addr);
extern void intel_e1000_init(uint32_t mmio_addr, uint16_t device_id);

extern uint32_t active_ahci_bar5;
void os2_smart_scan() {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            for (uint32_t f = 0; f < 8; f++) {
                uint32_t vendor_device = os2_pci_read(bus, slot, f, 0);
                if (vendor_device != 0xFFFFFFFF) {
                    uint32_t class_reg = os2_pci_read(bus, slot, f, 0x08);
                    uint8_t class_code = (class_reg >> 24) & 0xFF;
                    
                    /// 0x02 = Network Controller!
                    if (class_code == 0x02) {
                        os2_net_vendor = vendor_device & 0xFFFF;
                        os2_net_device = vendor_device >> 16;
                        uint32_t bar0 = os2_pci_read(bus, slot, f, 0x10);
                        uint32_t clean_bar0 = bar0 & 0xFFFFFFF0;
                        if ((bar0 & 0x06) == 0x04) {
                            uint32_t bar1 = os2_pci_read(bus, slot, f, 0x14);
                            if (bar1 != 0) continue; /// Zu hoch fuer 32-bit Pointers!
                        }
                        if (clean_bar0 != 0 && os2_net_bar0 == 0) {
                            os2_net_bar0 = clean_bar0;
                            uint32_t pci_cmd = os2_pci_read(bus, slot, f, 0x04);
                            uint32_t pci_addr = 0x80000000 | (bus << 16) | (slot << 11) | (f << 8) | 0x04;
#ifndef __linux__
                            __asm__ volatile("outl %0, %w1" : : "a"(pci_addr), "Nd"((uint16_t)0xCF8));
                            __asm__ volatile("outl %0, %w1" : : "a"(pci_cmd | 0x06), "Nd"((uint16_t)0xCFC));
#endif
                            
                            map_mmio_64(os2_net_bar0);
                            
                            if (os2_net_vendor == 0x8086) {
                                intel_e1000_init(os2_net_bar0, os2_net_device);
                            }
                        }
                    } else if (class_code == 0x01 && ((class_reg >> 16) & 0xFF) == 0x06) {
                        /// AHCI Controller
                        uint32_t bar5 = os2_pci_read(bus, slot, f, 0x24) & 0xFFFFFFF0;
                        if(bar5 != 0 && active_ahci_bar5 == 0) {
                            active_ahci_bar5 = bar5;
                            global_ahci_abar = bar5;
                            map_mmio_64(bar5);
                            struct HBA_MEM { uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc; uint8_t rsv[0x70]; struct HBA_PORT { uint32_t clb, clbu, fb, fbu, is, ie, cmd, rsv1, tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs; } ports[32]; };
                            HBA_MEM* hba = (HBA_MEM*)bar5;
                            hba->ghc |= 0x80000000;
                            hba->ghc &= ~(1 << 1); 
                            for(int i=0; i<32; i++) {
                                if(hba->pi & (1 << i)) {
                                    uint32_t ssts = hba->ports[i].ssts;
                                    if((ssts & 0x0F) == 3 && ((ssts >> 8) & 0x0F) == 1) {
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
void internal_test_task() {
    while(1) {
        // Direkter Video-RAM Zugriff (0xB8000 ist fast immer gemappt)
        char* vga = (char*)0xB8000;
        vga[0] = 'T'; // 'T' für Task
        vga[1] = 0x0A; // Hellgrün
        
        // Pause, um die CPU nicht zu grillen
        for(volatile int i = 0; i < 1000000; i++) {}
        
        // Abgeben
        ; 
    }
}

/// ==========================================
/// APP ENGINES (FROM ARCADE.CPP)
/// ==========================================
extern _50 run_browser_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
extern _50 run_pong_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
extern _50 run_blobby_engine(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
extern _50 run_holyspirit(_43 wx, _43 wy, _43 ww, _43 wh, _44 blocked);
extern "C" void run_smash_cats_engine(int wx, int wy, int ww, int wh, bool is_blocked);
extern _50 browser_handle_keyboard(_43 key_scancode, _184 ascii);
_50 run_emulator_engine(_43, _43, _43, _43, _44);

/// ==========================================
/// BARE METAL FIX: GLOBALE HARDWARE-TIMER & SCHEDULER
/// ==========================================
volatile uint64_t system_ticks = 0; 
uint64_t last_window_click = 0;

/// HIER WIRD SIE ECHT DEFINIERT (Kein extern mehr!)
int task_quantum = 0; 

/// Freiwillige CPU-Abgabe
void yield() {
    task_quantum = 15; 
    
    /// BARE METAL FIX: sti schließt die Interrupts wieder auf!
    /// So kann der Hardware-Timer uns wecken, auch wenn wir aus 
    /// einem System-Call kommen!
    ; 
}

/// ==========================================
/// DER WORKER TASK
/// ==========================================
void dynamic_task_worker() {
    while(1) {
        /// Dieser Task wird per Klick zur Laufzeit in den RAM geladen!
        volatile int z = 0;
        z++;
        yield(); 
    }
}
/// ==========================================
/// BARE METAL FIX: 64-BIT TASK SCHEDULER & STACKS
/// ==========================================
struct Task {
    uint64_t rsp;          /// 64-Bit Pointer für den Stack
    uint8_t stack[8192];   /// 8 KB eigener Arbeitsspeicher pro Task
    bool active;
    bool paused;           /// BARE METAL FIX: Pause-Flag
};

Task tasks[4];             
int current_task = 0;      
int num_tasks = 1;         

// BARE METAL FIX: Eine Struktur, die exakt den Stack abbildet,
// wie ihn deine pit_isr vor dem iretq erwartet!
// BARE METAL FIX: Die 16 Bytes, die den Triple Fault verursacht haben!
#ifdef __linux__
static uint64_t get_ram_addr_raw() { static uint8_t* p = new uint8_t[32*1024*1024]; return (uint64_t)p; }
static uint64_t get_ram_addr_bmp() { static uint8_t* p = new uint8_t[32*1024*1024]; return (uint64_t)p; }
static uint64_t get_ram_addr_wav() { static uint8_t* p = new uint8_t[32*1024*1024]; return (uint64_t)p; }
static uint64_t get_ram_addr_text() { static uint8_t* p = new uint8_t[32*1024*1024]; return (uint64_t)p; }
static uint64_t get_ram_addr_copy() { static uint8_t* p = new uint8_t[32*1024*1024]; return (uint64_t)p; }
#else
#define get_ram_addr_raw() 0x0E000000ULL
#define get_ram_addr_bmp() 0x0C000000ULL
#define get_ram_addr_wav() 0x0B500000ULL
#define get_ram_addr_text() 0x03000000ULL
#define get_ram_addr_copy() 0x09000000ULL
#endif
struct InterruptFrame {
    // Exakt die 15 Register deiner Naked-ISR
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    
    // Die 5 Register, die iretq frisst
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));
void sys_idle_task() {
    while(1) {
        // Hier kannst du Dinge tun, die immer laufen sollen,
        // auch wenn keine App geladen ist.
        // Wenn du nichts tun willst:
        ; // CPU schlafen legen, bis ein Interrupt kommt
    }
}

void create_task(void (*entry_point)()) {
    // 1. Timer anhalten, Lebensgefahr!
    ; 
    
    int id = -1;
    for (int i = 1; i < 4; i++) {
        if (!tasks[i].active) {
            id = i;
            break;
        }
    }
    if (id == -1 && num_tasks < 4) {
        id = num_tasks;
    }
    if (id == -1) {
        ;
        return;
    }
    
    // ==========================================
    // BARE METAL FIX: DER KUGELSICHERE STACK
    // Keine .bss Arrays mehr! Wir nutzen 18 MB (0x01200000)
    // Jeder Task bekommt 1 volles Megabyte Abstand.
    // ==========================================
    uint64_t stack_base = 0x01200000 + (id * 0x100000); 
    stack_base &= ~0xF; // Perfektes 16-Byte Alignment
    
    InterruptFrame* frame = (InterruptFrame*)(stack_base - sizeof(InterruptFrame));
    
    // Alles mit Nullen füllen, um Müll aus dem RAM zu entfernen
    uint8_t* frame_ptr = (uint8_t*)frame;
    for (uint32_t i = 0; i < sizeof(InterruptFrame); i++) {
        frame_ptr[i] = 0;
    }
    
    // 64-Bit Variablen für die Segment-Register (verhindert 16-Bit Müll!)
    uint64_t current_cs = 0, current_ss = 0;
    ;
    ;
    
    // Frame befüllen
    frame->rip = (uint64_t)entry_point;
    frame->cs = current_cs;        
    frame->ss = current_ss;        
    
    // BARE METAL FIX: Absolut saubere Flags erzwingen! (IF=1, Reserved=1)
    // Keine zufälligen Bits mehr aus dem Kernel übernehmen.
    frame->rflags = 0x0202;  
    
    // Den Stack-Pointer für das C++ ABI auf -8 ausrichten
    frame->rsp = stack_base - 8; 
    
    // Task eintragen und scharf schalten
    tasks[id].rsp = (uint64_t)frame;
    tasks[id].active = true;
    tasks[id].paused = false;
    if (id >= num_tasks) num_tasks = id + 1;
    
    // 2. Timer wieder an! Auf in die Schlacht!
    ; 
}

extern "C" uint64_t schedule(uint64_t old_rsp) {
    system_ticks++; 
    
    tasks[current_task].rsp = old_rsp;
    
    // Nächsten aktiven und NICHT pausierten Task suchen!
    for (int i = 0; i < num_tasks; i++) {
        current_task++;
        if (current_task >= num_tasks) current_task = 0;
        if (tasks[current_task].active && !tasks[current_task].paused) {
            break; // Gefunden!
        }
    }
    
    // Falls alle pausiert sind (was nicht passieren sollte, da Task 0 (OS) nie pausiert),
    // landen wir sicher wieder bei Task 0 oder dem letzten laufenden Task.
    
    return tasks[current_task].rsp;
}

/// ==========================================
/// BARE METAL FIX: 64-BIT NAKED PIT ISR
/// ==========================================
__attribute__((naked)) void pit_isr() {
    ;
}
/// ==========================================
/// BARE METAL FIX: SAVE AS & FOLDER GLOBALS
/// ==========================================
extern void run_cosmos_script(char* file_buffer, int file_size);
extern char term_buffer[15][64];
int save_step = 0; 
char save_filename[32] = "NEWFILE";
int save_as_mode = 0; /// 0 = Notepad, 1 = Browser Download 
int save_name_idx = 7; 
char new_folder_name[32] = "NEWDIR"; 
int folder_name_idx = 6;
/// ==========================================
/// BARE METAL FIX: USER SPACE RAM (64 KB pro Task)
/// ==========================================
/// aligned(4096) sorgt dafür, dass die Programme sauber auf RAM-Seiten-Grenzen liegen!
uint8_t user_programs[4][65536] __attribute__((aligned(4096)));
/// ==========================================
/// BARE METAL TOOL: STRING CONCAT (DER KLEBER)
/// ==========================================
void str_cat(char* dest, const char* src) {
    /// 1. Gehe zum Ende des ersten Textes (bis zur Null)
    while (*dest) {
        dest++;
    }
    /// 2. Kopiere den zweiten Text genau dort hin
    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    /// 3. Setze am Ende die neue Null-Terminierung
    *dest = 0;
}
/// ==========================================
/// BARE METAL FIX: LOKALER PCI-READER (BYPASS LINKER)
/// ==========================================
uint32_t pci_read(uint32_t bus, uint32_t slot, uint32_t func, uint32_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    
    /// Direkter CPU-Hardware-Zugriff (Inline Assembler) -> Braucht keine externen Dateien!
    ;
    return 0;
}
struct MirrorEntry {
    uint32_t bus, dev, func;
    uint32_t vendor, device;
    uint64_t bar0;
    char name[32];
};
MirrorEntry mirror_list[32];
int mirror_count = 0;
bool show_oracle = false;

extern _50 pci_scan_all();
extern _44 xhci_bot_read_sectors(_184 slot_id, _43 lba, _89 dest_ram);
extern void xhci_poll_events_and_mouse();
extern void (*usb_mouse_callback)(int, int, int);
extern "C" _50 check_incoming();
extern "C" _50 net_check_link();
extern "C" _50 send_arp_ping();


/// ==========================================
/// BARE METAL FIX: DER UNIVERSAL-LESE-ADAPTER (FINAL)
/// ==========================================
extern int selected_drive_idx; 
extern int ahci_read_sectors(uint32_t lba, uint64_t dest_ram); 
extern _44 xhci_bot_read_sectors(_184 slot_id, uint32_t lba, uint64_t dest_ram);
extern char linux_dev_paths[8][48];

/// ==========================================
/// BARE METAL FIX: DER UNIVERSELLE DISK-ROUTER
/// ==========================================
struct CFS_DIR_ENTRY { 
    uint8_t type;          /// 1 Byte: 0 = Leer, 1 = Datei, 2 = Ordner
    char filename[11];     /// 11 Byte: Name (8.3 Format)
    uint16_t start_lba;    /// 2 Byte: Start Sektor
    uint16_t file_size;    /// 2 Byte: Dateigröße in KB
    uint8_t parent_idx;    /// 1 Byte: NEU! Wem gehört diese Datei? (255 = Root)
    uint8_t reserved;      /// 1 Byte: Padding, um auf glatte 18 Bytes zu kommen (Alignment)
} __attribute__((packed)); /// Exakt 18 Bytes pro Eintrag!
int simulator_page_offset = 0;
int global_cfs_page_offset = 0;

bool disk_read_auto(uint32_t lba, uint64_t target_ram_addr) {
#ifdef __linux__
    if (selected_drive_idx == -1) {
        if (lba == 1002) {
        /// DIRECTORY READ (CFS LBA 1002)
        CFS_DIR_ENTRY* entries = (CFS_DIR_ENTRY*)target_ram_addr;
        for (int i=0; i<28; i++) entries[i].type = 0;
        
        const char* path = "/opt/meinos/virtual_hdd";
        if (selected_drive_idx == 1) path = "/media";
        
        DIR* dir = opendir(path);
        if (dir) {
            int slot = 0;
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL && slot < 28) {
                if (ent->d_name[0] == '.') continue;
                int len = 0; while(ent->d_name[len]) len++;
                
                // FILTER HACK: Hide Linux build files to leave room for Windows EXEs
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 'c' && ent->d_name[len-2] == 'p' && ent->d_name[len-1] == 'p') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'o') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'h') continue;
                if (len > 3 && ent->d_name[len-3] == '.' && ent->d_name[len-2] == 's' && ent->d_name[len-1] == 'h') continue;
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 't' && ent->d_name[len-2] == 'x' && ent->d_name[len-1] == 't') continue;
                if (len > 4 && ent->d_name[len-4] == '.' && ent->d_name[len-3] == 'i' && ent->d_name[len-2] == 'm' && ent->d_name[len-1] == 'g') continue;
                if (len > 2 && ent->d_name[len-2] == '.' && ent->d_name[len-1] == 'l') continue;
                
                if (simulator_page_offset > 0) { 
                    simulator_page_offset--; 
                    continue; 
                }
                if (ent->d_type != DT_DIR) {
                    int has_ext = 0;
                    for (int k=0; k<len; k++) if (ent->d_name[k] == '.') has_ext = 1;
                    if (!has_ext) continue;
                }
                
                entries[slot].type = (ent->d_type == DT_DIR) ? 2 : 1;
                
                if (len > 4 && ent->d_name[len-4] == '.') {
                    strncpy((char*)entries[slot].filename, ent->d_name, 7);
                    entries[slot].filename[7] = '.';
                    entries[slot].filename[8] = ent->d_name[len-3];
                    entries[slot].filename[9] = ent->d_name[len-2];
                    entries[slot].filename[10] = ent->d_name[len-1];
                } else {
                    strncpy((char*)entries[slot].filename, ent->d_name, 10);
                    entries[slot].filename[10] = 0;
                }
                entries[slot].start_lba = 1000000 + slot; 
                
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);
                strncpy(linux_file_paths[slot], full_path, 255);
                linux_file_paths[slot][255] = 0;
                
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    entries[slot].file_size = st.st_size;
                } else {
                    entries[slot].file_size = 0;
                }
                slot++;
            }
            closedir(dir);
        }
        return true;
    } else if (lba >= 1000000 && lba < 1000028) {
        /// FILE READ (Mapped via LBA)
        int slot = lba - 1000000;
        FILE* f = fopen(linux_file_paths[slot], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            fread((void*)target_ram_addr, 1, fsize, f);
            fclose(f);
        }
        return true;
    }
    } // <-- MISSING BRACE
    
    /// RAW SECTOR READ from real block device (/dev/sdX)
    /// This handles MBR reads (LBA 0), partition tables, NTFS/FAT32 VBR etc.
    if (selected_drive_idx >= 0 && selected_drive_idx < 8 && linux_dev_paths[selected_drive_idx][0] != 0) {
        FILE* f = fopen(linux_dev_paths[selected_drive_idx], "rb");
        if (f) {
            off_t offset = (off_t)lba * 512;
            fseeko(f, offset, SEEK_SET);
            size_t read_bytes = fread((void*)target_ram_addr, 1, 512, f);
            fclose(f);
            if (read_bytes == 512) return true;
        }
    }
    return true;
#else
    _15(selected_drive_idx EQ -1) _96 _86;
    
    _43 type = drives[selected_drive_idx].type;
    _43 port = drives[selected_drive_idx].base_port;
    
    /// FALL 1: SATA / AHCI FESTPLATTE
    _15(type EQ 2) {
        /// HIER rufen wir den echten AHCI-Treiber mit 4 Argumenten auf!
        _96 ahci_read_sectors(port, lba, 1, target_ram_addr);
    }
    
    /// FALL 2: xHCI USB MASS STORAGE (Typ 3)
    _15(type EQ 3) {
        /// port ist jetzt slot_id (1, 2, ...)
        _96 xhci_bot_read_sectors((uint8_t)port, (uint32_t)lba, (uint64_t)target_ram_addr);
    }
    
    /// FALL 3: UHCI USB MASS STORAGE (Typ 4)
    _15(type EQ 4) {
        _96 usb_bot_read_sectors(port, 1, 0x82, lba, 1, (uint8_t*)target_ram_addr);
    }
    
    /// BARE METAL FIX: FALL 4: CARTRIDGE RAMDISK (Typ 5)
    _15(type EQ 5) {
        if (cartridge_start > 0) {
            uint8_t* src = (uint8_t*)(cartridge_start + (lba * 512));
            uint8_t* dst = (uint8_t*)target_ram_addr;
            for(int i=0; i<512; i++) {
                dst[i] = src[i];
            }
            _96 _128;
        }
    }
    
    _96 _86;
#endif
}

/// ==========================================
/// BARE METAL FIX: DER UNIVERSELLE WRITE-ROUTER
/// ==========================================
extern _44 xhci_bot_write_sectors(_184 slot_id, uint32_t lba, uint64_t dest_ram);

bool disk_write_auto(uint32_t lba, uint64_t source_ram_addr) {
#ifdef __linux__
    if (selected_drive_idx == -1) {
        if (lba == 1002) {
        /// Directory table write - save to /opt/meinos/.cosmos_dir
        FILE* f = fopen("/opt/meinos/.cosmos_dir", "wb");
        if (f) {
            fwrite((void*)source_ram_addr, 1, 512, f);
            fclose(f);
        }
        return true;
    } else if (lba >= 1000000 && lba < 1000028) {
        /// File data write - write to the mapped Linux file path
        int slot = lba - 1000000;
        if (linux_file_paths[slot][0] != 0) {
            FILE* f = fopen(linux_file_paths[slot], "wb");
            if (f) {
                fwrite((void*)source_ram_addr, 1, 512, f);
                fclose(f);
            }
        }
        return true;
    }
    } // <-- MISSING BRACE
    /// RAW SECTOR WRITE to real block device (/dev/sdX)
    if (selected_drive_idx >= 0 && selected_drive_idx < 8 && linux_dev_paths[selected_drive_idx][0] != 0) {
        FILE* f = fopen(linux_dev_paths[selected_drive_idx], "r+b");
        if (f) {
            off_t offset = (off_t)lba * 512;
            fseeko(f, offset, SEEK_SET);
            fwrite((void*)source_ram_addr, 1, 512, f);
            fclose(f);
            return true;
        }
    }
    /// Fallback: write to /opt/meinos/.cosmos_sectors/
    {
        char sector_path[256];
        snprintf(sector_path, sizeof(sector_path), "/opt/meinos/.cosmos_sectors/lba_%u.bin", lba);
        mkdir("/opt/meinos/.cosmos_sectors", 0755);
        FILE* f = fopen(sector_path, "wb");
        if (f) {
            fwrite((void*)source_ram_addr, 1, 512, f);
            fclose(f);
        }
    }
    return true;

#else
    _15(selected_drive_idx EQ -1) _96 _86;
    _43 type = drives[selected_drive_idx].type;
    _43 port = drives[selected_drive_idx].base_port;
    
    _15(type EQ 2) {
        /// SATA schreiben!
        _96 ahci_write_sectors(port, lba, 1, source_ram_addr);
    }
    
    /// FALL 2: xHCI USB MASS STORAGE (Typ 3)
    _15(type EQ 3) {
        /// port ist jetzt slot_id!
        _96 xhci_bot_write_sectors((uint8_t)port, (uint32_t)lba, (uint64_t)source_ram_addr);
    }
    
    /// FALL 3: UHCI USB MASS STORAGE (Typ 4)
    _15(type EQ 4) {
        /// UHCI! Wir haben noch kein usb_bot_write_sectors
        _96 _86;
    }
    
    /// BARE METAL FIX: FALL 4: CARTRIDGE RAMDISK (Typ 5)
    _15(type EQ 5) {
        if (cartridge_start > 0) {
            uint8_t* dst = (uint8_t*)(cartridge_start + (lba * 512));
            uint8_t* src = (uint8_t*)source_ram_addr;
            for(int i=0; i<512; i++) {
                dst[i] = src[i];
            }
            _96 _128;
        }
    }
    
    _96 _86;
#endif
}

/// ==========================================
/// BARE METAL FIX: PCI SCANNER FÜR USB (UHCI)
/// ==========================================
static inline void outl(uint16_t port, uint32_t val) { ; }
static inline uint32_t inl(uint16_t port) { return 0; }
static inline void outw(uint16_t port, uint16_t val) { ; }
/// ==========================================
/// BARE METAL DOWNLOAD SAVER
/// ==========================================

/// ==========================================
/// BARE METAL FIX: SMART xHCI (USB 3.0) SCANNER
/// ==========================================
extern int init_xhci_probe(uint64_t base_addr, int id); 

void find_and_init_usb() {
    extern char cmd_status[256]; 
    extern void str_cpy(char* d, const char* s);
    usb_io_base = 0; // Reset

    for(int bus = 0; bus < 256; bus++) { 
        for(int slot = 0; slot < 32; slot++) {
            for(int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_read(bus, slot, func, 0);
                
                if ((vendor_device & 0xFFFF) != 0xFFFF) {
                    uint32_t class_info = pci_read(bus, slot, func, 0x08);
                    uint8_t class_code = (class_info >> 24) & 0xFF;
                    uint8_t subclass   = (class_info >> 16) & 0xFF;
                    uint8_t prog_if    = (class_info >>  8) & 0xFF;
                    
                    if (class_code == 0x0C && subclass == 0x03) {
                        
                        /// xHCI (USB 3.0) - MMIO, handled separately by init_xhci_probe
                        if (prog_if == 0x30) {
                            uint32_t bar0 = pci_read(bus, slot, func, 0x10);
                            uint32_t clean_bar0 = bar0 & 0xFFFFFFF0;
                            uint32_t bar1 = pci_read(bus, slot, func, 0x14);
                            
                            uint64_t full_bar = clean_bar0;
                            if ((bar0 & 0x04) != 0) {
                                full_bar = ((uint64_t)bar1 << 32) | clean_bar0;
                            }
                            
                            str_cpy(cmd_status, "xHCI (USB 3.0) FOUND!");
                            init_xhci_probe(full_bar, 1);
                            /// Keep scanning for UHCI too
                        }
                        /// UHCI (USB 1.1) - Port I/O via BAR4
                        else if (prog_if == 0x00) {
                            /// Enable Bus Mastering + I/O space
                            uint32_t pci_cmd_addr = 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | 0x04;
                            outl(0xCF8, pci_cmd_addr);
                            uint32_t pci_cmd = inl(0xCFC);
                            outl(0xCF8, pci_cmd_addr);
                            outl(0xCFC, pci_cmd | 0x05); /// I/O enable + Bus Master
                            
                            /// BAR4 is the I/O base for UHCI
                            uint32_t bar4 = pci_read(bus, slot, func, 0x20);
                            _15(bar4 & 0x01) { /// Bit 0 = 1 means I/O space BAR
                                usb_io_base = bar4 & 0xFFFC; /// Mask off the I/O indicator bits
                                str_cpy(cmd_status, "UHCI (USB 1.1) FOUND!");
                            }
                        }
                        /// EHCI (USB 2.0)
                        _41 _15(prog_if == 0x20) {
                            uint32_t bar0 = pci_read(bus, slot, func, 0x10);
                            extern uint64_t global_ehci_base_addr;
                            global_ehci_base_addr = bar0 & 0xFFFFFFF0;
                            
                            
                            str_cpy(cmd_status, "EHCI (USB 2.0) FOUND!");
                        }
                    }
                }
            }
        }
    }
}

/// Der Retter-Wrapper für alte Speicher-Befehle im CFS:
int ahci_write_sectors(uint32_t lba, uint64_t buffer_addr) {
    return disk_write_auto(lba, buffer_addr);
}

/// ==========================================
/// BARE METAL FIX: DER RETTER-WRAPPER (C++ OVERLOADING)
/// ==========================================
/// Fängt alle alten Aufrufe aus main() und process_cmd() ab
/// und leitet sie vollautomatisch an den neuen Router weiter!
int ahci_read_sectors(uint32_t lba, uint64_t buffer_addr) {
    return disk_read_auto(lba, buffer_addr);
}
/// ==========================================
/// BARE METAL FIX: DER UNIVERSAL-SCHREIB-ADAPTER
/// ==========================================
extern int ahci_write_sectors(uint32_t lba, uint64_t src_ram); 
extern _44 xhci_bot_write_sectors(_184 slot_id, uint32_t lba, uint64_t src_ram);

/// BARE METAL FIX: Den RAM für Texte restlos reinigen!
void mem_set(void* ptr, uint8_t value, uint32_t num) {
    uint8_t* p = (uint8_t*)ptr;
    while (num--) *p++ = value;
}
extern "C" _50 ahci_mount_drive();
extern "C" _50 usb_scan_and_mount();
extern _50 ahci_read_mbr();
extern "C" void play_hda_wav(uint32_t pcm_addr, uint32_t size_bytes, uint16_t sample_rate, uint16_t channels, uint16_t bits);
/// ==========================================
/// 1. HEAP ALLOCATOR (64-BIT GEFIXT)
/// ==========================================
#ifndef __linux__
MemoryBlock* heap_head = nullptr;
void init_heap() { heap_head = (MemoryBlock*)0x03000000; heap_head->size = 1024 * 1024 * 32; heap_head->is_free = 1; heap_head->next = nullptr; }
void* malloc(size_t size) {
    size = (size + 7) & ~7;
    MemoryBlock* curr = heap_head;
    while (curr != nullptr) {
        if (curr->is_free == 1 && curr->size >= size) {
            if (curr->size > size + sizeof(MemoryBlock) + 32) {
                MemoryBlock* new_block = (MemoryBlock*)((uint8_t*)curr + sizeof(MemoryBlock) + size);
                new_block->is_free = 1;
                new_block->size = curr->size - size - sizeof(MemoryBlock);
                new_block->next = curr->next;
                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            return (void*)((uint8_t*)curr + sizeof(MemoryBlock));
        }
        curr = curr->next;
    }
    return nullptr;
}
void free(void* ptr) { if (ptr == nullptr) return; MemoryBlock* block = (MemoryBlock*)((uint8_t*)ptr - sizeof(MemoryBlock)); block->is_free = 1; }
void* operator new(size_t size) { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete[](void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t size) noexcept { free(ptr); }
#endif
/// ==========================================
/// 2. BARE METAL PORTS & MOUSE DRIVER
/// ==========================================
inline uint8_t inb(uint16_t port) { return 0; }
inline void outb(uint16_t port, uint8_t val) { ; }
_50 mouse_wait(_184 type) { _89 t = 100000; _114(t--) { _15(type EQ 0 AND (inb(0x64)&1)) _96; _15(type EQ 1 AND !(inb(0x64)&2)) _96; } }
_50 mouse_write(_184 w) { mouse_wait(1); outb(0x64, 0xD4); mouse_wait(1); outb(0x60, w); }
_184 mouse_read() { mouse_wait(0); _96 inb(0x60); }
_50 init_mouse() { mouse_wait(1); outb(0x64,0xA8); mouse_wait(1); outb(0x64,0x20); mouse_wait(0); _184 s=inb(0x60)|2; mouse_wait(1); outb(0x64,0x60); mouse_wait(1); outb(0x60,s); mouse_write(0xF6); mouse_read(); mouse_write(0xF4); mouse_read(); }
/// ==========================================
/// 3. REAL-TIME CLOCK (RTC) BARE METAL
/// ==========================================
_43 rtc_h, rtc_m, rtc_day, rtc_mon, rtc_year;
_184 bcd2bin(_184 b) { _96 ((b >> 4) * 10) + (b & 0xF); }
_50 read_rtc() { 
    outb(0x70, 4); rtc_h = bcd2bin(inb(0x71)); 
    outb(0x70, 2); rtc_m = bcd2bin(inb(0x71)); 
    outb(0x70, 7); rtc_day = bcd2bin(inb(0x71)); 
    outb(0x70, 8); rtc_mon = bcd2bin(inb(0x71)); 
    outb(0x70, 9); rtc_year = bcd2bin(inb(0x71)); 
    rtc_h = (rtc_h + 1) % 24; /// Zeitzonen-Korrektur (CET)
}
/// 4. ENGINE GLOBALS & DATA STRUCTURES
/// ==========================================
uint32_t frame = 0;
extern _43 mouse_x; extern _43 mouse_y; extern _44 mouse_down; extern _44 mouse_just_pressed;
extern _44 key_new;
bool mouse_right_down = false; // Speichert, ob rechts gedrückt ist
int mouse_sub_x = 40000;
int mouse_sub_y = 30000;
/// Deine einstellbare Sensitivität! 
/// 10 = 1.0 (Normal) | 50 = 5 (Halb) | 100 = 10 (Extrem langsam)
int mouse_sens = 120; /// <-- Versuch es mal mit 30 (0.3) für die Diva!
/// ==========================================
/// BARE METAL FIX: DIE USB-MAUS SCHNITTSTELLE
/// ==========================================
void update_mouse_position(int dx, int dy, int btn) {
    
    /// BARE METAL FIX: Der USB-Werte-Wrap!
    /// Wir zwingen die 0-255 Werte hart in den negativen Bereich (-128 bis +127)
    int real_dx = (signed char)dx;
    int real_dy = (signed char)dy;
    
    /// 1. Die echte Mausbewegung mit Sensitivität multiplizieren
    mouse_sub_x += (real_dx * mouse_sens);
    mouse_sub_y += (real_dy * mouse_sens);
    
    /// 2. Den echten Bildschirm-Pixel berechnen
    int new_x = mouse_sub_x / 100;
    int new_y = mouse_sub_y / 100;

    /// 3. Achsen abriegeln UND Sub-Pixel-Konto zurücksetzen!
    if (new_x < 0) { new_x = 0; mouse_sub_x = 0; }
    if (new_x > 799) { new_x = 799; mouse_sub_x = 799 * 100; }
    
    if (new_y < 0) { new_y = 0; mouse_sub_y = 0; }
    if (new_y > 599) { new_y = 599; mouse_sub_y = 599 * 100; }

    /// 4. An dein System übergeben
    mouse_x = new_x;
    mouse_y = new_y;

    /// 5. Klick-Logik (Links)
    if (btn & 1) { 
        if (!mouse_down) mouse_just_pressed = _128; 
        else mouse_just_pressed = _86;
        mouse_down = _128;
    } else {
        mouse_down = _86;
        mouse_just_pressed = _86;
    }

    // ==========================================
    // BARE METAL FIX: RECHTSKLICK AUSLESEN!
    // ==========================================
    if (btn & 2) {
        mouse_right_down = true;
    } else {
        mouse_right_down = false;
    }
}
_72 _184 m_packet[3]; _72 _43 m_ptr = 0;
_44 galaxy_open = _86; _43 galaxy_expansion = 0;
_43 input_cooldown = 0; _44 click_consumed = _86;
struct Control {
    int type; // 0=none, 1=button, 2=static, 3=progress, 4=combobox
    int x, y, w, h;
    char text[64];
    int id;
    int state;
    int data;
    uint64_t wndproc;
    uint64_t user_data;
    char items[10][32];
    int num_items;
    int selected_item;
    int is_dropped;
};
struct Window { _43 id; _30 title[16]; _43 x, y, w, h; _44 open, minimized, fullscreen; _89 color; _30 content[2048]; _43 cursor_pos; uint32_t* pixels; int pixel_w; int pixel_h; uint64_t wndproc; _44 needs_paint; Control controls[32]; int num_controls; uint64_t user_data; uint32_t msg_msg; uint64_t msg_wparam; uint64_t msg_lparam; };
struct Planet { _43 ang; _43 dist; _30 name[8]; float cur_x, cur_y; float real_ang; }; 
struct Star { _43 x, y, z, type, speed; };
_43 win_z[20] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
Window windows[20]; 

/// ==========================================
/// BARE METAL FIX: GLOBAL DEBUG POPUP
/// ==========================================
_50 focus_window(_43 id);
extern _50 str_cpy(_30* d, _71 _30* s);
_50 debug_print(const char* msg) {
    _15(!windows[7].open) {
        windows[7].open = _128;
        windows[7].minimized = _86;
        str_cpy(windows[7].title, "UNIVERSAL APP INSTALLER");
        windows[7].x = 200; windows[7].y = 10;
        windows[7].w = 500; windows[7].h = 300;
        windows[7].color = 0x550000;
        windows[7].cursor_pos = 0;
        for(int i=0; i<2048; i++) windows[7].content[i] = 0;
        focus_window(7);
    }
    
    int len = 0;
    while(windows[7].content[len] && len < 4000) len++;
    
    // Auto-Scroll Reset
    if(len > 1800) {
        len = 0;
        for(int i=0; i<2048; i++) windows[7].content[i] = 0;
    }
    
    int m = 0;
    while(msg[m] && len < 2000) {
        char c = msg[m++];
        if (c >= 'a' && c <= 'z') c = c - 32; // Uppercase
        if (c == '=') c = ':';
        windows[7].content[len++] = c;
    }
    windows[7].content[len] = '\n';
    windows[7].content[len+1] = 0;
    windows[7].cursor_pos = len + 1;
}

Planet planets[6];
Star stars[200];
_44 cfg_planets_on = _128;
_44 cfg_galaxy_on = _128;
_44 cfg_stars_on = _128;
_43 cfg_star_count = 200;
_43 cfg_speed = 1;
_43 cfg_target_fps = 0;
uint32_t current_sound_freq = 0;
bool cfg_sound_surreal = true;
_43 drag_win = -1; _43 drag_off_x = 0; _43 drag_off_y = 0; _43 resize_win = -1; _44 z_blocked = _86;
_72 _89 rng_seed = 123456789;
_89 cosmos_random() { rng_seed = (rng_seed * 1103515245 + 12345) & 0x7FFFFFFF; _96 rng_seed; }
_43 int_sqrt(_43 n) { _43 x=n, y=1; _114(x>y){x=(x+y)/2; y=n/x;} _96 x; }
_71 _43 sin_lut[256] = { 1, 2, 4, 7, 9, 12, 14, 17, 19, 21, 24, 26, 28, 30, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 56, 58, 60, 61, 63, 64, 66, 67, 68, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 79, 80, 81, 81, 82, 82, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 82, 82, 81, 81, 80, 79, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 68, 67, 66, 64, 63, 61, 60, 58, 56, 55, 53, 51, 49, 47, 45, 43, 41, 39, 37, 35, 33, 30, 28, 26, 24, 21, 19, 17, 14, 12, 9, 7, 4, 2, 1, -1, -2, -4, -7, -9, -12, -14, -17, -19, -21, -24, -26, -28, -30, -33, -35, -37, -39, -41, -43, -45, -47, -49, -51, -53, -55, -56, -58, -60, -61, -63, -64, -66, -67, -68, -70, -71, -72, -73, -74, -75, -76, -77, -78, -79, -79, -80, -81, -81, -82, -82, -83, -83, -83, -84, -84, -84, -84, -84, -84, -84, -83, -83, -83, -82, -82, -81, -81, -80, -79, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -68, -67, -66, -64, -63, -61, -60, -58, -56, -55, -53, -51, -49, -47, -45, -43, -41, -39, -37, -35, -33, -30, -28, -26, -24, -21, -19, -17, -14, -12, -9, -7, -4, -2, -1 };
_43 Cos(_43 a) { _96 sin_lut[(a + 64) % 256]; }
_43 Sin(_43 a) { _96 sin_lut[a % 256]; }
_50 str_cpy(_30* d, _71 _30* s) { _114(*s) *d++ = *s++; *d=0; }
_43 str_len(_71 _30* s) { _43 l=0; _114(*s++)l++; _96 l; }
_44 is_over(_43 mx, _43 my, _43 ox, _43 oy, _43 r) { _96 (mx-ox)*(mx-ox) + (my-oy)*(my-oy) < r*r; }
_44 is_over_rect(_43 mx, _43 my, _43 x, _43 y, _43 w, _43 h) { _96 (mx >= x AND mx <= x+w AND my >= y AND my <= y+h); }
/// ==========================================
/// BARE METAL FIX: HEX-KONVERTER FÜR DEBUGGING
/// ==========================================
void byte_to_hex(unsigned char byte, char* str) {
    const char* hex_digits = "0123456789ABCDEF";
    str[0] = hex_digits[(byte >> 4) & 0x0F];
    str[1] = hex_digits[byte & 0x0F];
    str[2] = 0; /// Null-Terminierung für den String
}
/// TEXT ENGINE HELPER FÜR FENSTER
bool str_equal(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (*(const unsigned char*)s1 == *(const unsigned char*)s2);
}
bool str_starts(const char* full, const char* prefix) {
    while(*prefix) { if(*prefix++ != *full++) return false; }
    return true;
}
void print_win(Window* win, const char* text) {
#ifdef __linux__
    printf("%s", text);
    fflush(stdout);
#endif
    while (*text) {
        if (win->cursor_pos < 2040) win->content[win->cursor_pos++] = *text;
        text++;
    }
    win->content[win->cursor_pos] = 0;

    int lines = 0;
    for(int i=0; i<win->cursor_pos; i++) { if (win->content[i] == '\n') lines++; }

    while (lines > 28) {
        int first_nl = 0;
        while(first_nl < win->cursor_pos && win->content[first_nl] != '\n') first_nl++;
        if (win->content[first_nl] == '\n') first_nl++;
        int new_len = win->cursor_pos - first_nl;
        for(int i=0; i<new_len; i++) win->content[i] = win->content[first_nl + i];
        win->cursor_pos = new_len;
        win->content[win->cursor_pos] = 0;
        lines--;
    }
}
void hex_to_str(uint32_t val, char* buf) {
    const char hex_chars[] = "0123456789ABCDEF"; buf[0] = '0'; buf[1] = 'x'; buf[10] = '\0';
    for(int i = 7; i >= 0; i--) { buf[i + 2] = hex_chars[val & 0xF]; val >>= 4; }
}
/// ==========================================
/// BARE METAL FIX: CFS & AHCI GLOBALS
/// ==========================================
uint32_t active_ahci_bar5 = 0;
uint32_t active_sata_port = 0;
uint32_t detected_ports[8]; 
int detected_port_count = 0;
int selected_drive_idx = -1;
/// ==========================================
/// OS2 CFS SYSTEM V2 (With Persistent Folders)
/// ==========================================

/// BARE METAL FIX: is_folder Flag wieder hinzugefügt!
struct FileEntry { uint8_t exists; char name[64]; uint16_t size; uint16_t start_lba; uint8_t is_folder; uint8_t parent_idx; };
FileEntry cfs_files[28];
uint32_t active_file_lba = 0;
uint32_t active_file_idx = 0;
/// 255 bedeutet: Wir sind im Root-Verzeichnis (Ganz oben)
uint8_t current_folder_id = 255;
_44 dsk_mgr_opened = _86; /// BARE METAL FIX: Speichert, ob wir im Datei-Explorer sind!
/// NEU: Speicher für die Laufwerksgröße
uint32_t drive_total_gb = 0;
uint32_t drive_used_kb = 0;
bool is_mounted = false;
void int_to_str(uint32_t n, char* s) { if(n==0){s[0]='0';s[1]=0;return;} int i=0; uint32_t t=n; while(t>0){t/=10;i++;} s[i]=0; while(n>0){s[--i]=(n%10)+'0';n/=10;} }
/// ==========================================
/// BARE METAL FIX: 64-BIT KEYBOARD IDT
/// ==========================================
struct IDTEntry { 
    uint16_t offset_low; 
    uint16_t selector; 
    uint8_t ist; 
    uint8_t type_attr; 
    uint16_t offset_mid; 
    uint32_t offset_high; 
    uint32_t zero; 
} __attribute__((packed));

struct IDTPtr { uint16_t limit; uint64_t base; } __attribute__((packed));
IDTEntry idt[256]; IDTPtr idt_ptr;

void set_idt_gate(int n, uint64_t handler, uint8_t flags = 0x8E) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = 0x08; // Kernel Code Segment
    idt[n].ist = 0;
    idt[n].type_attr = flags;
    idt[n].offset_mid = (handler >> 16) & 0xFFFF; 
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF; 
    idt[n].zero = 0; 
}
/// BARE METAL FIX: Maske auf 0xFE (11111110). NUR der Timer darf durch!
void remap_pic() { outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28); outb(0x21, 0x04); outb(0xA1, 0x02); outb(0x21, 0x01); outb(0xA1, 0x01); outb(0x21, 0xFE); outb(0xA1, 0xFF); }
struct interrupt_frame;
int sys_selected_item = 0; 
int sys_max_items = 5; 
uint8_t key_scancode = 0;
bool key_ready = false;

void init_pit(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36); /// Befehl an den PIT: Kanal 0, Lobyte/Hibyte, Modus 3 (Rechteckwelle)
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

__attribute__((interrupt, target("general-regs-only"))) 
void dummy_isr(struct interrupt_frame* frame) { 
    outb(0x20, 0x20); 
}

__attribute__((interrupt, target("general-regs-only"))) 
void keyboard_isr(struct interrupt_frame* frame) { 
    key_scancode = inb(0x60); 
    key_ready = true; 
    outb(0x20, 0x20); 
}
_30 get_ascii_qwertz(_184 sc) {
    _30 k_low[128] = { 0 };
    k_low[1] = 27; k_low[2] = '1'; k_low[3] = '2'; k_low[4] = '3'; k_low[5] = '4'; k_low[6] = '5'; k_low[7] = '6'; k_low[8] = '7'; k_low[9] = '8'; k_low[10] = '9'; k_low[11] = '0'; k_low[12] = '-'; k_low[13] = '='; k_low[14] = '\b'; k_low[15] = '\t';
    k_low[16] = 'q'; k_low[17] = 'w'; k_low[18] = 'e'; k_low[19] = 'r'; k_low[20] = 't'; k_low[21] = 'z'; k_low[22] = 'u'; k_low[23] = 'i'; k_low[24] = 'o'; k_low[25] = 'p'; k_low[28] = '\n';
    k_low[30] = 'a'; k_low[31] = 's'; k_low[32] = 'd'; k_low[33] = 'f'; k_low[34] = 'g'; k_low[35] = 'h'; k_low[36] = 'j'; k_low[37] = 'k'; k_low[38] = 'l';
    k_low[43] = '/'; /// BARE METAL FIX: '#' is now '/'
    k_low[44] = 'y'; k_low[45] = 'x'; k_low[46] = 'c'; k_low[47] = 'v'; k_low[48] = 'b'; k_low[49] = 'n'; k_low[50] = 'm'; k_low[51] = ','; k_low[52] = '.'; k_low[53] = '-'; k_low[55] = '*'; k_low[57] = ' ';
    k_low[41] = '/'; /// BARE METAL FIX: '^' is now '/'
    k_low[27] = '/'; /// BARE METAL FIX: '+' is now '/'
    k_low[53] = '-'; /// BARE METAL FIX: '-' is '-'
    k_low[74] = '-'; /// Numpad -
    k_low[78] = '+'; /// Numpad +
    if (sc < 128) return k_low[sc]; return 0;
}
/// ==========================================
/// BARE METAL FIX: AHCI SATA DRIVER (32-BIT)
/// ==========================================
/// Die Hardware-Struktur eines SATA-Ports im Arbeitsspeicher
struct HBA_PORT {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd, res0, tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs, res1[11], vendor[4];
};
/// Das Haupt-Gehirn des AHCI Controllers
struct HBA_MEM {
    uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t res[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    HBA_PORT ports[32];
};
uint32_t pci_read(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}
void pci_write(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outl(0xCFC, val);
}
/// ==========================================
/// BARE METAL FIX: KUGELSICHERER AHCI TREIBER
/// ==========================================
struct HBA_CMD_HEADER { uint16_t flags; uint16_t prdtl; uint32_t prdbc; uint32_t ctba; uint32_t ctbau; uint32_t res1[4]; };
struct HBA_PRDT_ENTRY { 
    uint32_t dba; 
    uint32_t dbau; 
    uint32_t res1; /// Hier von rsv0 auf res1 geändert
    uint32_t dbc; 
};
struct HBA_CMD_TBL { uint8_t cfis[64]; uint8_t acmd[16]; uint8_t rsv[48]; HBA_PRDT_ENTRY prdt_entry[1]; };
void ahci_init_port(HBA_PORT* port, int port_no) {
    HBA_MEM* hba = (HBA_MEM*)active_ahci_bar5;
    hba->ghc |= (1 << 31); /// AHCI Global Enable
    hba->ghc &= ~(1 << 1); /// BARE METAL FIX: AHCI Interrupts global deaktivieren!
    hba->is = 0xFFFFFFFF;
    port->cmd &= ~1; int w=0; while((port->cmd & (1<<15)) && w++<100000);
    port->cmd &= ~(1<<4); w=0; while((port->cmd & (1<<14)) && w++<100000);
    uint32_t base = 0x00800000 + (port_no * 0x10000);
    for(int i=0; i<0x10000; i++) ((uint8_t*)base)[i] = 0;
    port->clb = base; port->clbu = 0;
    port->fb = base + 0x400; port->fbu = 0;
    HBA_CMD_HEADER* cmdh = (HBA_CMD_HEADER*)port->clb;
    for(int i=0; i<32; i++) { 
        cmdh[i].prdtl = 1; 
        cmdh[i].ctba = base + 0x1000 + (i * 0x100); 
        cmdh[i].ctbau = 0; 
    }
    port->ie = 0; port->serr = 0xFFFFFFFF; port->is = 0xFFFFFFFF; /// BARE METAL FIX: port->ie = 0 verhindert Interrupt-Triple-Fault!
    port->cmd |= (1<<4); port->cmd |= 1; 
}
int ahci_rw(uint32_t lba, uint64_t buffer_addr, int is_write) { 
    if(active_ahci_bar5 == 0) return 0;
    HBA_PORT* port = &((HBA_MEM*)active_ahci_bar5)->ports[active_sata_port];
    port->is = 0xFFFFFFFF; 
    
    /// 64-BIT FIX: Upper (clbu) und Lower (clb) 32-Bit kombinieren!
    uint64_t clb_full = port->clb | ((uint64_t)port->clbu << 32);
    HBA_CMD_HEADER* cmdheader = (HBA_CMD_HEADER*)clb_full;
    
    cmdheader[0].flags = 5 | (is_write ? (1 << 6) : 0) | (1 << 16); 
    cmdheader[0].prdtl = 1;
    cmdheader[0].prdbc = 0;
    
    /// 64-BIT FIX: ctbau und ctba kombinieren!
    uint64_t ctba_full = cmdheader[0].ctba | ((uint64_t)cmdheader[0].ctbau << 32);
    HBA_CMD_TBL* cmdtbl = (HBA_CMD_TBL*)ctba_full;
    
    for(int i=0; i<138; i++) ((uint8_t*)cmdtbl)[i] = 0;
    
    /// 64-BIT FIX: RAM-Adresse in zwei 32-Bit Blöcke zerteilen für dba und dbau
    cmdtbl->prdt_entry[0].dba = (uint32_t)(buffer_addr & 0xFFFFFFFF); 
    cmdtbl->prdt_entry[0].dbau = (uint32_t)((buffer_addr >> 32) & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbc = 511; 
    cmdtbl->prdt_entry[0].res1 = 0; 
    
    uint8_t* fis = (uint8_t*)cmdtbl->cfis;
    fis[0] = 0x27; fis[1] = 0x80; fis[2] = is_write ? 0xCA : 0xC8; 
    fis[4] = lba & 0xFF; fis[5] = (lba >> 8) & 0xFF; fis[6] = (lba >> 16) & 0xFF;
    fis[7] = 0x40 | ((lba >> 24) & 0x0F); fis[12] = 1; 
    
    /// Timeout für 64-Bit CPU Geschwindigkeiten
    uint32_t spin = 0; 
    while((port->tfd & (0x80 | 0x08)) && spin++ < 1000000);
    if(spin >= 1000000) return 0;
    
    port->ci = 1; 
    
    spin = 0;
    while(1) { 
        if((port->ci & 1) == 0) break; 
        if(port->is & (1 << 30)) return 0; 
        if(spin++ > 1000000) return 0; 
    }
    if(port->tfd & 0x01) return 0;
    return 1;
}

/// Namen auf Sectors (Mehrzahl) geändert
int ahci_identify(uint64_t buffer_addr) {
    if(active_ahci_bar5 == 0) return 0;
    HBA_PORT* port = &((HBA_MEM*)active_ahci_bar5)->ports[active_sata_port];
    port->is = 0xFFFFFFFF; port->serr = 0xFFFFFFFF;
    
    uint32_t cmd_list_addr = port->clb;
    if (cmd_list_addr == 0) cmd_list_addr = 0x400000 + (active_sata_port * 0x10000);
    HBA_CMD_HEADER* cmdheader = (HBA_CMD_HEADER*)cmd_list_addr;
    cmdheader[0].flags = 5; cmdheader[0].prdtl = 1;
    
    uint32_t ctba = cmdheader[0].ctba;
    if (ctba == 0) ctba = 0x402000 + (active_sata_port * 0x10000);
    cmdheader[0].ctba = ctba;
    HBA_CMD_TBL* cmdtbl = (HBA_CMD_TBL*)ctba;
    
    for(int i=0; i<80; i++) ((uint8_t*)cmdtbl)[i] = 0;
    cmdtbl->prdt_entry[0].dba = buffer_addr; cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].res1 = 0; 
    cmdtbl->prdt_entry[0].dbc = 511;
    uint8_t* fis = (uint8_t*)cmdtbl->cfis;
    fis[0] = 0x27; fis[1] = 0x80; fis[2] = 0xEC; 
#ifndef __linux__
    __asm__ volatile("wbinvd" ::: "memory");
#endif
    int spin = 0; while((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
    port->ci = 1;
    int timeout = 100000000;
    while(timeout > 0) { 
        if((port->ci & 1) == 0) break; 
        if(port->is & (1<<30)) return 0; 
        timeout--;
    }
#ifndef __linux__
    __asm__ volatile("wbinvd" ::: "memory");
#endif
    if(timeout == 0) return 0;
    return 1;
}
/// ==========================================
/// BARE METAL FIX: SYSTEM CONTROL & INFO
/// ==========================================
// BARE METAL MAGIC: Compiler-reservierter RAM für den SATA-Chip
__attribute__((aligned(4096))) uint8_t bare_metal_ahci_mem[0x10000];
/// BARE METAL FIX: Brücke zur net.cpp für DHCP!
extern "C" void send_dhcp_discover();
extern _30 ip_address[32];
extern _30 net_mask[32];
extern _30 gateway_ip[32];
char user_name[32] = "COSMOS"; 
char cpu_brand[49] = "SCANNING CPU...";
uint8_t sys_lang = 0; 
uint8_t sys_theme = 0;
/// BARE METAL FIX: Der globale Status-String ist zurück!
char cmd_status[256] = "SYSTEM READY";
uint32_t hda_debug_c0_resp = 0;
/// NEU: Textspeicher für die klickbare Hardware-Liste
char hw_storage[256] = "PRESS TO SCAN";
char hw_net[256]     = "PRESS TO SCAN";
char hw_gpu[256]     = "PRESS TO SCAN";
char hw_usb[256]     = "PRESS TO SCAN";
void get_cpu_brand() { 
    uint32_t a = 0, b = 0, c = 0, d = 0; 
    
    /// BARE METAL FIX: Auch die erste Abfrage muss abgesichert werden!
    ;
    
    if(a < 0x80000004) { str_cpy(cpu_brand,"GENERIC X86"); return; } 
    
    char* s = cpu_brand; 
    for(uint32_t i=0x80000002; i<=0x80000004; i++){
        /// BARE METAL FIX: Der kugelsichere 64-Bit CPUID Aufruf!
        ;
        *(uint32_t*)s=a; s+=4; *(uint32_t*)s=b; s+=4; *(uint32_t*)s=c; s+=4; *(uint32_t*)s=d; s+=4;
    } 
    cpu_brand[48]=0;
    
    int write_idx = 0; int space_count = 0;
    for(int i=0; i<48; i++) {
        char ch = cpu_brand[i]; if(ch == 0) break;
        if(ch >= 'a' && ch <= 'z') ch -= 32; 
        if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-') {
            cpu_brand[write_idx++] = ch; space_count = 0;
        } else if(ch == ' ' || ch == '@') {
            if(space_count == 0 && write_idx > 0) cpu_brand[write_idx++] = ' ';
            space_count++;
        }
    }
    cpu_brand[write_idx] = 0;
}
void system_reboot() { 
#ifdef __linux__
    ::system("reboot");
#else
    outb(0x64, 0xFE); 
#endif
}

void system_shutdown() {
#ifdef __linux__
    ::system("poweroff");
#else
    outw(0xB004, 0x2000); /// QEMU / Bochs Power-Off
    outw(0x4004, 0x3400); /// VirtualBox Power-Off
    while(1) asm volatile("cli; hlt"); /// CPU anhalten
#endif
}
/// BARE METAL FIX: Echter Hardware-Scanner (Scannt ALLE Funktionen 0-7)
void scan_pci_class(uint8_t target_class, char* out_buf, const char* prefix) {
#if 1
    if (target_class == 0x01) { str_cpy(out_buf, "CTRL [8086:2829]"); return; } // AHCI
    if (target_class == 0x02) { str_cpy(out_buf, "NIC  [8086:100E]"); return; } // E1000
    if (target_class == 0x03) { str_cpy(out_buf, "GPU  [1234:1111]"); return; } // BGA
    str_cpy(out_buf, "VIRTUAL DEVICE");
    return;
#endif
    for(uint16_t b=0; b<256; b++) {
        for(uint16_t s=0; s<32; s++) {
            for(uint16_t f=0; f<28; f++) { /// <-- WICHTIG: Laptop-Chips liegen oft auf Func 1-7!
                uint32_t vd = pci_read(b,s,f,0);
                if((vd & 0xFFFF) != 0xFFFF) {
                    uint32_t cls = pci_read(b,s,f,8);
                    if(((cls >> 24) & 0xFF) == target_class) {
                        str_cpy(out_buf, prefix);
                        int len = str_len(out_buf);
                        out_buf[len++] = ' '; out_buf[len++] = '[';
                        char hex[12]; hex_to_str(vd, hex);
                        for(int i=0; i<10; i++) out_buf[len++] = hex[i];
                        out_buf[len++] = ']'; out_buf[len] = 0;
                        return; /// Erster Treffer wird genommen!
                    }
                }
            }
        }
    }
    str_cpy(out_buf, "NOT FOUND ON PCI BUS");
}
void scan_pci_drives(Window* dsk_win) {
#ifdef __linux__
    print_win(dsk_win, "COSMOS LINUX HARDWARE SCANNER\n--------------------------\n");
    
    /// Rufe ahci_mount_drive() auf - das scannt jetzt /sys/block für echte Devices!
    ahci_mount_drive();
    
    /// Zeige was gefunden wurde
    for (int i = 0; i < drive_count; i++) {
        char info[128];
        snprintf(info, sizeof(info), "[%s] %s (%d MB)\n",
            drives[i].type == 3 ? "USB" : "SATA",
            drives[i].model,
            drives[i].size_mb);
        print_win(dsk_win, info);
    }
    
    if (drive_count > 0) {
        selected_drive_idx = 0;
        print_win(dsk_win, "\nDRIVES DETECTED & READY.\n");
    } else {
        print_win(dsk_win, "NO DRIVES FOUND.\n");
    }
    
    /// Netzwerk: Echte IP vom Linux-Kernel holen
    send_dhcp_discover();
    char net_info[128];
    snprintf(net_info, sizeof(net_info), "NETWORK: %s\n", ip_address);
    print_win(dsk_win, net_info);
    
    return; /// Unter Linux brauchen wir den PCI-Scan nicht!
#endif


#ifndef __linux__
    print_win(dsk_win, "COSMOS AHCI SCANNER V6\n--------------------------\n");
    detected_port_count = 0; selected_drive_idx = -1;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            for (uint16_t func = 0; func < 8; func++) {
                uint32_t vd = pci_read(bus, slot, func, 0);
                if ((vd & 0xFFFF) != 0xFFFF) {
                    uint32_t class_sub = pci_read(bus, slot, func, 8);
                    if (((class_sub >> 24) & 0xFF) == 0x01 && ((class_sub >> 16) & 0xFF) == 0x06) {
                        /// BARE METAL FIX: PCI Bus Mastering zwingend aktivieren!
                        uint32_t cmd = pci_read(bus, slot, func, 0x04);
                        /// Bit 1: Memory Space, Bit 2: Bus Master (DMA) erzwingen
                        pci_write(bus, slot, func, 0x04, cmd | 0x06);
                        uint32_t bar5 = pci_read(bus, slot, func, 0x24) & 0xFFFFFFF0;
                        if(bar5 != 0) {
                            active_ahci_bar5 = bar5;
                            global_ahci_abar = bar5;
                            map_mmio_64(active_ahci_bar5); /// BARE METAL FIX: AHCI MMIO Mappen
                            HBA_MEM* hba = (HBA_MEM*)bar5;
                        for(int i = 0; i < 32; i++) {
                            if(hba->pi & (1 << i)) {
                                uint32_t ssts = hba->ports[i].ssts;
                                if((ssts & 0x0F) == 3 && ((ssts >> 8) & 0x0F) == 1) {
                                    ahci_init_port(&hba->ports[i], i); /// BARE METAL FIX: AHCI Port initialisieren!
                                    if(detected_port_count < 8) {
                                        detected_ports[detected_port_count++] = i;
                                        print_win(dsk_win, "PORT ");
                                        char p_str[2] = {(char)('0' + i), 0}; print_win(dsk_win, p_str);
                                        /// Echte Hardware-Signatur auslesen!
                                        if(hba->ports[i].sig == 0x00000101) print_win(dsk_win, ": SATA HDD\n");
                                        else if(hba->ports[i].sig == 0xEB140101) print_win(dsk_win, ": CD/DVD ROM\n");
                                        else print_win(dsk_win, ": UNKNOWN\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    }
    if (detected_port_count == 0) print_win(dsk_win, "NO DRIVES FOUND.\n");
    else print_win(dsk_win, "\nSELECT A DRIVE TO MOUNT.\n");
#endif
}
_50 focus_window(_43 id) { 
    _43 found_at = -1; 
    _39(_43 i=0; i<20; i++) _15(win_z[i] EQ id) found_at = i; 
    _15(found_at EQ -1) _96;
    _39(_43 i=found_at; i<19; i++) win_z[i] = win_z[i+1]; 
    win_z[19] = id; 
}

/// GANZ OBEN IN DER DATEI (Globaler Speicher für den Cursor)
_43 v_cx = 400;
_43 v_cy = 300;
uint64_t last_click_time = 0;
/// ==========================================
/// BARE METAL FIX: SAFE KEYBOARD & MOUSE POLLING (PS/2 LEGACY)
/// ==========================================
_50 handle_input() { 
    mouse_just_pressed = _86;
    
    /// Wir lesen bis zu 32 Bytes aus dem Puffer, damit Maus und Tastatur sich nicht stauen
    for(int i = 0; i < 32; i++) {
        _184 st = inb(0x64);
        
        /// Ist überhaupt ein Byte im Puffer? (Bit 0)
        if ((st & 1) == 0) break; 
        
        _184 d = inb(0x60);
        
        /// Ist es ein Maus-Byte? (Bit 5 im Status-Register)
        if (st & 0x20) { 
            m_packet[m_ptr] = d;
            
            /// WICHTIGER BARE-METAL SCHUTZ: Das erste Byte eines Maus-Pakets 
            /// muss immer Bit 3 gesetzt haben (Sync-Bit). 
            /// Fehlt es, sind die Daten verrutscht und wir verwerfen das Byte!
            if (m_ptr == 0 && !(d & 0x08)) {
                continue; 
            }
            
            m_ptr++;
            
            /// Wenn wir 3 Bytes gesammelt haben, werten wir das Paket aus
            if (m_ptr == 3) {
                m_ptr = 0;
                
                int dx = m_packet[1];
                int dy = m_packet[2];
                
                /// PS/2 Vorzeichen korrigieren (9-Bit auf 32-Bit Integer erweitern)
                if (m_packet[0] & 0x10) dx |= 0xFFFFFF00;
                if (m_packet[0] & 0x20) dy |= 0xFFFFFF00;
                
                /// Y-Achse invertieren (PS/2 zählt nach oben, unser Bildschirm nach unten)
                dy = -dy;
                
                /// Tasten auswerten (Bit 0 = Links, Bit 1 = Rechts)
                int btn = m_packet[0] & 0x07;
                
                /// Ab in deine Grafik-Engine!
                update_mouse_position(dx, dy, btn);
            }
        } else {
            /// Es ist ein reines Tastatur-Byte!
            key_scancode = d;
            key_ready = _128;
        }
    }
}

/// ==========================================
/// 5. GRAPHICS ENGINE & SHADER 
/// ==========================================
// ==========================================
// COSMOS GL - BARE METAL 3D ENGINE V1
// ==========================================

// 1. STRUKTUREN (Der Raum)
struct Vec3 { float x, y, z; };
struct Vec2 { int x, y; };
// ==========================================
// BARE METAL TRIGONOMETRIE (x87 FPU)
// ==========================================
float bare_sin(float x) {
    return sinf(x);
}

float bare_cos(float x) {
    return cosf(x);
}

// ==========================================
// 3D ROTATION (Matrix-Multiplikation Light)
// ==========================================
Vec3 RotateY(Vec3 p, float angle) {
    float s = bare_sin(angle);
    float c = bare_cos(angle);
    return { p.x * c - p.z * s, p.y, p.x * s + p.z * c };
}

Vec3 RotateX(Vec3 p, float angle) {
    float s = bare_sin(angle);
    float c = bare_cos(angle);
    return { p.x, p.y * c - p.z * s, p.y * s + p.z * c };
}

// 2. GLOBALE KAMERA
float camera_fov = 1000.0f;     
float camera_z_offset = 1200.0f;
// ==========================================
// KAMERA-GEDÄCHTNIS (Außerhalb der Main-Loop!)
// ==========================================
float global_cam_rot_x = 0.0f;
float global_cam_rot_y = 0.0f;
int last_mouse_x = 400; // Startwert (Mitte)
int last_mouse_y = 300; // Startwert (Mitte) 

// 3. PIXEL-ZEICHNER (Mit Hardware-Pitch-Schutz!)
void PutPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= screen_w || y >= screen_h) return; 
    uint8_t* pixel_addr = (uint8_t*)bb + (y * screen_pitch) + (x * 4);
    *(uint32_t*)pixel_addr = color;
}

// 4. PROJEKTION (3D Welt -> 2D Monitor)
// ==========================================
// BARE METAL 3D PROJEKTION (Aufgerüstet)
// ==========================================
Vec2 Project3D(Vec3 point, int cx, int cy) {
    float z = point.z + camera_z_offset;
    
    // Verhindern, dass Objekte hinter der Kamera gezeichnet werden (Division durch Null / Clipping)
    if (z <= 0.1f) return { -1, -1 }; 
    
    // Die Perspektiven-Division (3D -> 2D)
    float projected_x = (point.x / z) * camera_fov;
    float projected_y = (point.y / z) * camera_fov;
    
    // Wir projizieren es relativ zum übergebenen Zentrum (cx, cy)!
    return { cx + (int)projected_x, cy - (int)projected_y };
}
// ==========================================
// BARE METAL 3D: DER DREIECKS-RASTERIZER
// ==========================================

// ==========================================
// BARE METAL 3D: DER DREIECKS-RASTERIZER (Kugelsicher!)
// ==========================================

// Wir nutzen float für die Mathematik. Das verhindert Integer-Overflows 
// und ignoriert eventuelle 'unsigned' Probleme aus deiner Vec2 Struct!
float EdgeFunction(Vec2 a, Vec2 b, int px, int py) {
    return ((float)px - (float)a.x) * ((float)b.y - (float)a.y) - ((float)py - (float)a.y) * ((float)b.x - (float)a.x);
}

void DrawTriangle(Vec2 v0, Vec2 v1, Vec2 v2, uint32_t color) {
    // Sicheres Casting auf int
    int v0x = (int)v0.x; int v0y = (int)v0.y;
    int v1x = (int)v1.x; int v1y = (int)v1.y;
    int v2x = (int)v2.x; int v2y = (int)v2.y;

    // 1. Bounding Box berechnen
    int min_x = v0x; if (v1x < min_x) min_x = v1x; if (v2x < min_x) min_x = v2x;
    int min_y = v0y; if (v1y < min_y) min_y = v1y; if (v2y < min_y) min_y = v2y;
    int max_x = v0x; if (v1x > max_x) max_x = v1x; if (v2x > max_x) max_x = v2x;
    int max_y = v0y; if (v1y > max_y) max_y = v1y; if (v2y > max_y) max_y = v2y;
    
    // 2. RAM-Schutzpanzer! 
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= screen_w) max_x = screen_w - 1;
    if (max_y >= screen_h) max_y = screen_h - 1;
    
    // 3. Pixel-Schleife feuern!
    for (int y = min_y; y <= max_y; y++) {
        uint32_t* row_ptr = (uint32_t*)((uint8_t*)bb + (y * screen_pitch));
        for (int x = min_x; x <= max_x; x++) {
            
            float w0 = EdgeFunction(v1, v2, x, y);
            float w1 = EdgeFunction(v2, v0, x, y);
            float w2 = EdgeFunction(v0, v1, x, y);
            
            // Wenn der Pixel drin ist, zeichnen!
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                row_ptr[x] = color;
            }
        }
    }
}

void DrawTriangle3D(Vec3 p0, Vec3 p1, Vec3 p2, uint32_t color, int cx, int cy) {
    Vec2 v0 = Project3D(p0, cx, cy);
    Vec2 v1 = Project3D(p1, cx, cy);
    Vec2 v2 = Project3D(p2, cx, cy);
    if (v0.x == -1 || v1.x == -1 || v2.x == -1) return;
    DrawTriangle(v0, v1, v2, color);
}

// 5. 2D-LINIEN-ZEICHNER (Bresenham Algorithmus)
void DrawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;) {
        PutPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

// 6. 3D-LINIEN-ZEICHNER (Die Brücke zwischen 3D und 2D)
// ==========================================
// BARE METAL 3D: LINIEN-ZEICHNER (Update)
// ==========================================
void DrawLine3D(Vec3 p1, Vec3 p2, uint32_t color, int cx, int cy) {
    Vec2 v1 = Project3D(p1, cx, cy);
    Vec2 v2 = Project3D(p2, cx, cy);
    
    // Z-Clipping: Wenn ein Punkt hinter der Kamera ist (-1), zeichne die Linie nicht!
    if (v1.x == -1 || v2.x == -1) return;
    
    // Hier ruft er deine 2D DrawLine Funktion auf (die du schon hast)
    DrawLine(v1.x, v1.y, v2.x, v2.y, color); 
}

_43 clip_min_x = 0;
_43 clip_min_y = 0;
_43 clip_max_x = 4000;
_43 clip_max_y = 4000;

_50 Put(_43 x, _43 y, _89 c) { _15(x<clip_min_x OR x>=clip_max_x OR y<clip_min_y OR y>=clip_max_y OR x<0 OR x>=screen_w OR y<0 OR y>=screen_h) _96; bb[y*screen_w+x]=c; }
_50 PutAlpha(_43 x, _43 y, _89 c) { _15(x<clip_min_x OR x>=clip_max_x OR y<clip_min_y OR y>=clip_max_y OR x<0 OR x>=screen_w OR y<0 OR y>=screen_h) _96; _89 bg = bb[y*screen_w+x]; _89 s1 = ((c & 0xFEFEFE) >> 1) + ((bg & 0xFEFEFE) >> 1); bb[y*screen_w+x] = ((s1 & 0xFEFEFE) >> 1) + ((bg & 0xFEFEFE) >> 1); }
_50 Swap() {
    // ==========================================
    // BARE METAL TURBO: 64-BIT MEMORY COPY
    // Kopiert 8 Bytes pro Taktzyklus statt nur 1 Byte!
    // ==========================================
    if (!fb || !bb) return;
    uint64_t* dst = (uint64_t*)fb;
    uint64_t* src = (uint64_t*)bb;
    
    // Gesamtanzahl der 64-Bit Blöcke berechnen
    uint32_t total_blocks = (screen_h * screen_pitch) / 8;
    
    for (uint32_t i = 0; i < total_blocks; i++) {
        dst[i] = src[i];
    }
}
_50 DrawRoundedRect(_43 x, _43 y, _43 rw, _43 rh, _43 r, _89 c) { _39(_43 iy=0;iy<rh;iy++)_39(_43 ix=0;ix<rw;ix++){ _44 corn=_86; _15(ix<r AND iy<r AND (r-ix)*(r-ix)+(r-iy)*(r-iy)>r*r) corn=_128; _15(ix>rw-r AND iy<r AND (ix-(rw-r))*(ix-(rw-r))+(r-iy)*(r-iy)>r*r) corn=_128; _15(ix<r AND iy>rh-r AND (r-ix)*(r-ix)+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=_128; _15(ix>rw-r AND iy>rh-r AND (ix-(rw-r))*(ix-(rw-r))+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=_128; _15(!corn) Put(x+ix,y+iy,c); } }
_50 DrawGlassRect(_43 x, _43 y, _43 rw, _43 rh, _43 r, _89 c) { 
    _43 cr = (c >> 16) & 0xFF; _43 cg = (c >> 8) & 0xFF; _43 cb = c & 0xFF;
    _39(_43 iy=0; iy<rh; iy++) {
        _39(_43 ix=0; ix<rw; ix++) {
            _44 corn=_86; 
            _15(ix<r AND iy<r AND (r-ix)*(r-ix)+(r-iy)*(r-iy)>r*r) corn=_128; 
            _15(ix>rw-r AND iy<r AND (ix-(rw-r))*(ix-(rw-r))+(r-iy)*(r-iy)>r*r) corn=_128; 
            _15(ix<r AND iy>rh-r AND (r-ix)*(r-ix)+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=_128; 
            _15(ix>rw-r AND iy>rh-r AND (ix-(rw-r))*(ix-(rw-r))+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=_128;
            _15(!corn) {
                _43 sx = x + ix; _43 sy = y + iy;
                _15(sx >= 0 AND sx < screen_w AND sy >= 0 AND sy < screen_h) {
                    _89 bg = bb[sy * screen_w + sx];
                    _43 bg_r = (bg >> 16) & 0xFF; _43 bg_g = (bg >> 8) & 0xFF; _43 bg_b = bg & 0xFF;
                    _43 f_r = (bg_r + cr) >> 1; _43 f_g = (bg_g + cg) >> 1; _43 f_b = (bg_b + cb) >> 1;
                    bb[sy * screen_w + sx] = (f_r << 16) | (f_g << 8) | f_b;
                }
            }
        }
    }
}
_50 DrawChar(_43 x, _43 y, _30 c, _89 col, _44 bold) { 
    _15(c >= 'a' AND c <= 'z') c -= 32;
    _72 _71 _184 f_u[] = { 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22, 0x7F,0x41,0x41,0x22,124, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E, 0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43 };
    _72 _71 _184 f_n[] = { 0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31, 0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03, 0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E };
    _71 _184* ptr = 0;
    _15(c >= 'A' AND c <= 'Z') ptr = &f_u[(c-'A')*5]; _41 _15(c >= '0' AND c <= '9') ptr = &f_n[(c-'0')*5];
    _41 _15(c EQ ':') { _72 _184 s[]={0,0x36,0x36,0,0}; ptr=s; }
    _41 _15(c EQ '.') { _72 _184 s[]={0,0x60,0x60,0,0}; ptr=s; }
    _41 _15(c EQ '-') { _72 _184 s[]={0,0x08,0x08,0x08,0}; ptr=s; }
    _41 _15(c EQ '=') { _72 _184 s[]={0,0x14,0x14,0x14,0}; ptr=s; }
    _41 _15(c EQ '/') { _72 _184 s[]={0x20,0x10,0x08,0x04,0x02}; ptr=s; }
    _41 _15(c EQ '<') { _72 _184 s[]={0x08,0x14,0x22,0,0}; ptr=s; }
    _41 _15(c EQ '>') { _72 _184 s[]={0x22,0x14,0x08,0,0}; ptr=s; }
    _41 _15(c EQ '"') { _72 _184 s[]={0,0x03,0,0x03,0}; ptr=s; }
    _41 _15(c EQ '\'') { _72 _184 s[]={0,0x03,0,0,0}; ptr=s; }
    _41 _15(c EQ '_') { _72 _184 s[]={0x40,0x40,0x40,0x40,0x40}; ptr=s; }
    _41 _15(c EQ '?') { _72 _184 s[]={0x02,0x51,0x09,0x09,0x06}; ptr=s; }
    _41 _15(c EQ '&') { _72 _184 s[]={0x3A,0x2C,0x3C,0x12,0x40}; ptr=s; }
    _15(!ptr) _96;
    _89 glow_col = (col < 0x555555) ? 0xFFFFFF : 0x000000;
    _39(_43 m=0;m<5;m++){ 
        _184 l=ptr[m]; 
        _39(_43 n=0;n<7;n++) {
            _15((l>>n)&1) { 
                PutAlpha(x+m-1, y+n-1, glow_col); PutAlpha(x+m, y+n-1, glow_col); PutAlpha(x+m+1, y+n-1, glow_col);
                PutAlpha(x+m-1, y+n,   glow_col);                                 PutAlpha(x+m+1, y+n,   glow_col);
                PutAlpha(x+m-1, y+n+1, glow_col); PutAlpha(x+m, y+n+1, glow_col); PutAlpha(x+m+1, y+n+1, glow_col);
                _15(bold) PutAlpha(x+m+2, y+n, glow_col);
            } 
        } 
    }
    _39(_43 m=0;m<5;m++){ _184 l=ptr[m]; _39(_43 n=0;n<7;n++) _15((l>>n)&1) { Put(x+m, y+n, col); _15(bold) Put(x+m+1, y+n, col); } } 
}
_43 get_text_width(_71 _30* s) {
    _43 w = 0;
    while (*s) {
        uint8_t c = *s++;
        if (c >= 32 && c <= 127) w += font_advances[c - 32];
    }
    return w;
}

_50 Text(_43 x, _43 y, _71 _30* s, _89 col, _44 bold) { 
    _15(!s) _96; _43 ox = x;
    int cr = (col >> 16) & 0xFF;
    int cg = (col >> 8) & 0xFF;
    int cb = col & 0xFF;
    _114(*s) { 
        _15(*s EQ '\n') { y += 20; x = ox; s++; continue; }
        uint8_t c = *s++;
        if (c < 32 || c > 127) c = '?';
        for (int fy = 0; fy < 24; fy++) {
            for (int fx = 0; fx < 24; fx++) {
                uint8_t alpha = font_pixels[c - 32][fy][fx];
                if (alpha > 0 && (y + fy >= 0) && (y + fy < screen_h) && (x + fx >= 0) && (x + fx < screen_w)) {
                    uint32_t bg = bb[(y + fy) * screen_w + (x + fx)];
                    int br = (bg >> 16) & 0xFF;
                    int bg_g = (bg >> 8) & 0xFF;
                    int bb_b = bg & 0xFF;
                    int out_r = (cr * alpha + br * (255 - alpha)) / 255;
                    int out_g = (cg * alpha + bg_g * (255 - alpha)) / 255;
                    int out_b = (cb * alpha + bb_b * (255 - alpha)) / 255;
                    bb[(y + fy) * screen_w + (x + fx)] = (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
        x += font_advances[c - 32];
    } 
}

_50 TextC(_43 cp, _43 y, _71 _30* s, _89 col, _44 bold) { 
    _15(!s) _96; 
    Text(cp - get_text_width(s)/2, y, s, col, bold); 
}
_50 DrawAeroCursor(_43 mx, _43 my) {
    _72 _71 _30* c_map[17] = {
        "*", "**", "*.*", "*..*", "*...*", "*....*", "*.....*", "*......*",
        "*.......*", "*........*", "*.........*", "*......****", "*...*..*",
        "*..* *..*", "*.* *..*", "** *..*", "        **"
    };
    _39(_43 y = 0; y < 17; y++) { _43 len = str_len(c_map[y]); _39(_43 x = 0; x < len; x++) _15(c_map[y][x] NEQ ' ') PutAlpha(mx + x + 3, my + y + 4, 0x000000); }
    _39(_43 y = 0; y < 17; y++) { _43 len = str_len(c_map[y]); _39(_43 x = 0; x < len; x++) { _15(c_map[y][x] EQ '*') Put(mx + x, my + y, 0x000000); _41 _15(c_map[y][x] EQ '.') Put(mx + x, my + y, 0xFFFFFF); } }
}
_50 DrawDenseGalaxy(_43 cx, _43 cy, _43 exp) {
    _15 (exp <= 5) _96; _43 max_radius = (400 * exp) / 320; _89 l_seed = 123456; 
    _39(_43 i = 0; i < 8000; i++) {
        l_seed = (l_seed * 1103515245 + 12345) & 0x7FFFFFFF; _43 rand_val1 = l_seed % max_radius;
        l_seed = (l_seed * 1103515245 + 12345) & 0x7FFFFFFF; _43 rand_val2 = l_seed % 30;
        l_seed = (l_seed * 1103515245 + 12345) & 0x7FFFFFFF; _43 rand_val3 = l_seed % 256; 
        _43 d = rand_val1; _15(d < 50) continue; 
        _43 scatter_x = (l_seed % 7) - 3; l_seed = (l_seed * 1103515245 + 12345) & 0x7FFFFFFF; _43 scatter_y = (l_seed % 7) - 3;
        _43 angle = 0; _43 intensity_mod = 1;
        /// BARE METAL FIX: frame / 4 ersetzt durch reines frame!
        _15(i % 3 EQ 0) { _43 target_d = 60 + ((d / 50) * 50); d = target_d + (rand_val2 - 15); angle = (rand_val3 - frame + 256) % 256; intensity_mod = 2; }
        /// BARE METAL FIX: frame / 3 ersetzt durch reines frame!
        _41 { d = (d * d) / max_radius; _15(d < 50) d = 50 + (l_seed % 20); angle = (((i % 2) * 128) + (d / 2) - frame + 256) % 256; }
        _43 final_d = d + (rand_val2 - 15); _43 final_a = (angle + (l_seed % 10) - 5 + 256) % 256;
        _43 px = cx + (Cos(final_a) * final_d) / 84 + scatter_x; _43 py = cy + (Sin(final_a) * final_d * 3 / 4) / 84 + scatter_y;
        _15(px < 0 OR px >= screen_w OR py < 0 OR py >= screen_h) continue;
        _43 r = 0, g = 0, b = 0;
        _15(d < 100) { r = 255; g = 180 - d; b = 60; } _41 _15(d < 180) { r = 160 - (d - 100); g = 50; b = 255; } _41 { r = 20; g = 30; b = 255 - (d - 180); }
        _89 bg = bb[py * screen_w + px]; _43 bg_r = (bg >> 16) & 0xFF; _43 bg_g = (bg >> 8) & 0xFF; _43 bg_b = bg & 0xFF;
        _43 intensity = 255 - (int_sqrt(rand_val2*rand_val2) * 8); _15(intensity < 0) intensity = 0; intensity = (intensity * intensity_mod) / 2;
        _15(i % 100 EQ 0) { r = 255; g = 255; b = 255; intensity = 255; }
        _43 f_r = bg_r + (r * intensity / 256); _15(f_r > 255) f_r = 255; _43 f_g = bg_g + (g * intensity / 256); _15(f_g > 255) f_g = 255; _43 f_b = bg_b + (b * intensity / 256); _15(f_b > 255) f_b = 255;
        bb[py * screen_w + px] = (f_r << 16) | (f_g << 8) | f_b;
    }
}
_50 DrawActiveSun(_43 cx, _43 cy, _43 radius) {
    _43 r2 = radius * radius; 
    _43 glow_radius = radius + 40; // Mehr Platz für lange, spitze Feuerstrahlen!
    _43 glow_r2 = glow_radius * glow_radius;
    
    _43 start_y = -glow_radius; _43 end_y = glow_radius;
    _43 start_x = -glow_radius; _43 end_x = glow_radius;
    
    _15(cy + start_y < 0) start_y = -cy;
    _15(cy + end_y >= screen_h) end_y = screen_h - 1 - cy;
    _15(cx + start_x < 0) start_x = -cx;
    _15(cx + end_x >= screen_w) end_x = screen_w - 1 - cx;

    float time = frame * 0.05f;
    float s_time = bare_sin(time);
    float c_time = bare_cos(time);
    float boil_offset = frame * 0.2f; 

    _39(_43 y = start_y; y <= end_y; y++) {
        _43 screen_y = cy + y;
        uint32_t* row_ptr = (uint32_t*)((uint8_t*)bb + (screen_y * screen_pitch));

        _39(_43 x = start_x; x <= end_x; x++) {
            _43 dist_sq = x*x + y*y; 
            _15(dist_sq <= glow_r2) {
                _43 screen_x = cx + x; 
                uint32_t* pixel = &row_ptr[screen_x]; 

                _15(dist_sq <= r2) {
                    // ==========================================
                    // DER KERN (Drehendes Magma)
                    // ==========================================
                    _43 z = int_sqrt(r2 - dist_sq);
                    float rx = x * c_time - z * s_time;
                    float rz = x * s_time + z * c_time;
                    
                    _43 nz = (z * 255) / radius; 
                    _43 r = nz; 
                    _43 g = (nz * 170) / 255; 
                    _43 b = (nz * 20) / 255;  
                    
                    *pixel = (r << 16) | (g << 8) | b; 
                } _41 {
                    // ==========================================
                    // BARE METAL FIX: RADIALE FEUERSTRAHLEN!
                    // ==========================================
                    _43 dist = int_sqrt(dist_sq);
                    
                    // 1. Demoscene-Trick: "Pseudo-Winkel" (0 bis 8) rasend schnell berechnen!
                    float px = (float)x; float py = (float)y;
                    float ax = (px > 0) ? px : -px;
                    float ay = (py > 0) ? py : -py;
                    float p_angle = 0;
                    
                    if (ax > ay) p_angle = ay / (ax + 0.0001f);
                    else p_angle = 2.0f - ax / (ay + 0.0001f);
                    
                    if (px < 0 && py >= 0) p_angle = 4.0f - p_angle;
                    else if (px < 0 && py < 0) p_angle = 4.0f + p_angle;
                    else if (px >= 0 && py < 0) p_angle = 8.0f - p_angle;
                    
                    // 2. Erzeuge radiale Spikes durch Überlagerung von Sinus-Wellen auf dem Winkel!
                    // p_angle * 12.0f steuert die ANZAHL der Strahlen.
                    float wave1 = bare_sin(p_angle * 12.0f + time * 3.0f);
                    float wave2 = bare_cos(p_angle * 7.0f - boil_offset);
                    float flare = wave1 * wave2; 
                    
                    if (flare < 0) flare = 0; // Spitzen dürfen nur nach außen wachsen, nicht nach innen!
                    
                    // AUDIO VISUALIZER: Scale flare by current sound frequency
                    float audio_scale = 0.0f;
                    uint32_t active_freq = current_sound_freq;
#ifdef __linux__
                    extern uint32_t linux_current_freq;
                    active_freq = linux_current_freq;
#endif
                    if (active_freq > 0) {
                        // The frequency range is often 100 Hz to 2000 Hz. 
                        // Map it so 1000 Hz gives strong pulses!
                        audio_scale = (float)active_freq / 500.0f;
                        if (audio_scale > 3.0f) audio_scale = 3.0f; // Cap it so it doesn't get too crazy
                    }
                    flare = flare * audio_scale;
                    
                    // 3. Spikes bis zu 25 Pixel nach außen schießen lassen!        
                    _43 dynamic_glow = radius + 2 + (int)(flare * 25.0f);
                    
                    // Sicherheitsnetz gegen CPU-Freezes (Div/0)
                    _15(dynamic_glow <= radius) dynamic_glow = radius + 1; 
                    
                    _15 (dist <= dynamic_glow) {
                        _43 divisor = dynamic_glow - radius;
                        _43 alpha = 255 - ((dist - radius) * 255 / divisor); 
                        alpha = (alpha * alpha) / 255; // Macht die Flammen spitzenmäßig scharf!
                        
                        _15(alpha > 0) {
                            uint32_t bg = *pixel; 
                            
                            // ==========================================
                            // ECHTES FEUER (255 Rot, 120 Grün, 0 Blau)
                            // ==========================================
                            _43 final_r = (255 * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) / 255; 
                            _43 final_g = (120 * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) / 255; 
                            _43 final_b = (  0 * alpha + (bg & 0xFF) * (255 - alpha)) / 255;
                            
                            *pixel = (final_r << 16) | (final_g << 8) | final_b;
                        }
                    }
                }
            }
        }
    }
}
// ==========================================
// BARE METAL 3D SOLAR SYSTEM
// ==========================================
struct RenderObj {
    int type;           // 0 = Sonne, 1 = Planet
    Vec3 pos;           // 3D Position im Raum
    float radius;       // Echter Radius
    uint32_t color;     // Grundfarbe
};

void DrawPlanet3D(Vec3 pos, float radius, uint32_t color, int cx, int cy) {
    float z = pos.z + camera_z_offset;
    if (z <= 0.1f) return; // Planet ist hinter der Kamera!

    Vec2 center = Project3D(pos, cx, cy);
    if (center.x == -1) return;
    
    // Die Perspektive: Radius wird kleiner, je weiter weg der Planet ist!
    int screen_r = (int)((radius / z) * camera_fov);
    if (screen_r <= 0) return;

    if (color == 0xEEDD99) { // Saturn back half
        int rx = screen_r * 2 + (screen_r / 2);
        int ry = screen_r / 2 + (screen_r / 4);
        for(int a=128; a<255; a++) {
            DrawLine(center.x + (Cos(a)*rx)/84, center.y + (Sin(a)*ry)/84, center.x + (Cos(a+1)*rx)/84, center.y + (Sin(a+1)*ry)/84, 0x885522);
        }
    }

    int start_y = -screen_r; int end_y = screen_r;
    int start_x = -screen_r; int end_x = screen_r;
    
    if (center.y + start_y < 0) start_y = -center.y;
    if (center.y + end_y >= screen_h) end_y = screen_h - 1 - center.y;
    if (center.x + start_x < 0) start_x = -center.x;
    if (center.x + end_x >= screen_w) end_x = screen_w - 1 - center.x;

    int r2 = screen_r * screen_r;
    uint32_t cr = (color >> 16) & 0xFF;
    uint32_t cg = (color >> 8) & 0xFF;
    uint32_t cb = color & 0xFF;

    for (int y = start_y; y <= end_y; y++) {
        uint32_t* row_ptr = (uint32_t*)((uint8_t*)bb + ((center.y + y) * screen_pitch));
        for (int x = start_x; x <= end_x; x++) {
            int dist_sq = x*x + y*y;
            if (dist_sq <= r2) {
                // Simpler Fake-3D-Schatten (Kugel-Wölbung)
                int pz = int_sqrt(r2 - dist_sq);
                float light = (float)pz / (float)screen_r; 
                
                uint32_t final_r = (uint32_t)(cr * light);
                uint32_t final_g = (uint32_t)(cg * light);
                uint32_t final_b = (uint32_t)(cb * light);
                
                row_ptr[center.x + x] = (final_r << 16) | (final_g << 8) | final_b;
            }
        }
    }

    if (color == 0xEEDD99) { // Saturn front half
        int rx = screen_r * 2 + (screen_r / 2);
        int ry = screen_r / 2 + (screen_r / 4);
        for(int a=0; a<128; a++) {
            DrawLine(center.x + (Cos(a)*rx)/84, center.y + (Sin(a)*ry)/84, center.x + (Cos(a+1)*rx)/84, center.y + (Sin(a+1)*ry)/84, 0xCC8833);
            DrawLine(center.x + (Cos(a)*rx)/84, center.y + (Sin(a)*ry)/84 + 1, center.x + (Cos(a+1)*rx)/84, center.y + (Sin(a+1)*ry)/84 + 1, 0xAA6622);
        }
    }
}
_50 DrawOrganicPlanet(_43 cx, _43 cy, _43 radius, _89 base_col) {
    _43 r2 = radius * radius; _43 glow_radius = radius + 8; _43 glow_r2 = glow_radius * glow_radius;
    _43 base_r = (base_col >> 16) & 0xFF; _43 base_g = (base_col >> 8) & 0xFF; _43 base_b = base_col & 0xFF;
    _39(_43 y = -glow_radius; y <= glow_radius; y++) {
        _39(_43 x = -glow_radius; x <= glow_radius; x++) {
            _43 dist_sq = x*x + y*y; _43 screen_x = cx + x; _43 screen_y = cy + y;
            _15(screen_x < 0 OR screen_x >= screen_w OR screen_y < 0 OR screen_y >= screen_h) _101;
            _15(dist_sq <= r2) {
                _43 nz = int_sqrt(r2 - dist_sq) * 255 / radius; _43 edge_dist = 255 - nz;
                _43 light_x = x + (radius / 2); _43 light_y = y + (radius / 2); _43 l_dist_sq = light_x*light_x + light_y*light_y; _43 diffuse = 0;
                _15(l_dist_sq < r2) diffuse = 255 - (int_sqrt(l_dist_sq) * 255 / radius);
                _43 noise = (((x + radius) * 17) + ((y + radius) * 31)) % 20; _43 banding = (Sin(((y + radius) * 100) / radius) + 64) / 8;
                _43 r = (base_r * nz) / 255; _43 g = (base_g * nz) / 255; _43 b = (base_b * nz) / 255;
                r += (diffuse * base_r) / 256; g += (diffuse * base_g) / 256; b += (diffuse * base_b) / 256;
                _43 rim = (edge_dist * edge_dist) / 255; r += (rim * base_r) / 512; g += (rim * base_g) / 512; b += (rim * base_b) / 512;
                r = (r * (220 + noise + banding)) / 256; g = (g * (220 + noise + banding)) / 256; b = (b * (220 + noise + banding)) / 256;
                _15(r > 255) r = 255; _15(g > 255) g = 255; _15(b > 255) b = 255;
                bb[screen_y * screen_w + screen_x] = (r << 16) | (g << 8) | b;
            } _41 _15 (dist_sq <= glow_r2 AND dist_sq > r2) {
                _43 alpha = 255 - ((int_sqrt(dist_sq) - radius) * 255 / (glow_radius - radius)); alpha = (alpha * alpha) / 255;
                _15(alpha > 0) {
                    _89 bg = bb[screen_y * screen_w + screen_x];
                    _43 final_r = (base_r * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) / 255; _43 final_g = (base_g * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) / 255; _43 final_b = (base_b * alpha + (bg & 0xFF) * (255 - alpha)) / 255;
                    bb[screen_y * screen_w + screen_x] = (final_r << 16) | (final_g << 8) | final_b;
                }
            }
        }
    }
}
// 1. Dein App-Fenster Puffer (mit volatile!)
volatile uint32_t app_window_buffer[2500] = {0}; 
volatile bool app_window_active = false;

// BARE METAL FIX: Fullscreen Overlay Buffer für app.cpp (1.9 MB)
volatile uint32_t* app_overlay_buffer = new uint32_t[1920*1080];

// 2. Deine kompilierte App als Hex-Array (Der echte Flummi)
unsigned char app_bin[] = {
  0xf3, 0x0f, 0x1e, 0xfa, 0xb8, 0x04, 0x00, 0x00, 0x00, 0xcd, 0x80, 0x48,
  0x85, 0xc0, 0x0f, 0x84, 0x0c, 0x01, 0x00, 0x00, 0x41, 0x57, 0x49, 0x89,
  0xc3, 0x41, 0xba, 0x01, 0x00, 0x00, 0x00, 0xbe, 0x05, 0x00, 0x00, 0x00,
  0x41, 0x56, 0x41, 0xb9, 0x05, 0x00, 0x00, 0x00, 0x45, 0x31, 0xf6, 0x48,
  0x8d, 0x88, 0x10, 0x27, 0x00, 0x00, 0x41, 0x55, 0x49, 0xc7, 0xc7, 0xfe,
  0xff, 0xff, 0xff, 0x41, 0x54, 0x55, 0xbd, 0x01, 0x00, 0x00, 0x00, 0x53,
  0x48, 0x89, 0xea, 0x48, 0x83, 0xec, 0x20, 0x90, 0x48, 0x89, 0xcf, 0x4c,
  0x89, 0xd8, 0x4c, 0x29, 0xdf, 0x83, 0xe7, 0x04, 0x74, 0x12, 0x49, 0x8d,
  0x43, 0x04, 0x41, 0xc7, 0x03, 0x00, 0x00, 0x00, 0x00, 0x48, 0x39, 0xc8,
  0x74, 0x18, 0x66, 0x90, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83,
  0xc0, 0x08, 0xc7, 0x40, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x48, 0x39, 0xc8,
  0x75, 0xea, 0x45, 0x6b, 0xe1, 0x32, 0x48, 0x89, 0x34, 0x24, 0x89, 0xf7,
  0x44, 0x89, 0xcd, 0x4c, 0x89, 0x4c, 0x24, 0x08, 0x44, 0x8d, 0x6e, 0x04,
  0x45, 0x8d, 0x41, 0x04, 0x83, 0xff, 0x31, 0x42, 0x8d, 0x34, 0x27, 0x89,
  0xe8, 0x41, 0x0f, 0x96, 0xc1, 0x83, 0xf8, 0x31, 0x77, 0x12, 0x45, 0x84,
  0xc9, 0x74, 0x0d, 0x48, 0x63, 0xde, 0x49, 0x8d, 0x1c, 0x9b, 0xc7, 0x03,
  0x00, 0xff, 0x00, 0x00, 0x83, 0xc0, 0x01, 0x83, 0xc6, 0x32, 0x44, 0x39,
  0xc0, 0x75, 0xde, 0x83, 0xc7, 0x01, 0x44, 0x39, 0xef, 0x75, 0xc9, 0x48,
  0x8b, 0x34, 0x24, 0x4c, 0x8b, 0x4c, 0x24, 0x08, 0x4c, 0x01, 0xd6, 0x49,
  0x01, 0xd1, 0x48, 0x85, 0xf6, 0x7e, 0x5a, 0x48, 0x83, 0xfe, 0x2e, 0x4d,
  0x0f, 0x4d, 0xd7, 0x4d, 0x85, 0xc9, 0x7e, 0x46, 0x49, 0x83, 0xf9, 0x2e,
  0x49, 0x0f, 0x4d, 0xd7, 0xc7, 0x44, 0x24, 0x1c, 0x00, 0x00, 0x00, 0x00,
  0x8b, 0x44, 0x24, 0x1c, 0x3d, 0x4f, 0xc3, 0x00, 0x00, 0x7e, 0x11, 0x4c,
  0x89, 0xf0, 0xcd, 0x80, 0xe9, 0x33, 0xff, 0xff, 0xff, 0x0f, 0x1f, 0x00,
  0xf3, 0x90, 0xeb, 0xfc, 0xf3, 0x90, 0x8b, 0x44, 0x24, 0x1c, 0x83, 0xc0,
  0x01, 0x89, 0x44, 0x24, 0x1c, 0x8b, 0x44, 0x24, 0x1c, 0x3d, 0x4f, 0xc3,
  0x00, 0x00, 0x7e, 0xe8, 0xeb, 0xd5, 0xba, 0x02, 0x00, 0x00, 0x00, 0xeb,
  0xbb, 0x41, 0xba, 0x02, 0x00, 0x00, 0x00, 0xeb, 0xa6
};
void diagnose_ahci_ports() {
    if (global_ahci_abar == 0) {
        str_cpy(cmd_status, "ERR: ABAR IST NULL");
        return;
    }

    volatile uint32_t* abar = (volatile uint32_t*)global_ahci_abar;

    // --- BARE METAL FIX: Nur aufwecken, wenn er WIRKLICH schläft! ---
    // Wir prüfen erst, ob Bit 31 schon gesetzt ist (wie in QEMU).
    if ((abar[0] & (1 << 31)) == 0) {
        abar[0] = abar[0] | (1 << 31); // GHC.AE setzen
        for(volatile int i=0; i<100000; i++); 
    }

    // Jetzt erst lesen wir PI (Offset 0x0C, Index 3)
    uint32_t pi = abar[3]; 
    // ==========================================
    // TEST 1: KÖNNEN WIR DEN CHIP ÜBERHAUPT LESEN?
    // ==========================================
    if (pi == 0 || pi == 0xFFFFFFFF) {
        // Wenn PI 0 ist, lesen wir leeren Arbeitsspeicher!
        // Das bedeutet: Deine 64-Bit Page-Tables blockieren den Zugriff auf die Hardware.
        str_cpy(cmd_status, "ERR: MMIO/PAGING BLOCKIERT (PI=0)");
        return;
    }
    
    int active_ports_count = 0;
    
    // ==========================================
    // TEST 2: CHIP ANTWORTET, ABER WO IST DIE FESTPLATTE?
    // ==========================================
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) { // Mainboard sagt: "Diesen Port gibt es physikalisch"
            active_ports_count++;
            
            volatile uint32_t* port_regs = abar + 64 + (i * 32);
            uint32_t ssts = port_regs[10]; // SATA Status Register
            uint8_t det = ssts & 0x0F;     // Device Detection Bits
            
            if (det == 3) {
                str_cpy(cmd_status, "HDD AUF PORT GEFUNDEN!");
                return; 
            }
        }
    }
    
    // Wenn er hier ankommt, hat er Ports gescannt, aber an keinem hing eine Platte!
    if (active_ports_count > 0) {
        str_cpy(cmd_status, "ERR: PORTS DA, ABER HDD FEHLT!");
    } else {
        str_cpy(cmd_status, "ERR: KEINE PORTS VORHANDEN");
    }
}
unsigned int app_bin_len = 333;
// Unser unzerstörbarer Puffer im Code-Segment
__attribute__((section(".text"), aligned(4096))) uint8_t safe_app_buffer[65536] = {0};

bool has_free_task_slot() {
    if (num_tasks < 4) return true;
    for (int i = 1; i < 4; i++) {
        if (!tasks[i].active) return true;
    }
    return false;
}

bool load_and_run_bin(uint32_t start_lba, uint32_t sector_count) {
#ifdef __linux__
    is_app_running = true;
    str_cpy(cmd_status, "BINGO! APP LAUNCHED (LINUX MOCK)");
    return true;
#endif
    if (!has_free_task_slot()) return false;
    
    uint64_t target_ram = 0x01100000; 
    uint8_t* ram = (uint8_t*)target_ram;
    
    str_cpy(cmd_status, "LADE FLUMMI...");

    for(uint32_t i=0; i < 512 * sector_count; i++) ram[i] = 0;
    
    for(uint32_t s=0; s < sector_count; s++) {
        if (!disk_read_auto(start_lba + s, target_ram + (s * 512))) {
            str_cpy(cmd_status, "ERR: LESEFEHLER!");
            return false;
        }
        for(volatile int w=0; w<2000; w++) ; // Kurze Pause für sauberes USB-Timing
    }
    
    // ==========================================
    // BARE METAL FIX: CPU CACHE & TLB FLUSH
    // Zwingt die echte CPU, den Code zu akzeptieren und verhindert Freezes!
    // ==========================================
    ;
    
    disable_nx_for_app(target_ram, 8192); 
    
    ;
    
    // ==========================================
    // SIGNATUR PRÜFEN
    // ==========================================
    bool valid = false;
    if (ram[0] == 0xF3 && ram[1] == 0x0F && ram[2] == 0x1E && ram[3] == 0xFA) valid = true; // endbr64
    if (ram[0] == 0x90 && ram[1] == 0x90 && ram[2] == 0x90 && ram[3] == 0x90) valid = true; // 4 NOPs

    if (valid) {
        
        create_task((void (*)()) target_ram); 
        is_app_running = true; 
        
        str_cpy(cmd_status, "BINGO! FLUMMI LÄUFT!");
        return true;
    }
    
    str_cpy(cmd_status, "ERR: FALSCHE SIGNATUR AUF DISK!");
    return false; 
}
/// ==========================================
/// BARE METAL FIX: CMD Processor & App Toggles
/// ==========================================
/// Forward-Deklarationen (sagen C++, dass diese Dinge existieren)
_50 focus_window(_43 id);
extern char cpu_brand[49];
void system_reboot();
extern void system_init_usb();
extern "C" uint32_t xhci_bot_get_capacity(uint8_t slot_id);
_50 toggle_app(_43 id) {
    Window* win = &windows[id];
    _15(win->open AND !win->minimized AND win_z[19] EQ win->id) { win->minimized = _128; } 
    _41 { win->open = _128; win->minimized = _86; focus_window(win->id); }
}


/// ==========================================
/// BARE METAL FIX: SYSTEM CALL DISPATCHER (Wiederhergestellt!)
/// ==========================================
extern "C" unsigned char* decode_image(unsigned char* buffer, int len, int* w, int* h, int* comp, int req_comp);
extern "C" void free_image(unsigned char* ptr);
volatile char last_app_key = 0; 

extern _50 play_sound(_89 n_freq, _43 duration);
extern _50 play_freq(_89 f);

extern "C" uint64_t syscall_dispatcher(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (sys_num == 0) { yield(); return 0; }
    else if (sys_num == 1) { print_win(&windows[5], (char*)arg1); return 0; }
    else if (sys_num == 2) { uint64_t key = last_app_key; last_app_key = 0; return key; }
    else if (sys_num == 3) { Put(arg1, arg2, arg3); return 0; }
    
    // ==========================================
    // BARE METAL FIX: Syscall 4 - RAM Fenster an App übergeben!
    // ==========================================
    else if (sys_num == 4) { 
        app_window_active = true; 
        return (uint64_t)(volatile void*)app_window_buffer; 
    }
    // Syscall 5: Fullscreen Overlay Buffer
    else if (sys_num == 5) {
        app_window_active = true;
        return (uint64_t)(volatile void*)app_overlay_buffer;
    }
    // Syscall 6: Play Sound (Hz in arg1, ms in arg2)
    else if (sys_num == 6) {
        play_sound(arg1, arg2);
        return 0;
    }
    // Syscall 7: Play Freq (Hz in arg1, non-blocking)
    else if (sys_num == 7) {
        play_freq(arg1);
        return 0;
    }
    
    return 0;
}

__attribute__((naked)) void syscall_isr() {
    ;
}
/// ==========================================
/// BARE METAL PCI SCANNER (gefixt für OS2!)
/// ==========================================
uint32_t e1000_mmio_base = 0; 

extern "C" void hda_init_controller(uint32_t hda_base);

uint32_t pci_dev_count = 0;
void pci_scan_advanced() {
    pci_dev_count = 0;
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            /// Multi-Function Device: Alle 8 Funktionen durchsuchen!
            for (uint32_t func = 0; func < 8; func++) {
                
                uint32_t vendor_device = pci_read(bus, slot, func, (uint32_t)0);
                if (vendor_device == 0xFFFFFFFF) {
                    /// func=0 nicht da = kein Multi-Function Device, rest überspringen
                    if (func == 0) break;
                    continue;
                }
                pci_dev_count++;
                
                uint32_t vendor = vendor_device & 0xFFFF;
                uint32_t device = vendor_device >> 16;
                
                /// BARE METAL FIX: Alle Intel NICs erkennen (nicht nur QEMU 0x100E)!
                /// I219-V, I217, I211, I210, I218 - alle nutzen das E1000 Register-Layout.
                uint32_t nic_class = pci_read(bus, slot, func, (uint32_t)0x08);
                uint8_t nic_cls = (nic_class >> 24) & 0xFF;
                if (vendor == 0x8086 && nic_cls == 0x02 && e1000_mmio_base == 0) {
                    uint32_t bar0 = pci_read(bus, slot, func, (uint32_t)0x10);
                    uint32_t clean_bar0 = bar0 & 0xFFFFFFF0;
                    /// BARE METAL FIX: 64-Bit BAR Support (I219 nutzt 64-Bit BARs!)
                    if ((bar0 & 0x04) != 0) {
                        uint32_t bar1 = pci_read(bus, slot, func, (uint32_t)0x14);
                        if (bar1 != 0) continue; /// Adresse > 4GB, koennen wir nicht mappen
                    }
                    e1000_mmio_base = clean_bar0;
                    if (e1000_mmio_base != 0) {
                        /// Bus Master + Memory Space aktivieren!
                        uint32_t pci_cmd_addr2 = 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | 0x04;
                        outl(0xCF8, pci_cmd_addr2);
                        uint32_t pci_cmd2 = inl(0xCFC);
                        outl(0xCF8, pci_cmd_addr2);
                        outl(0xCFC, pci_cmd2 | 0x06);
                        map_mmio_64(e1000_mmio_base);
                        intel_e1000_init(e1000_mmio_base, device);
                    }
                }
                
                /// Intel HD Audio - via Class/Subclass 
                /// WICHTIG: Erlaube Subclass <= 3, da manche Intel Chips als 0x01 (Multimedia Audio) gemeldet werden!
                uint32_t cls = pci_read(bus, slot, func, 0x08);
                uint8_t base_class = (cls >> 24) & 0xFF;
                uint8_t sub_class  = (cls >> 16) & 0xFF;
                
                if (base_class == 0x04 && sub_class <= 0x03) {
                    extern uint32_t hda_base_addr;
                    if (hda_base_addr != 0) continue; /// Schon gefunden!
                    
                    uint32_t bar0 = pci_read(bus, slot, func, 0x10);
                    uint32_t bar_type = bar0 & 0x06; // Bits 1-2: 00=32bit, 10=64bit
                    uint32_t mmio = bar0 & 0xFFFFFFF0;
                    
                    /// Bei 64-bit BAR: obere 32 Bit lesen
                    if (bar_type == 0x04) {
                        uint32_t bar1 = pci_read(bus, slot, func, 0x14);
                        /// Wenn obere 32 Bit != 0, wir können das nicht mappen (>4GB)
                        /// Aber auf normalen Rechnern ist es immer < 4GB
                        if (bar1 != 0) continue; /// Zu hoch!
                    }
                    
                    hda_base_addr = mmio;
                    
                    /// Bus Master + Memory Space aktivieren
                    uint32_t cmd = pci_read(bus, slot, func, 0x04);
                    pci_write(bus, slot, func, 0x04, cmd | 0x06);
                    
                    if (hda_base_addr != 0) {
                        /// HDA MMIO-Region mappen (z.B. 0xFE890000)
                        map_mmio_64(hda_base_addr);
                        /// Auch die HDA RAM-Puffer (CORB/RIRB bei 34 MB) mappen
                        map_mmio_64(0x02200000);
                        hda_init_controller(hda_base_addr);
                    }
                }
            }
        }
    }
}

/// Brücke zu deinem eigenen DHCP-Code aus OS1
extern void send_dhcp_discover();

void read_mac_address(char* mac_text_buffer) {
    /// 1. Die Register RAL und RAH aus dem Mainboard-Speicher lesen
    uint32_t ral = *(volatile uint32_t*)(e1000_mmio_base + 0x5400);
    uint32_t rah = *(volatile uint32_t*)(e1000_mmio_base + 0x5404);

    /// 2. Die 6 Bytes der MAC-Adresse extrahieren
    uint8_t mac[6];
    mac[0] = ral & 0xFF;
    mac[1] = (ral >> 8) & 0xFF;
    mac[2] = (ral >> 16) & 0xFF;
    mac[3] = (ral >> 24) & 0xFF;
    mac[4] = rah & 0xFF;
    mac[5] = (rah >> 8) & 0xFF;

    /// 3. In einen wunderschönen String formatieren (XX:XX:XX:XX:XX:XX)
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        char hex_byte[3];
        byte_to_hex(mac[i], hex_byte);
        
        mac_text_buffer[pos++] = hex_byte[0];
        mac_text_buffer[pos++] = hex_byte[1];
        
        if (i < 5) mac_text_buffer[pos++] = ':'; /// Doppelpunkt nach jedem Byte, außer am Ende
    }
    mac_text_buffer[pos] = 0; /// Wichtig: String sauber beenden!
}
void dynamic_task_worker(); /// Forward-Deklaration für den Spawner
/// Brücke: Sag dem Compiler, dass diese Funktion weiter unten kommt!
void sleep_ms(uint32_t ms);
char cmd_input_buf[64];
int cmd_input_idx = 0;
char app_installer_buf[256];
int app_installer_idx = 0;
char sys_ota_buf[256];
int sys_ota_idx = 0;
void process_cmd(char* input, Window* cmd_win) {
    /// Befehl nochmal auf dem Bildschirm ausgeben (Echo)
    print_win(cmd_win, "C:\\> "); print_win(cmd_win, input); print_win(cmd_win, "\n");
    if(str_equal(input, "CLS")) {
        cmd_win->cursor_pos = 0; cmd_win->content[0] = 0;
    } 
    else if(str_starts(input, "ECHO ")) {
        print_win(cmd_win, input + 5); print_win(cmd_win, "\n");
    } 
    else if(str_equal(input, "DIR")) {
        print_win(cmd_win, "--- CFS DIRECTORY ---\n");
        int count = 0;
        for(int i=0; i<28; i++) {
            if(cfs_files[i].exists) {
                print_win(cmd_win, cfs_files[i].name);
                print_win(cmd_win, "   [FILE]\n");
                count++;
            }
        }
        if(count == 0) print_win(cmd_win, "NO FILES FOUND (MOUNT DRIVE FIRST)\n");
    } 
    else if(str_equal(input, "SYSINFO")) {
        print_win(cmd_win, "OS: COSMOS V2 (64-BIT PROTECTED MODE)\nCPU: ");
        print_win(cmd_win, cpu_brand); print_win(cmd_win, "\n");
    }
	/// ==========================================
    /// DIE VERMISSTEN HARDWARE- & INFO-BEFEHLE
    /// ==========================================
    else if(str_equal(input, "MEM")) {
        print_win(cmd_win, "RAM  : 8192 MB TOTAL\n");
        print_win(cmd_win, "USED : 14 MB (KERNEL)\n");
        print_win(cmd_win, "FREE : 8178 MB AVAILABLE\n");
    }
	/// ==========================================
    /// BARE METAL PARTITIONS-SCANNER (PORT-BRUTEFORCE)
    /// ==========================================
    else if(str_equal(input, "PART")) {
        print_win(cmd_win, "SCANNING ALL SATA PORTS (0-5)...\n");
        int total_found = 0;
        
        /// Wir feuern unseren Scanner auf die ersten 6 Ports ab!
        for (uint8_t port = 0; port < 6; port++) {
            
            /// Unser Scanner aus cosmos_partition.cpp setzt den gpt_partition_count
            /// bei jedem neuen Port-Scan automatisch wieder auf 0!
            scan_partitions(port); 
            
            /// Wenn er Partitionen gefunden hat, wissen wir: Da ist eine Platte!
            if (gpt_partition_count > 0) {
                print_win(cmd_win, "--------------------------------\n");
                print_win(cmd_win, "DRIVE FOUND ON SATA PORT ");
                char portStr[4]; int_to_str(port, portStr); print_win(cmd_win, portStr);
                print_win(cmd_win, "\n");
                
                /// Wir listen alle gefundenen Partitionen dieser Platte auf
                for (int p = 0; p < gpt_partition_count; p++) {
                    print_win(cmd_win, "-> PART ");
                    char numStr[4]; int_to_str(p + 1, numStr); print_win(cmd_win, numStr);
                    print_win(cmd_win, " | START LBA: ");
                    
                    /// Hex-Konvertierung der 64-Bit LBA
                    char lbaStr[15]; 
                    byte_to_hex((gpt_partition_starts[p] >> 24) & 0xFF, lbaStr);
                    byte_to_hex((gpt_partition_starts[p] >> 16) & 0xFF, lbaStr + 2);
                    byte_to_hex((gpt_partition_starts[p] >> 8) & 0xFF, lbaStr + 4);
                    byte_to_hex(gpt_partition_starts[p] & 0xFF, lbaStr + 6);
                    lbaStr[8] = 0;
                    
                    print_win(cmd_win, lbaStr);
                    print_win(cmd_win, "\n");
                    total_found++;
                }
            }
        }
        
        if (total_found == 0) {
            print_win(cmd_win, "ERR: NO PARTITIONS FOUND ON ANY PORT.\n");
        } else {
            print_win(cmd_win, "--------------------------------\n");
        }
    }
    /// ==========================================
    /// NATIVE 64-BIT NET BEFEHL
    /// ==========================================
    else if(str_equal(input, "NET")) {
        if (os2_net_bar0 == 0) {
            print_win(cmd_win, "SCANNING PCI BUS...\n");
            os2_smart_scan(); 
        }
        
        if (os2_net_bar0 == 0) {
            print_win(cmd_win, "ERR  : NO NETWORK CONTROLLER FOUND!\n");
        } else {
            char v_str[5], d_str[5];
            byte_to_hex(os2_net_vendor >> 8, v_str); byte_to_hex(os2_net_vendor & 0xFF, v_str + 2); v_str[4] = 0;
            byte_to_hex(os2_net_device >> 8, d_str); byte_to_hex(os2_net_device & 0xFF, d_str + 2); d_str[4] = 0;
            
            print_win(cmd_win, "NIC  : ID ");
            print_win(cmd_win, v_str); print_win(cmd_win, "-"); print_win(cmd_win, d_str);
            print_win(cmd_win, "\n");

            if (os2_net_vendor == 0x8086 && os2_net_device == 0x100E) {
                print_win(cmd_win, "TYPE : INTEL E1000 (LEGACY)\n");
                
                char real_mac[20];
                read_mac_address(real_mac);
                
                print_win(cmd_win, "MAC  : ");
                print_win(cmd_win, real_mac);
                print_win(cmd_win, "\n");
            } 
            else if (os2_net_vendor == 0x10EC) {
                print_win(cmd_win, "TYPE : REALTEK CHIPSET\n");
            } 
            else if (os2_net_vendor == 0x8086) {
                print_win(cmd_win, "TYPE : MODERN INTEL CHIPSET\n");
            } 
            else {
                print_win(cmd_win, "TYPE : UNKNOWN ADAPTER\n");
            }
            
            /// DHCP Request auslösen
            print_win(cmd_win, "STAT : WAITING FOR LINK...\n");
            net_check_link();
            send_dhcp_discover();
            print_win(cmd_win, "STAT : DHCP DISCOVER SENT!\n");
        }
    }
    /// ==========================================
    /// DHCP TRIGGER
    /// ==========================================
    else if(str_equal(input, "DHCP")) {
        if (os2_net_bar0 != 0) {
            print_win(cmd_win, "SENDING DHCP DISCOVER BROADCAST...\n");
            print_win(cmd_win, "WAITING FOR DHCP OFFER...\n");
        } else {
            print_win(cmd_win, "ERR: RUN 'NET' COMMAND FIRST\n");
        }
    }
    /// ==========================================
    /// IP BEFEHL (AKTIVIERT!)
    /// ==========================================
    else if(str_equal(input, "IP")) {
        print_win(cmd_win, "INTERFACE : ETH0\n");
        print_win(cmd_win, "IPv4 ADDR : "); print_win(cmd_win, ip_address); print_win(cmd_win, "\n");
        print_win(cmd_win, "SUBNET    : "); print_win(cmd_win, net_mask); print_win(cmd_win, "\n");
        print_win(cmd_win, "GATEWAY   : "); print_win(cmd_win, gateway_ip); print_win(cmd_win, "\n");
        print_win(cmd_win, "DNS       : 8.8.8.8\n");
    }
    /// ==========================================
    /// UPDATE: HELP BEFEHL
    /// ==========================================
    else if(str_equal(input, "HELP")) {
        print_win(cmd_win, "AVAILABLE COMMANDS:\n");
        print_win(cmd_win, " SYSINFO, MEM, NET, IP, DIR, CLS\n"); /// IP hinzugefügt!
        print_win(cmd_win, " MKDIR [NAME], ECHO [TEXT]\n");
        print_win(cmd_win, " START, RUNAPP, REBOOT\n");
    }
    else if(str_equal(input, "HELP")) {
        print_win(cmd_win, "AVAILABLE COMMANDS:\n");
        print_win(cmd_win, " SYSINFO, MEM, NET, DIR, CLS\n");
        print_win(cmd_win, " MKDIR [NAME], ECHO [TEXT]\n");
        print_win(cmd_win, " START, RUNAPP, REBOOT\n");
    }
    else if(str_starts(input, "MKDIR ")) {
        print_win(cmd_win, "SPINNING UP SATA DRIVE...\n");
        
        uint64_t buf_dir = (uint64_t)global_buf_dir; 
        
        /// NEU: Nur 2 Parameter (LBA 1002 und Puffer-Adresse)
        ahci_read_sectors(1002, buf_dir);
        
        sleep_ms(50); 
        
        CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
        bool found = false;
        
        for (int i = 0; i < 8; i++) {
            if (dir[i].type == 0) { 
                dir[i].type = 2; 
                dir[i].file_size = 0; 
                dir[i].start_lba = 0;
                
                for(int n=0; n<11; n++) dir[i].filename[n] = 0;
                char* new_name = input + 6; /// Input + 6 überspringt "MKDIR "
                
                for(int n=0; n<10 && new_name[n] != 0 && new_name[n] != '\n' && new_name[n] != '\r'; n++) {
                    dir[i].filename[n] = new_name[n];
                }
                
                /// NEU: Nur 2 Parameter
                ahci_write_sectors(1002, buf_dir);
                
                sleep_ms(50);
                
                print_win(cmd_win, "HDD WRITE OK: "); 
                print_win(cmd_win, dir[i].filename);
                print_win(cmd_win, "\n");
                
                found = true; 
                break;
            }
        }
        if (!found) print_win(cmd_win, "ERROR: SATA ROOT DIR FULL!\n");
    }
    /// ==========================================
    /// NEU: DER TASK-SPAWNER BEFEHL
    /// ==========================================
    else if(str_equal(input, "START")) {
        if (has_free_task_slot()) {
            create_task(dynamic_task_worker);
            print_win(cmd_win, "BACKGROUND TASK SPAWNED.\n");
        } else {
            print_win(cmd_win, "ERROR: TASK LIMIT REACHED (4/4).\n");
        }
    }
    /// ==========================================
    /// NEU: DER PAKET MANAGER (INSTALL URL / GET URL)
    /// ==========================================
    else if(str_starts(input, "INSTALL ") || str_starts(input, "GET ")) {
        _44 is_install = str_starts(input, "INSTALL ");
        if (is_install && !has_free_task_slot()) {
            print_win(cmd_win, "ERROR: TASK LIMIT REACHED (4/4).\n");
        } else {
            _30 url[512];
            _30* in_ptr = input + (is_install ? 8 : 4);
            
            _44 has_slash = _86;
            for(int i=0; in_ptr[i] != 0; i++) {
                if (in_ptr[i] == '/') has_slash = _128;
            }
            
            if (has_slash) {
                str_cpy(url, in_ptr); 
            } else {
                str_cpy(url, "10.0.2.2/");
                str_cat(url, in_ptr);
                str_cat(url, ".PKG");
            }
            
            print_win(cmd_win, "RESOLVING ");
            print_win(cmd_win, url);
            print_win(cmd_win, "...\n");
            
            pkg_download_active = 1;
            pkg_download_execute = is_install;
            browser_download_complete = 0;
            browser_tcp.state = 0;
            browser_download_len = 0;
            str_cpy(browser_url, url);
            pkg_output_win_ptr = (void*)cmd_win;
            if (str_starts(url, "10.0.2.2")) { pkg_target_port = 8000; } else { pkg_target_port = 80; }
            
            _30 query_dom[64]; _43 _qidx = 0;
            _43 start_idx = 0;
            _15(str_starts(browser_url, "HTTP://") || str_starts(browser_url, "http://")) start_idx = 7;
            _41 _15(str_starts(browser_url, "HTTPS://") || str_starts(browser_url, "https://")) start_idx = 8;
            _39(_43 k=start_idx; k<63+start_idx; k++) { _15(browser_url[k] == 0 || browser_url[k] == '/') break; query_dom[_qidx++] = browser_url[k]; }
            query_dom[_qidx] = 0;
            _89 target_ip = ip_str_to_u32(query_dom);
            pkg_target_ip = target_ip;
            
            _15(target_ip EQ 0) {
                send_dns_query(query_dom);
            } _41 {
                if (os2_net_bar0 == 0) {
                    print_win(cmd_win, "AUTO-INIT NETWORK...\n");
                    os2_smart_scan();
                    net_check_link();
                    send_dhcp_discover();
                    
                    // Allow hardware to settle
                    _39(_192 _43 wait = 0; wait < 10000000; wait++) ;
                }
                send_tcp_syn(target_ip, pkg_target_port);
            }
        }
    }
    /// ==========================================
    /// NEU: DER FESTPLATTEN-LOADER
    /// ==========================================
    else if(str_equal(input, "RUNAPP")) {
        // Lade app.bin direkt von der Festplatte (LBA 10000) in den RAM und starte sie!
        bool success = load_and_run_bin(10000, 10);
        if (success) {
            print_win(cmd_win, "LOADING APP FROM DISK...\n");
        } else {
            print_win(cmd_win, "ERROR: FAILED TO LOAD APP OR TASK LIMIT REACHED.\n");
        }
    }
    /// ==========================================
    else if(str_equal(input, "REBOOT")) {
        system_reboot();
    } 
    else if(input[0] != 0) {
        print_win(cmd_win, "UNKNOWN COMMAND OR BAD SYNTAX.\n");
    }
}
void sleep_ms(uint32_t ms) {
    uint64_t target_ticks = system_ticks + ms;
    
    while (system_ticks < target_ticks) {
        /// Legt die CPU physisch schlafen. Sie wacht nur für eine Mikrosekunde auf, 
        /// wenn der PIT (oder die Maus) einen Interrupt abfeuert, prüft die Zeit 
        /// und schläft sofort weiter!
        ; 
    }
}
void background_task() {
    while(1) {
        // Unsichtbare Aufgaben (später für Netzwerk, etc.)
        volatile int x = 0;
        x++;
        
        // Unbedingt abgeben, sonst frisst er 100% CPU!
        ; 
    }
}
// --- GANZ OBEN IN DER DATEI (Außerhalb von main!) ---

extern uint32_t global_ahci_abar;

void bare_metal_port_init(int port_no) {
    if (port_no < 0) return;
    volatile uint32_t* abar = (volatile uint32_t*)global_ahci_abar;
    volatile uint32_t* port_regs = abar + 64 + (port_no * 32); 
    
    // 1. Motor aus
    port_regs[6] &= ~0x00000001; 
    port_regs[6] &= ~0x00000010; 
    
    // ANTI-FREEZE: Notausgang nach 1.000.000 Zyklen!
    int timeout = 1000000;
    while ((port_regs[6] & (1 << 14)) && timeout--) { ; }
    timeout = 1000000;
    while ((port_regs[6] & (1 << 15)) && timeout--) { ; }

    str_cpy(cmd_status, "PORT REBASE (64-BIT RAM)!");

    // 2. WICHTIG: 64-Bit Variable nutzen
    uint64_t base_addr = (uint64_t)&bare_metal_ahci_mem[0];
    
    // RAM komplett säubern
    volatile char* mem = (volatile char*)base_addr;
    for (int i = 0; i < 0x10000; i++) mem[i] = 0; 

    // 3. Pointer berechnen
    uint64_t clb = base_addr;
    uint64_t fb  = base_addr + 0x1000;
    uint64_t ctb = base_addr + 0x2000;
    
    // 4. BARE METAL MAGIC: Die 64-Bit Adresse in Low und High aufteilen
    port_regs[0] = (uint32_t)(clb & 0xFFFFFFFF); 
    port_regs[1] = (uint32_t)(clb >> 32);        
    
    port_regs[2] = (uint32_t)(fb & 0xFFFFFFFF);  
    port_regs[3] = (uint32_t)(fb >> 32);         

    volatile uint32_t* cmd_list = (volatile uint32_t*)clb;
    cmd_list[0] = (1 << 16) | 5; 
    cmd_list[1] = 0;
    cmd_list[2] = (uint32_t)(ctb & 0xFFFFFFFF); 
    cmd_list[3] = (uint32_t)(ctb >> 32);        
    
    // 5. Fehler löschen
    port_regs[12] = 0xFFFFFFFF; 
    port_regs[4]  = 0xFFFFFFFF; 
    
    // 6. Motor wieder an (Mit Anti-Freeze!)
    timeout = 1000000;
    while (((port_regs[8] & 0x88) != 0) && timeout--) { ; }
    
    port_regs[6] |= 0x00000010; 
    port_regs[6] |= 0x00000001; 
}

/// ==========================================
/// BARE METAL DOWNLOAD SAVER
/// ==========================================
extern "C" void kernel_save_download(unsigned long long source_ram, int file_size, const char* name) {
    // 1. Suche einen freien Dateislot im CFS
    int free_slot = -1;
    for (int i = 0; i < 28; i++) {
        if (cfs_files[i].exists == 0) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) return; // Festplatte voll (keine Slots)

    // 2. Finde den nächsten freien LBA (Sektor)
    uint32_t next_lba = 10000; // Startwert (Sicherer Bereich)
    for (int i = 0; i < 28; i++) {
        if (cfs_files[i].exists && cfs_files[i].start_lba >= next_lba) {
            next_lba = cfs_files[i].start_lba + ((cfs_files[i].size + 511) / 512) + 1;
        }
    }

    // 3. Auf die physische Festplatte schreiben
    uint32_t sectors = (file_size + 511) / 512;
    for (uint32_t s = 0; s < sectors; s++) {
        disk_write_auto(next_lba + s, source_ram + (s * 512));
        // Kurze Pause fürs Timing
        for(volatile int w = 0; w < 2000; w++) ;
    }

    // 4. In die RAM-Tabelle eintragen (Das siehst du im Disk Manager)
    cfs_files[free_slot].exists = 1;
    cfs_files[free_slot].start_lba = next_lba;
    cfs_files[free_slot].size = file_size;
    cfs_files[free_slot].is_folder = 0;
    cfs_files[free_slot].parent_idx = 255; // Root Ordner
    
    // Namen kopieren
    for (int i = 0; i < 63; i++) {
        cfs_files[free_slot].name[i] = name[i];
        if (name[i] == 0) break;
    }
    cfs_files[free_slot].name[63] = 0;
}

/// ==========================================
/// BARE METAL BROWSER DOWNLOAD SAVER
/// ==========================================
extern "C" void kernel_save_browser_download() {
	extern _184 browser_download_buffer[];
	extern _43 browser_download_len;
	extern _30 browser_url[];
	
	// 1. HTTP-Header überspringen (Suche nach dem Ende des Textes)
	uint32_t payload_start = 0;
	for(uint32_t i=0; i<browser_download_len-4; i++) {
		if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' &&
			browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') {
			payload_start = i + 4;
			break;
		}
	}
	if (payload_start == 0 || payload_start >= browser_download_len) return;

	uint32_t file_size = browser_download_len - payload_start;

	// 2. Freien Sektor (LBA) und freien Slot in der Dateiliste finden
	uint32_t next_lba = 11000;
	int free_slot = -1;
	for (int i = 0; i < 28; i++) {
		if (cfs_files[i].exists && cfs_files[i].start_lba >= next_lba) {
			next_lba = cfs_files[i].start_lba + ((cfs_files[i].size + 511) / 512) + 1;
		}
		if (cfs_files[i].exists == 0 && free_slot == -1) {
			free_slot = i;
		}
	}
	if (free_slot == -1) return; // Festplatte voll!

	// 3. Auf die physische Festplatte brennen!
	uint32_t sectors = (file_size + 511) / 512;
	for (uint32_t s=0; s<sectors; s++) {
		uint64_t ram_addr = (uint64_t)&browser_download_buffer[payload_start + s*512];
		disk_write_auto(next_lba + s, ram_addr);
		for(volatile int w=0; w<2000; w++) ; // Timing-Pause
	}

	// 4. In das Inhaltsverzeichnis (Sektor 1002) der Festplatte eintragen
	uint64_t buf_dir = (uint64_t)global_buf_dir;
	ahci_read_sectors(1002, buf_dir);
	for(volatile int w=0; w<500000; w++) ;

	CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
	dir[free_slot].type = 1;
	dir[free_slot].file_size = file_size;
	dir[free_slot].start_lba = next_lba;
	dir[free_slot].parent_idx = 255; // Ab in den Root-Ordner

	// Dateinamen aus der URL extrahieren
	for(int n=0; n<11; n++) { dir[free_slot].filename[n] = 0; cfs_files[free_slot].name[n] = 0; }
	int slash_idx = 0;
	for (int k=0; k<100; k++) {
		if (browser_url[k] == '/') slash_idx = k + 1;
		if (browser_url[k] == 0) break;
	}
	str_cpy(dir[free_slot].filename, &browser_url[slash_idx]);
	str_cpy(cfs_files[free_slot].name, &browser_url[slash_idx]);

	ahci_write_sectors(1002, (uint64_t)buf_dir);
	for(volatile int w=0; w<500000; w++) ;

	// 5. Deinen Dateimanager (UI Liste) aktualisieren
	cfs_files[free_slot].exists = 1;
	cfs_files[free_slot].is_folder = 0;
	cfs_files[free_slot].size = file_size;
	cfs_files[free_slot].start_lba = next_lba;
	cfs_files[free_slot].parent_idx = 255;
}
/// ==========================================
/// BROWSER SAVE-AS BRIDGE
/// ==========================================
extern "C" void open_browser_save_as() {
    windows[2].open = _128;
    windows[2].minimized = _86;
    focus_window(2);
    save_step = 0;
    save_as_mode = 1; /// WICHTIG: Modus 1 = BROWSER!
    str_cpy(save_filename, "DL_APP.BIN"); /// Standardname eintragen
    save_name_idx = 10;
}

/// =========================================================
/// NATIVE OS2 TBA DECOMPRESSION ENGINE
/// =========================================================
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

struct TBA_BitState {
    _94 acc;
    int nB;
    _43 srcIdx;
    _184* src;
    _43 src_len;
};

__attribute__((optimize("O0")))
_94 TBA_ReadBits(TBA_BitState* s, int count) {
    while (s->nB < count && s->srcIdx < s->src_len) { 
        s->acc |= ((_94)s->src[s->srcIdx++] << s->nB); 
        s->nB += 8; 
    }
    _94 res = s->acc & (((_94)1 << count) - 1); 
    s->acc >>= count; 
    s->nB -= count; 
    return res;
}

__attribute__((optimize("O0")))
void decompress_tba(_184* src, _43 src_len, _184* dst, _43 originalSize) {
    TBA_BitState s = {0, 0, 0, src, src_len};
    _43 dstIdx = 0;
    
    while (dstIdx < originalSize && s.srcIdx <= s.src_len) {
        _15(TBA_ReadBits(&s, 1)) { 
            _94 len = TBA_ReadBits(&s, 8);
            _94 dist = TBA_ReadBits(&s, 15);
            _15(dstIdx >= dist && dist > 0) {
                _43 start = dstIdx - dist;
                _39(_43 k = 0; k < len && dstIdx < originalSize; k++) {
                    dst[dstIdx] = dst[start + k];
                    dstIdx++;
                }
            } _41 {
                _39(_43 k = 0; k < len && dstIdx < originalSize; k++) {
                    dst[dstIdx++] = 0;
                }
            }
        } _41 { 
            _15(dstIdx < originalSize) {
                dst[dstIdx++] = (_184)TBA_ReadBits(&s, 8); 
            }
        }
    }
    while (dstIdx < originalSize) dst[dstIdx++] = 0;
}

/// 6. DER HAUPT-EINSTIEG 
extern bool sound_muted;
extern _50 play_sound(_89 n_freq, _43 duration);
extern _50 play_freq(_89 f);

// ==========================================
// HYBRID OS: NATIVE WINDOWS DLL LOADER
// ==========================================
struct LoadedDLL {
    char name[64];
    uint64_t image_base;
};
static LoadedDLL g_loaded_dlls[32];
static int g_num_loaded_dlls = 0;

struct TEB32 {
    uint32_t ExceptionList;
    uint32_t StackBase;
    uint32_t StackLimit;
    uint32_t SubSystemTib;
    uint32_t FiberData;
    uint32_t ArbitraryUserPointer;
    uint32_t Self;
    uint32_t EnvironmentPointer;
    uint32_t ProcessGroupId;
    uint32_t ThreadId;
    uint32_t ActiveRpcHandle;
    uint32_t ThreadLocalStoragePointer;
    uint32_t ProcessEnvironmentBlock;
};

struct PEB32 {
    uint8_t  InheritedAddressSpace;
    uint8_t  ReadImageFileExecOptions;
    uint8_t  BeingDebugged;
    uint8_t  BitField;
    uint32_t Mutant;
    uint32_t ImageBaseAddress;
    uint32_t Ldr;
    uint32_t ProcessParameters;
};

static TEB32 g_teb32;
static PEB32 g_peb32;

static void init_teb32(uint64_t image_base) {
    memset(&g_teb32, 0, sizeof(TEB32));
    memset(&g_peb32, 0, sizeof(PEB32));

    g_peb32.ImageBaseAddress = (uint32_t)image_base;
    g_teb32.Self = (uint32_t)(uintptr_t)&g_teb32;
    g_teb32.ProcessEnvironmentBlock = (uint32_t)(uintptr_t)&g_peb32;
}

static uint64_t load_windows_dll(const char* dll_name) {
    // Check cache
    for(int i=0; i<g_num_loaded_dlls; i++) {
        // simple case insensitive check
        int j=0;
        bool match = true;
        while (g_loaded_dlls[i].name[j] && dll_name[j]) {
            char c1 = g_loaded_dlls[i].name[j]; if (c1>='a' && c1<='z') c1-=32;
            char c2 = dll_name[j]; if (c2>='a' && c2<='z') c2-=32;
            if (c1 != c2) { match = false; break; }
            j++;
        }
        if (g_loaded_dlls[i].name[j] != dll_name[j]) match = false;
        if (match) return g_loaded_dlls[i].image_base;
    }
    
    if (g_num_loaded_dlls >= 32) return 0;
    
    // PRE-REGISTER TO PREVENT CIRCULAR DEPENDENCY INFINITE LOOPS!
    int slot = g_num_loaded_dlls;
    strncpy(g_loaded_dlls[slot].name, dll_name, 63);
    g_loaded_dlls[slot].image_base = 0; // 0 = currently loading
    g_num_loaded_dlls++;
    
    char path[256];
    snprintf(path, 256, "/opt/meinos/virtual_hdd/Windows/System32/%s", dll_name);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f);
    fclose(f);
    
    uint64_t entry = load_and_resolve_pe(buf, size);
    if (!entry) { free(buf); return 0; }
    
    uint32_t pe_offset = *(uint32_t*)(&buf[0x3C]);
    uint32_t ep_rva = *(uint32_t*)(&buf[pe_offset + 40]);
    uint64_t image_base = entry - ep_rva;
    free(buf);
    
    if (image_base) {
        init_teb32(image_base);
        g_loaded_dlls[slot].image_base = image_base;
        char logmsg[128];
        snprintf(logmsg, 128, "[HYBRID] LOADED NATIVE DLL: %s\n", dll_name);
        print_win(&windows[15], logmsg);
        
        // Execute DllMain safely if there is an entry point
        if (ep_rva > 0) {
#ifdef __linux__
            typedef int (__attribute__((ms_abi, force_align_arg_pointer)) *DllMain_t)(void*, uint32_t, void*);
#else
            typedef int (__attribute__((ms_abi)) *DllMain_t)(void*, uint32_t, void*);
#endif
            DllMain_t dll_main = (DllMain_t)entry;
            
            // Set up a local sigsetjmp for safety in case DllMain crashes
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_sigaction = segfault_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
            
            struct sigaction old_segv, old_ill, old_fpe, old_abrt;
            sigaction(SIGSEGV, &sa, &old_segv);
            sigaction(SIGILL, &sa, &old_ill);
            sigaction(SIGFPE, &sa, &old_fpe);
            sigaction(SIGABRT, &sa, &old_abrt);
            
            bool was_in_exe = in_exe_thread;
            in_exe_thread = true; // allow segfault_handler to jump
            
            sigjmp_buf old_env;
            memcpy(&old_env, &jump_env, sizeof(sigjmp_buf)); // Save old jump_env

            if (sigsetjmp(jump_env, 1) == 0) {
                dll_main((void*)image_base, 1 /* DLL_PROCESS_ATTACH */, NULL);
            } else {
                snprintf(logmsg, 128, "[SYS] DllMain crashed in %s\n", dll_name);
                print_win(&windows[15], logmsg);
            }
            
            memcpy(&jump_env, &old_env, sizeof(sigjmp_buf)); // Restore old jump_env

            in_exe_thread = was_in_exe;
            sigaction(SIGSEGV, &old_segv, NULL);
            sigaction(SIGILL, &old_ill, NULL);
            sigaction(SIGFPE, &old_fpe, NULL);
            sigaction(SIGABRT, &old_abrt, NULL);
        }
        
        return image_base;
    }
    return 0;
}

bool str_iequals(const char* a, const char* b) {
    while(*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

#ifdef __linux__
#define WIN_ABI __attribute__((ms_abi, force_align_arg_pointer))
#elif defined(__x86_64__)
#define WIN_ABI __attribute__((ms_abi))
#else
#define WIN_ABI __attribute__((stdcall))
#endif

extern "C" WIN_ABI int meinos_MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, uint32_t uType) {
    char msg[256];
    snprintf(msg, sizeof(msg), "\n[WIN32 MessageBox]: %s - %s\n", lpCaption ? lpCaption : "Info", lpText ? lpText : "");
    print_win(&windows[15], msg);
    return 6; // IDYES
}

extern "C" WIN_ABI void* meinos_GetStdHandle(uint32_t nStdHandle) {
    if (nStdHandle == (uint32_t)-11 || nStdHandle == (uint32_t)-12) return (void*)1;
    return 0;
}

extern "C" WIN_ABI int meinos_WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t* lpNumberOfBytesWritten, void* lpOverlapped) {
    uint64_t handle = (uint64_t)hFile;
    if (handle == 1 && lpBuffer) {
        char buf[256];
        uint32_t to_write = nNumberOfBytesToWrite < 255 ? nNumberOfBytesToWrite : 255;
        for(uint32_t i=0; i<to_write; i++) buf[i] = ((char*)lpBuffer)[i];
        buf[to_write] = 0;
        print_win(&windows[15], buf);
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        return 1;
    }
    extern FILE* meinos_file_handles[100];
    if (handle >= 1 && handle < 100 && meinos_file_handles[handle]) {
        size_t written = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, meinos_file_handles[handle]);
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = written;
        return 1;
    }
    return 0;
}

extern "C" void* dummy_stub_array[1000];
extern "C" const char* stub_name_0; // Just to get the address of the first one, though they are separate variables. We can use an array of pointers instead!
// Wait, I should make `generate_stubs.py` output an array of string pointers we can modify!

extern "C" const char** get_stub_name_ptr(int i);

uint64_t generate_dummy_stub(const char* func_name) {
    static int stub_count = 0;
    if (stub_count >= 1000) return (uint64_t)dummy_stub_array[999];
    
    *get_stub_name_ptr(stub_count) = strdup(func_name);
    
    uint64_t stub = (uint64_t)dummy_stub_array[stub_count];
    stub_count++;
    return stub;
}

extern "C" void* get_window_15() {
    return &windows[15];
}

extern "C" WIN_ABI uint32_t meinos_TlsAlloc();
extern "C" WIN_ABI void* meinos_TlsGetValue(uint32_t dwTlsIndex);
extern "C" WIN_ABI uint32_t meinos_TlsSetValue(uint32_t dwTlsIndex, void* lpTlsValue);
extern "C" WIN_ABI uint32_t meinos_TlsFree(uint32_t dwTlsIndex);
extern "C" WIN_ABI uint32_t meinos_FlsAlloc(void* lpCallback) { return meinos_TlsAlloc(); }
extern "C" WIN_ABI void* meinos_FlsGetValue(uint32_t dwFlsIndex) { return meinos_TlsGetValue(dwFlsIndex); }
extern "C" WIN_ABI bool meinos_FlsSetValue(uint32_t dwFlsIndex, void* lpFlsData) { return meinos_TlsSetValue(dwFlsIndex, lpFlsData) != 0; }
extern "C" WIN_ABI bool meinos_FlsFree(uint32_t dwFlsIndex) { return meinos_TlsFree(dwFlsIndex) != 0; }

extern "C" WIN_ABI void* meinos_GetProcessHeap() {
    return (void*)0x8888;
}

extern "C" WIN_ABI void* meinos_HeapAlloc(void* hHeap, uint32_t dwFlags, size_t dwBytes) {
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        void* ptr = meinos_alloc32(dwBytes);
        if (ptr) memset(ptr, 0, dwBytes);
        return ptr;
    }
    return calloc(1, dwBytes);
}

extern "C" WIN_ABI bool meinos_HeapFree(void* hHeap, uint32_t dwFlags, void* lpMem) {
    extern bool is_pe_32bit;
    if (is_pe_32bit) return true; // bump allocator doesn't free
    free(lpMem);
    return true;
}

extern "C" WIN_ABI uint32_t meinos_HeapSize(void* hHeap, uint32_t dwFlags, void* lpMem) {
    extern bool is_pe_32bit;
    if (!lpMem) return 0;
    if (is_pe_32bit) {
        uint32_t* header = (uint32_t*)((char*)lpMem - 16);
        return *header;
    }
    // We can't query size for standard malloc portably without malloc_usable_size, so just return a dummy
    // But 64-bit MeinOS natively uses standard allocator. For now return 0.
    return 0;
}

extern "C" WIN_ABI void meinos_InitializeCriticalSection(void* lpCriticalSection);

extern "C" WIN_ABI void meinos_InitializeCriticalSectionEx(void* lpCriticalSection, uint32_t dwSpinCount, uint32_t Flags) {
    meinos_InitializeCriticalSection(lpCriticalSection);
}

extern "C" WIN_ABI void* meinos_EncodePointer(void* ptr) {
    return ptr;
}

extern "C" WIN_ABI int meinos_IsProcessorFeaturePresent(uint32_t ProcessorFeature) {
    return 1;
}

static pthread_key_t meinos_tls_keys[100];
static bool meinos_tls_allocated[100] = {false};
static pthread_mutex_t meinos_tls_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" WIN_ABI uint32_t meinos_TlsAlloc() {
    pthread_mutex_lock(&meinos_tls_mutex);
    for (int i = 1; i < 100; i++) {
        if (!meinos_tls_allocated[i]) {
            meinos_tls_allocated[i] = true;
            pthread_key_create(&meinos_tls_keys[i], NULL);
            pthread_mutex_unlock(&meinos_tls_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&meinos_tls_mutex);
    return 0xFFFFFFFF; // TLS_OUT_OF_INDEXES
}

extern "C" WIN_ABI void* meinos_TlsGetValue(uint32_t dwTlsIndex) {
    if (dwTlsIndex >= 100 || !meinos_tls_allocated[dwTlsIndex]) return NULL;
    return pthread_getspecific(meinos_tls_keys[dwTlsIndex]);
}

extern "C" WIN_ABI uint32_t meinos_TlsSetValue(uint32_t dwTlsIndex, void* lpTlsValue) {
    if (dwTlsIndex >= 100 || !meinos_tls_allocated[dwTlsIndex]) return 0;
    pthread_setspecific(meinos_tls_keys[dwTlsIndex], lpTlsValue);
    return 1;
}

extern "C" WIN_ABI uint32_t meinos_TlsFree(uint32_t dwTlsIndex) {
    if (dwTlsIndex < 100 && meinos_tls_allocated[dwTlsIndex]) {
        meinos_tls_allocated[dwTlsIndex] = false;
        pthread_key_delete(meinos_tls_keys[dwTlsIndex]);
        pthread_mutex_unlock(&meinos_tls_mutex);
        return 1;
    }
    pthread_mutex_unlock(&meinos_tls_mutex);
    return 0;
}


extern "C" WIN_ABI bool meinos_InitializeCriticalSectionAndSpinCount(void* lpCriticalSection, uint32_t dwSpinCount) {
    meinos_InitializeCriticalSection(lpCriticalSection);
    return true;
}

extern "C" WIN_ABI void* meinos_HeapReAlloc(void* hHeap, uint32_t dwFlags, void* lpMem, uint32_t dwBytes) {
    if (!lpMem) return meinos_alloc32(dwBytes);
    uint32_t old_size = meinos_HeapSize(hHeap, 0, lpMem);
    void* new_mem = meinos_alloc32(dwBytes);
    if (new_mem) {
        uint32_t copy_size = (old_size < dwBytes) ? old_size : dwBytes;
        memcpy(new_mem, lpMem, copy_size);
    }
    return new_mem;
}

extern "C" WIN_ABI void* meinos_HeapCreate(uint32_t flOptions, uint32_t dwInitialSize, uint32_t dwMaximumSize) {
    return (void*)0x8888;
}

extern "C" WIN_ABI bool meinos_HeapDestroy(void* hHeap) {
    return true;
}


extern "C" WIN_ABI const char* meinos_GetCommandLineA() {
    static char* cmd32 = 0;
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        if (!cmd32) {
            cmd32 = (char*)meinos_alloc32(256);
            strcpy(cmd32, "\"C:\\program.exe\"");
        }
        return cmd32;
    }
    return "\"C:\\program.exe\"";
}

extern "C" WIN_ABI const uint16_t* meinos_GetCommandLineW() {
    static uint16_t* cmd32 = 0;
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        if (!cmd32) {
            cmd32 = (uint16_t*)meinos_alloc32(256);
            const uint16_t c[] = {'"','C',':','\\','p','r','o','g','r','a','m','.','e','x','e','"',0};
            for(int i=0; i<16; i++) cmd32[i] = c[i];
        }
        return cmd32;
    }
    static const uint16_t cmd[] = {'"','C',':','\\','p','r','o','g','r','a','m','.','e','x','e','"',0};
    return cmd;
}

extern "C" WIN_ABI const uint16_t* meinos_GetEnvironmentStringsW() {
    static uint16_t* env32 = 0;
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        if (!env32) {
            env32 = (uint16_t*)meinos_alloc32(256);
            // double null terminated
            env32[0] = 'P'; env32[1] = 'A'; env32[2] = 'T'; env32[3] = 'H'; env32[4] = '='; env32[5] = 'C'; env32[6] = ':'; env32[7] = '\\'; env32[8] = 0;
            env32[9] = 0;
        }
        return env32;
    }
    static const uint16_t env[] = {'P','A','T','H','=','C',':','\\',0,0};
    return env;
}

extern "C" WIN_ABI void meinos_GetLocalTime(void* lpSystemTime) {
    if (lpSystemTime) {
        // SYSTEMTIME is 16 bytes: wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds
        uint16_t* st = (uint16_t*)lpSystemTime;
        st[0] = 2026; // Year
        st[1] = 7;    // Month
        st[2] = 1;    // DayOfWeek (Monday)
        st[3] = 20;   // Day
        st[4] = 12;   // Hour
        st[5] = 0;    // Minute
        st[6] = 0;    // Second
        st[7] = 0;    // Milliseconds
    }
}

extern "C" WIN_ABI int meinos_lstrlenA(const char* lpString) {
    if (!lpString) return 0;
    int len = 0;
    while (lpString[len]) len++;
    return len;
}

extern "C" WIN_ABI int meinos_lstrlenW(const uint16_t* lpString) {
    if (!lpString) return 0;
    int len = 0;
    while (lpString[len]) len++;
    return len;
}

extern "C" WIN_ABI int meinos_LCMapStringEx(const uint16_t* lpLocaleName, uint32_t dwMapFlags, const uint16_t* lpSrcStr, int cchSrc, uint16_t* lpDestStr, int cchDest, void* lpVersionInformation, void* lpReserved, void* sortHandle) {
    int req = cchSrc;
    if (req < 0) {
        req = 0;
        if (lpSrcStr) {
            while (lpSrcStr[req]) req++;
            req++; // include null terminator
        }
    }
    if (cchDest == 0) return req;
    if (cchDest > 0 && lpDestStr) {
        int copy_len = req < cchDest ? req : cchDest;
        for (int i=0; i<copy_len; i++) {
            lpDestStr[i] = lpSrcStr[i];
        }
        if (copy_len < cchDest) lpDestStr[copy_len] = 0;
        return copy_len;
    }
    return 0; // Failure
}

extern "C" WIN_ABI uint32_t meinos_GetVersion() {
    return 0x00000000;
}

extern "C" WIN_ABI bool meinos_FreeEnvironmentStringsW(void* penv) {
    return true;
}

extern "C" WIN_ABI uint32_t meinos_GetACP() {
    return 1252;
}

extern "C" WIN_ABI uint32_t meinos_GetOEMCP() {
    return 437;
}

extern "C" WIN_ABI uint32_t meinos_GetFileType(void* hFile) {
    return 0x0002; // FILE_TYPE_CHAR (console)
}

extern "C" WIN_ABI void meinos_GetSystemTimeAsFileTime(void* lpSystemTimeAsFileTime) {
    // Just zero it
    if (lpSystemTimeAsFileTime) {
        *(uint64_t*)lpSystemTimeAsFileTime = 0;
    }
}

extern "C" WIN_ABI void meinos_GetStartupInfoW(void* lpStartupInfo) {
    // lpStartupInfo is a STARTUPINFOW struct.
    // Set cb (size) and other fields to 0
    if (lpStartupInfo) {
        extern bool is_pe_32bit;
        uint32_t size = is_pe_32bit ? 68 : 104;
        uint8_t* p = (uint8_t*)lpStartupInfo;
        for (uint32_t i = 0; i < size; i++) p[i] = 0;
        *(uint32_t*)p = size; // cb
    }
}

extern "C" WIN_ABI bool meinos_QueryPerformanceCounter(void* lpPerformanceCount) {
    if (lpPerformanceCount) *(uint64_t*)lpPerformanceCount = 0;
    return true;
}

extern "C" WIN_ABI uint32_t meinos_GetCurrentProcessId() { return 1; }
extern "C" WIN_ABI uint32_t meinos_GetCurrentThreadId() { return 1; }

extern "C" WIN_ABI bool meinos_QueryPerformanceFrequency(void* lpFrequency) {
    if (lpFrequency) *(uint64_t*)lpFrequency = 1000;
    return true;
}

static FILE* meinos_file_handles[100] = {NULL};
static int meinos_next_file_handle = 1;

extern "C" WIN_ABI uint32_t meinos_GetModuleFileNameA(void* hModule, char* lpFilename, uint32_t nSize) {
    if (nSize > 0 && lpFilename) { 
        extern char dsk_mgr_picked_file[256];
        if (dsk_mgr_picked_file[0] != 0) {
            strncpy(lpFilename, dsk_mgr_picked_file, nSize - 1);
            lpFilename[nSize - 1] = 0;
            int len = 0; while(lpFilename[len]) len++;
            printf("[SYS] GetModuleFileNameA returning: %s\n", dsk_mgr_picked_file);
            return len;
        }
        
        // Fallback if no file picked
        const char* fallback = "C:\\program.exe";
        strncpy(lpFilename, fallback, nSize - 1);
        lpFilename[nSize - 1] = 0;
        int len = 0; while(lpFilename[len]) len++;
        return len;
    }
    return 0;
}

extern "C" WIN_ABI uint32_t meinos_GetModuleFileNameW(void* hModule, uint16_t* lpFilename, uint32_t nSize) {
    printf("[SYS] GetModuleFileNameW(hModule=%p, lpFilename=%p, nSize=%u)\n", hModule, lpFilename, nSize);
    if (nSize > 0 && lpFilename) { 
        extern char dsk_mgr_picked_file[256];
        if (dsk_mgr_picked_file[0] != 0) {
            int len = 0;
            while(dsk_mgr_picked_file[len] && len < (int)nSize - 1) {
                lpFilename[len] = (uint16_t)dsk_mgr_picked_file[len];
                len++;
            }
            lpFilename[len] = 0;
            printf("[SYS] GetModuleFileNameW returning: %s\n", dsk_mgr_picked_file);
            return len;
        }
        
        // Fallback if no file picked
        const char* fallback = "C:\\program.exe";
        int len = 0;
        while(fallback[len] && len < (int)nSize - 1) {
            lpFilename[len] = (uint16_t)fallback[len];
            len++;
        }
        lpFilename[len] = 0;
        return len;
    }
    return 0;
}

extern "C" WIN_ABI void* meinos_CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode, void* lpSecurityAttributes, uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes, void* hTemplateFile) {
    if (meinos_next_file_handle >= 100) return (void*)0xFFFFFFFFFFFFFFFF;
    
    bool want_write = (dwDesiredAccess & 0x40000000) || (dwDesiredAccess & 0x0002) || (dwDesiredAccess & 0x01000000);
    const char* mode = "rb";
    
    if (want_write) {
        if (dwCreationDisposition == 1 || dwCreationDisposition == 2 || dwCreationDisposition == 5) {
            mode = "wb+";
        } else {
            FILE* check = fopen(lpFileName, "rb");
            if (check) { fclose(check); mode = "r+b"; }
            else mode = "wb+";
        }
    } else {
        mode = "rb";
    }
    
    char safe_path[512];
    if (lpFileName) {
        int j = 0;
        int start = 0;
        if (lpFileName[0] == 'C' && lpFileName[1] == ':') start = 2; // Strip C:
        if (lpFileName[0] == 'Z' && lpFileName[1] == ':') start = 2; // Strip Z:
        if (lpFileName[start] == '\\' || lpFileName[start] == '/') start++; // Strip leading slash
        for(int i=start; lpFileName[i] && j < 510; i++) {
            if (lpFileName[i] == '\\') safe_path[j++] = '/';
            else safe_path[j++] = lpFileName[i];
        }
        safe_path[j] = 0;
        if (safe_path[0] == 0) strcpy(safe_path, "default_file.bin");
    } else {
        strcpy(safe_path, "null_file.bin");
    }
    
    printf("[SYS] CreateFileA: %s -> %s (mode: %s)\n", lpFileName ? lpFileName : "NULL", safe_path, mode);
    FILE* f = fopen(safe_path, mode);
    if (!f) {
        printf("[SYS] CreateFileA FAILED for %s (mapped to %s)\n", lpFileName, safe_path);
        return (void*)0xFFFFFFFFFFFFFFFF;
    }
    
    meinos_file_handles[meinos_next_file_handle] = f;
    return (void*)(uint64_t)meinos_next_file_handle++;
}

extern "C" WIN_ABI void* meinos_CreateFileW(const uint16_t* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode, void* lpSecurityAttributes, uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes, void* hTemplateFile) {
    char pathA[256];
    int i = 0;
    while(lpFileName[i] && i < 255) { pathA[i] = (char)lpFileName[i]; i++; }
    pathA[i] = 0;
    return meinos_CreateFileA(pathA, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

extern "C" WIN_ABI bool meinos_ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead, uint32_t* lpNumberOfBytesRead, void* lpOverlapped) {
    uint64_t handle = (uint64_t)hFile;
    if (handle < 1 || handle >= 100 || !meinos_file_handles[handle]) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return false;
    }
    size_t read = fread(lpBuffer, 1, nNumberOfBytesToRead, meinos_file_handles[handle]);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = read;
    return true;
}

extern "C" WIN_ABI uint32_t meinos_GetFileSize(void* hFile, uint32_t* lpFileSizeHigh) {
    uint64_t handle = (uint64_t)hFile;
    if (handle < 1 || handle >= 100 || !meinos_file_handles[handle]) return 0xFFFFFFFF;
    
    long cur = ftell(meinos_file_handles[handle]);
    fseek(meinos_file_handles[handle], 0, SEEK_END);
    long size = ftell(meinos_file_handles[handle]);
    fseek(meinos_file_handles[handle], cur, SEEK_SET);
    
    if (lpFileSizeHigh) *lpFileSizeHigh = (size >> 32);
    return (uint32_t)size;
}

extern "C" WIN_ABI bool meinos_SetFilePointerEx(void* hFile, int64_t liDistanceToMove, int64_t* lpNewFilePointer, uint32_t dwMoveMethod) {
    uint64_t handle = (uint64_t)hFile;
    if (handle < 1 || handle >= 100 || !meinos_file_handles[handle]) return false;
    
    int origin = SEEK_SET;
    if (dwMoveMethod == 1) origin = SEEK_CUR;
    if (dwMoveMethod == 2) origin = SEEK_END;
    
    fseek(meinos_file_handles[handle], liDistanceToMove, origin);
    if (lpNewFilePointer) *lpNewFilePointer = ftell(meinos_file_handles[handle]);
    return true;
}

extern "C" WIN_ABI uint32_t meinos_SetFilePointer(void* hFile, int32_t lDistanceToMove, int32_t* lpDistanceToMoveHigh, uint32_t dwMoveMethod) {
    uint64_t handle = (uint64_t)hFile;
    if (handle < 1 || handle >= 100 || !meinos_file_handles[handle]) return 0xFFFFFFFF;
    
    int origin = SEEK_SET;
    if (dwMoveMethod == 1) origin = SEEK_CUR;
    if (dwMoveMethod == 2) origin = SEEK_END;
    
    int64_t dist = lDistanceToMove;
    if (lpDistanceToMoveHigh) {
        dist = ((int64_t)(*lpDistanceToMoveHigh) << 32) | (uint32_t)lDistanceToMove;
    }
    
    fseek(meinos_file_handles[handle], dist, origin);
    int64_t pos = ftell(meinos_file_handles[handle]);
    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = (pos >> 32);
    return (uint32_t)pos;
}

extern "C" WIN_ABI int meinos_CloseHandle(void* hObject); // Add declaration
extern "C" WIN_ABI int meinos_GetSystemMetrics(int nIndex) {
    if (nIndex == 0) return 1024; // SM_CXSCREEN
    if (nIndex == 1) return 768;  // SM_CYSCREEN
    return 0;
}

extern "C" WIN_ABI void meinos_GetSystemInfo(void* lpSystemInfo) {
}

extern "C" WIN_ABI bool meinos_GlobalMemoryStatusEx(void* lpBuffer) {
    return true;
}


struct WinRegEntry {
    char key_path[256];
    char value_name[128];
    uint32_t type; // 1 = REG_SZ, 4 = REG_DWORD
    char data_str[256];
    uint32_t data_dword;
};

static WinRegEntry g_virtual_registry[32] = {
    {"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", 1, "Windows 10 Pro (MeinOS Hybrid Engine)", 0},
    {"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", 1, "19045", 0},
    {"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CSDVersion", 1, "Service Pack 1", 0},
    {"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir", 1, "C:\\Program Files", 0},
    {"SYSTEM\\CurrentControlSet\\Control", "SystemStartOptions", 1, "NOEXEC", 0},
    {"SOFTWARE\\MeinOS", "Version", 1, "2.2.0", 0}
};
static int g_num_reg_entries = 6;

extern "C" WIN_ABI uint32_t meinos_RegOpenKeyExA(void* hKey, const char* lpSubKey, uint32_t ulOptions, uint32_t samDesired, void** phkResult) {
    if (phkResult) *phkResult = (void*)0x80000001; // Valid pseudo HKEY
    return 0; // ERROR_SUCCESS
}

extern "C" WIN_ABI uint32_t meinos_RegOpenKeyExW(void* hKey, const uint16_t* lpSubKey, uint32_t ulOptions, uint32_t samDesired, void** phkResult) {
    if (phkResult) *phkResult = (void*)0x80000001;
    return 0; // ERROR_SUCCESS
}

extern "C" WIN_ABI uint32_t meinos_RegQueryValueExA(void* hKey, const char* lpValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) {
    if (!lpValueName) return 2; // ERROR_FILE_NOT_FOUND
    for (int i = 0; i < g_num_reg_entries; i++) {
        if (str_iequals(g_virtual_registry[i].value_name, lpValueName)) {
            if (lpType) *lpType = g_virtual_registry[i].type;
            if (g_virtual_registry[i].type == 1) { // REG_SZ
                uint32_t len = (uint32_t)strlen(g_virtual_registry[i].data_str) + 1;
                if (lpcbData) *lpcbData = len;
                if (lpData) strncpy((char*)lpData, g_virtual_registry[i].data_str, len);
            } else if (g_virtual_registry[i].type == 4) { // REG_DWORD
                if (lpcbData) *lpcbData = sizeof(uint32_t);
                if (lpData) *(uint32_t*)lpData = g_virtual_registry[i].data_dword;
            }
            return 0; // ERROR_SUCCESS
        }
    }
    return 2; // ERROR_FILE_NOT_FOUND
}

extern "C" WIN_ABI uint32_t meinos_RegQueryValueExW(void* hKey, const uint16_t* lpValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) {
    if (!lpValueName) return 2;
    char valA[128] = {0};
    for (int i = 0; i < 127 && lpValueName[i]; i++) valA[i] = (char)lpValueName[i];
    return meinos_RegQueryValueExA(hKey, valA, lpReserved, lpType, lpData, lpcbData);
}

extern "C" WIN_ABI uint32_t meinos_RegCreateKeyExA(void* hKey, const char* lpSubKey, uint32_t Reserved, char* lpClass, uint32_t dwOptions, uint32_t samDesired, void* lpSecurityAttributes, void** phkResult, uint32_t* lpdwDisposition) {
    if (phkResult) *phkResult = (void*)0x80000001;
    if (lpdwDisposition) *lpdwDisposition = 1; // REG_CREATED_NEW_KEY
    return 0; // ERROR_SUCCESS
}

extern "C" WIN_ABI uint32_t meinos_RegSetValueExA(void* hKey, const char* lpValueName, uint32_t Reserved, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) {
    if (!lpValueName || !lpData) return 0;
    for (int i = 0; i < g_num_reg_entries; i++) {
        if (str_iequals(g_virtual_registry[i].value_name, lpValueName)) {
            g_virtual_registry[i].type = dwType;
            if (dwType == 1) strncpy(g_virtual_registry[i].data_str, (const char*)lpData, 255);
            else if (dwType == 4) g_virtual_registry[i].data_dword = *(const uint32_t*)lpData;
            return 0;
        }
    }
    if (g_num_reg_entries < 32) {
        strncpy(g_virtual_registry[g_num_reg_entries].key_path, "SOFTWARE\\Custom", 255);
        strncpy(g_virtual_registry[g_num_reg_entries].value_name, lpValueName, 127);
        g_virtual_registry[g_num_reg_entries].type = dwType;
        if (dwType == 1) strncpy(g_virtual_registry[g_num_reg_entries].data_str, (const char*)lpData, 255);
        else if (dwType == 4) g_virtual_registry[g_num_reg_entries].data_dword = *(const uint32_t*)lpData;
        g_num_reg_entries++;
    }
    return 0; // ERROR_SUCCESS
}

extern "C" WIN_ABI uint32_t meinos_RegCloseKey(void* hKey) {
    return 0; // ERROR_SUCCESS
}
extern "C" WIN_ABI int meinos_GetDeviceCaps(void* hdc, int index) {
    return 0;
}
extern "C" WIN_ABI uint32_t meinos_GetFileAttributesW(const uint16_t* lpFileName) {
    if (!lpFileName) return 0xFFFFFFFF;
    if (lpFileName[0] == 'C' && lpFileName[1] == ':' && lpFileName[2] == '\\' && lpFileName[3] == 0) {
        return 0x10; // FILE_ATTRIBUTE_DIRECTORY
    }
    
    char pathA[512];
    int i = 0;
    while(lpFileName[i] && i < 510) { pathA[i] = (char)lpFileName[i]; i++; }
    pathA[i] = 0;
    
    // Quick sanitization
    char* final_path = pathA;
    if (pathA[0] == '.' && pathA[1] == '\\') final_path = pathA + 2;
    if (pathA[0] == 'C' && pathA[1] == ':') final_path = pathA + 2;
    if (pathA[0] == 'Z' && pathA[1] == ':') final_path = pathA + 2;
    if (final_path[0] == '\\' || final_path[0] == '/') final_path++;
    for(int j=0; final_path[j]; j++) if (final_path[j] == '\\') final_path[j] = '/';
    
    FILE* f = fopen(final_path, "rb");
    if (f) {
        fclose(f);
        return 0x80; // FILE_ATTRIBUTE_NORMAL
    }
    return 0xFFFFFFFF; // INVALID_FILE_ATTRIBUTES
}
extern "C" WIN_ABI int meinos_MessageBoxW(void* hWnd, const uint16_t* lpText, const uint16_t* lpCaption, uint32_t uType) {
    char buf[256] = {0};
    if (lpText) { for (int i=0; i<255 && lpText[i]; i++) buf[i] = (char)lpText[i]; }
    printf("\n[WIN32 MessageBoxW]: %s\n", buf);
    return 6; // IDYES
}
extern "C" WIN_ABI int meinos_MessageBoxExW(void* hWnd, const uint16_t* lpText, const uint16_t* lpCaption, uint32_t uType, uint16_t wLanguageId) {
    char buf[256] = {0};
    if (lpText) { for (int i=0; i<255 && lpText[i]; i++) buf[i] = (char)lpText[i]; }
    printf("\n[WIN32 MessageBoxExW]: %s\n", buf);
    return 6; // IDYES
}

extern "C" WIN_ABI bool meinos_SetProcessDefaultLayout(uint32_t dwDefaultLayout) {
    return true;
}

extern "C" WIN_ABI void* meinos_CreateWindowExA(uint32_t, const char*, const char*, uint32_t, int, int, int, int, void*, void*, void*, void*);

extern "C" WIN_ABI void* meinos_CreateDialogIndirectParamW(void* hInstance, const void* lpTemplate, void* hWndParent, void* lpDialogFunc, uint64_t dwInitParam) {
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
    
    int s_x = (x * 3) / 2;
    int s_y = (y * 9) / 5;
    int s_w = (w * 3) / 2;
    int s_h = (h * 9) / 5;

    void* hwnd = meinos_CreateWindowExA(exStyle, "Dialog", title, style, s_x, s_y, s_w, s_h, hWndParent, 0, hInstance, 0);
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
        
        int s_ix = (ix * 3) / 2;
        int s_iy = (iy * 9) / 5;
        int s_iw = (iw * 3) / 2;
        int s_ih = (ih * 9) / 5;

        meinos_CreateWindowExA(0, class_name, item_title, istyle | 0x40000000, s_ix, s_iy, s_iw, s_ih, hwnd, (void*)(uint64_t)id, hInstance, 0);
    }
    
    uint64_t slot = (uint64_t)hwnd;
    if (slot < 20) {
        windows[slot].wndproc = (uint64_t)lpDialogFunc;
        if (lpDialogFunc) {
            uint64_t (WIN_ABI *dlgproc)(uint64_t, uint32_t, uint64_t, uint64_t) = (uint64_t (WIN_ABI *)(uint64_t, uint32_t, uint64_t, uint64_t))lpDialogFunc;
            dlgproc(slot, 0x0110 /* WM_INITDIALOG */, (uint64_t)hwnd, dwInitParam);
        }
    }

    return hwnd;
}

extern "C" WIN_ABI int meinos_EnumFontFamiliesExA(void* hdc, void* lpLogfont, void* lpProc, uint64_t lParam, uint32_t dwFlags) {
    return 1; // success
}

extern "C" WIN_ABI bool meinos_IsWindowVisible(void* hWnd) {
    return true;
}

extern "C" WIN_ABI short meinos_GetKeyState(int nVirtKey) {
    return 0;
}

extern "C" WIN_ABI bool meinos_IsDialogMessageW(void* hDlg, void* lpMsg) {
    return false;
}

extern "C" WIN_ABI bool meinos_UpdateWindow(void* hWnd) {
    return true;
}

extern "C" WIN_ABI bool meinos_ChangeWindowMessageFilterEx(void* hWnd, uint32_t message, uint32_t action, void* pChangeFilterStruct) {
    return true;
}

extern "C" WIN_ABI int meinos_LCIDToLocaleName(uint32_t Locale, uint16_t* lpName, int cchName, uint32_t dwFlags) {
    if (lpName && cchName > 5) {
        lpName[0] = 'e'; lpName[1] = 'n'; lpName[2] = '-'; lpName[3] = 'U'; lpName[4] = 'S'; lpName[5] = 0;
        return 6;
    }
    return 0;
}

extern "C" WIN_ABI int meinos_LoadStringW(void* hInstance, uint32_t uID, uint16_t* lpBuffer, int cchBufferMax) {
    if (lpBuffer && cchBufferMax > 1) {
        lpBuffer[0] = 'R'; lpBuffer[1] = 'u'; lpBuffer[2] = 'f'; lpBuffer[3] = 'u'; lpBuffer[4] = 's'; lpBuffer[5] = 0;
        return 5;
    }
    return 0;
}

extern "C" WIN_ABI void* meinos_SetWinEventHook(uint32_t eventMin, uint32_t eventMax, void* hmodWinEventProc, void* pfnWinEventProc, uint32_t idProcess, uint32_t idThread, uint32_t dwFlags) {
    return (void*)0x1111;
}

extern "C" WIN_ABI bool meinos_SetCurrentDirectoryA(const char* lpPathName) {
    return false;
}
extern "C" WIN_ABI bool meinos_UnhookWinEvent(void* hWinEventHook) {
    return true;
}
extern "C" WIN_ABI bool meinos_DeleteFileA(const char* lpFileName) {
    return false;
}
extern "C" WIN_ABI bool meinos_CreateProcessA(const char* lpApplicationName, char* lpCommandLine, void* lpProcessAttributes, void* lpThreadAttributes, bool bInheritHandles, uint32_t dwCreationFlags, void* lpEnvironment, const char* lpCurrentDirectory, void* lpStartupInfo, void* lpProcessInformation) {
    return false;
}
extern "C" WIN_ABI bool meinos_CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes) {
    return false;
}
extern "C" WIN_ABI uint32_t meinos_MsgWaitForMultipleObjects(uint32_t nCount, void** pHandles, bool fWaitAll, uint32_t dwMilliseconds, uint32_t dwWakeMask);
extern "C" WIN_ABI int meinos_LoadStringA(void* hInstance, uint32_t uID, char* lpBuffer, int cchBufferMax) {
    if (cchBufferMax > 0 && lpBuffer) lpBuffer[0] = 0;
    return 0;
}
extern "C" WIN_ABI bool meinos_ExitWindowsEx(uint32_t uFlags, uint32_t dwReason) {
    return false;
}
extern "C" WIN_ABI void meinos_InitCommonControls() {
}
extern "C" WIN_ABI bool meinos_InitCommonControlsEx(void* picce) {
    return true;
}

extern "C" WIN_ABI uint32_t meinos_GetThreadLocale() { return 1033; }
extern "C" WIN_ABI bool meinos_SetThreadLocale(uint32_t Locale) { return true; }
extern "C" WIN_ABI uint32_t meinos_FormatMessageW(uint32_t dwFlags, const void* lpSource, uint32_t dwMessageId, uint32_t dwLanguageId, uint16_t* lpBuffer, uint32_t nSize, void* Arguments) {
    if (nSize > 0 && lpBuffer) lpBuffer[0] = 0;
    return 0;
}
extern "C" WIN_ABI uint32_t meinos_FormatMessageA(uint32_t dwFlags, const void* lpSource, uint32_t dwMessageId, uint32_t dwLanguageId, char* lpBuffer, uint32_t nSize, void* Arguments) {
    if (nSize > 0 && lpBuffer) lpBuffer[0] = 0;
    return 0;
}
extern "C" WIN_ABI uint32_t meinos_GetTempFileNameW(const uint16_t* lpPathName, const uint16_t* lpPrefixString, uint32_t uUnique, uint16_t* lpTempFileName) {
    if (lpTempFileName) {
        lpTempFileName[0] = 'C'; lpTempFileName[1] = ':'; lpTempFileName[2] = '\\';
        lpTempFileName[3] = 'T'; lpTempFileName[4] = 'M'; lpTempFileName[5] = 'P'; lpTempFileName[6] = 0;
    }
    return 1;
}
extern "C" WIN_ABI bool meinos_SetCurrentDirectoryW(const uint16_t* lpPathName) { return false; }
extern "C" WIN_ABI bool meinos_DeleteFileW(const uint16_t* lpFileName) { return false; }
extern "C" WIN_ABI int meinos_CoInitialize(void* pvReserved) { return 0; }
extern "C" WIN_ABI uint32_t meinos_GetEnvironmentVariableA(const char* lpName, char* lpBuffer, uint32_t nSize) {
    if (nSize > 0 && lpBuffer) lpBuffer[0] = 0;
    return 0;
}
extern "C" WIN_ABI uint32_t meinos_InterlockedExchange(uint32_t volatile *Target, uint32_t Value) {
    uint32_t old = *Target;
    *Target = Value;
    return old;
}

extern "C" WIN_ABI bool meinos_GetExitCodeProcess(void* hProcess, uint32_t* lpExitCode) {
    if (lpExitCode) *lpExitCode = 0;
    return true;
}

extern "C" WIN_ABI bool meinos_AttachConsole(uint32_t dwProcessId) {
    return true;
}

extern "C" WIN_ABI int meinos___wgetmainargs(int* _Argc, uint16_t*** _Argv, uint16_t*** _Env, int _DoWildCard, void* _StartInfo) {
    if (_Argc) *_Argc = 0;
    if (_Argv) *_Argv = 0;
    if (_Env) *_Env = 0;
    return 0;
}

extern "C" WIN_ABI uint32_t meinos_GetTempPathW(uint32_t nBufferLength, uint16_t* lpBuffer) {
    if (nBufferLength > 3 && lpBuffer) {
        lpBuffer[0] = 'C'; lpBuffer[1] = ':'; lpBuffer[2] = '\\'; lpBuffer[3] = 0;
        return 3;
    }
    return 0;
}

extern "C" WIN_ABI bool meinos_GetVersionExA(uint32_t* lpVersionInformation) {
    if (!lpVersionInformation) return false;
    uint32_t size = lpVersionInformation[0];
    if (size >= 20) {
        lpVersionInformation[1] = 10; // Major
        lpVersionInformation[2] = 0; // Minor
        lpVersionInformation[3] = 19045; // Build
        lpVersionInformation[4] = 2; // Platform = VER_PLATFORM_WIN32_NT
        char* szCSDVersion = (char*)&lpVersionInformation[5];
        szCSDVersion[0] = 0; // No service pack
    }
    return true;
}

extern "C" WIN_ABI bool meinos_GetVersionExW(uint32_t* lpVersionInformation) {
    if (!lpVersionInformation) return false;
    uint32_t size = lpVersionInformation[0];
    if (size >= 20) {
        lpVersionInformation[1] = 10; // Major
        lpVersionInformation[2] = 0; // Minor
        lpVersionInformation[3] = 19045; // Build
        lpVersionInformation[4] = 2; // Platform = VER_PLATFORM_WIN32_NT
        uint16_t* szCSDVersion = (uint16_t*)&lpVersionInformation[5];
        szCSDVersion[0] = 0;
    }
    return true;
}

extern "C" WIN_ABI void* meinos_FindResourceA(void* hModule, const char* lpName, const char* lpType);

extern "C" WIN_ABI uint32_t meinos_GetFileVersionInfoSizeW(const uint16_t* lptstrFilename, uint32_t* lpdwHandle) {
    if (lpdwHandle) *lpdwHandle = 0;
    uint32_t* data = (uint32_t*)meinos_FindResourceA(0, (const char*)1, (const char*)16);
    if (data) return data[1];
    return 0;
}

extern "C" WIN_ABI bool meinos_GetFileVersionInfoW(const uint16_t* lptstrFilename, uint32_t dwHandle, uint32_t dwLen, void* lpData) {
    uint32_t* data = (uint32_t*)meinos_FindResourceA(0, (const char*)1, (const char*)16);
    if (!data) return false;
    extern uint64_t meinos_main_exe_base;
    void* src = (void*)(meinos_main_exe_base + data[0]);
    if (dwLen > data[1]) dwLen = data[1];
    if (lpData && src && dwLen > 0) {
        memcpy(lpData, src, dwLen);
        return true;
    }
    return false;
}

extern "C" WIN_ABI bool meinos_VerQueryValueW(const void* pBlock, const uint16_t* lpSubBlock, void** lplpBuffer, uint32_t* puLen) {
    // Basic fake VerQueryValueW that returns a fake VS_FIXEDFILEINFO
    static uint32_t fake_fixed_file_info[13] = {
        0xFEEF04BD, 0x00010000, // dwSignature, dwStrucVersion
        0x0004000E, 0x00000000, // dwFileVersionMS (4.14), dwFileVersionLS
        0x0004000E, 0x00000000, // dwProductVersionMS (4.14), dwProductVersionLS
        0x0000003F, 0x00000000, // dwFileFlagsMask, dwFileFlags
        0x00000004, 0x00000001, // dwFileOS (NT), dwFileType (APP)
        0x00000000, 0x00000000, 0x00000000 // dwFileSubtype, dwFileDateMS, dwFileDateLS
    };
    if (lpSubBlock && lpSubBlock[0] == '\\' && lpSubBlock[1] == 0) {
        if (lplpBuffer) *lplpBuffer = fake_fixed_file_info;
        if (puLen) *puLen = sizeof(fake_fixed_file_info);
        return true;
    }
    // Return empty for strings to avoid crashing
    static uint16_t empty_str[2] = {0};
    if (lplpBuffer) *lplpBuffer = empty_str;
    if (puLen) *puLen = 0;
    return true;
}

extern "C" WIN_ABI bool meinos_VerQueryValueA(const void* pBlock, const char* lpSubBlock, void** lplpBuffer, uint32_t* puLen) {
    static uint32_t fake_fixed_file_info[13] = {
        0xFEEF04BD, 0x00010000,
        0x0004000E, 0x00000000,
        0x0004000E, 0x00000000,
        0x0000003F, 0x00000000,
        0x00000004, 0x00000001,
        0x00000000, 0x00000000, 0x00000000
    };
    if (lpSubBlock && lpSubBlock[0] == '\\' && lpSubBlock[1] == 0) {
        if (lplpBuffer) *lplpBuffer = fake_fixed_file_info;
        if (puLen) *puLen = sizeof(fake_fixed_file_info);
        return true;
    }
    static char empty_str[2] = {0};
    if (lplpBuffer) *lplpBuffer = empty_str;
    if (puLen) *puLen = 0;
    return true;
}

extern "C" WIN_ABI void* meinos_CreateMutexA(void* lpMutexAttributes, bool bInitialOwner, const char* lpName) {
    return (void*)0x12345678;
}

extern "C" WIN_ABI void* meinos_CreateMutexW(void* lpMutexAttributes, bool bInitialOwner, const uint16_t* lpName) {
    return (void*)0x12345678;
}

extern "C" WIN_ABI bool meinos_AllocateAndInitializeSid(void* pIdentifierAuthority, uint8_t nSubAuthorityCount, uint32_t nSubAuthority0, uint32_t nSubAuthority1, uint32_t nSubAuthority2, uint32_t nSubAuthority3, uint32_t nSubAuthority4, uint32_t nSubAuthority5, uint32_t nSubAuthority6, uint32_t nSubAuthority7, void** pSid) {
    if (pSid) {
        uint8_t* sid = (uint8_t*)malloc(8 + 4 * nSubAuthorityCount);
        sid[0] = 1; // Revision
        sid[1] = nSubAuthorityCount;
        if (pIdentifierAuthority) {
            uint8_t* auth = (uint8_t*)pIdentifierAuthority;
            for(int i=0; i<6; i++) sid[2+i] = auth[i];
        }
        uint32_t* sub_auth = (uint32_t*)(sid + 8);
        if (nSubAuthorityCount > 0) sub_auth[0] = nSubAuthority0;
        if (nSubAuthorityCount > 1) sub_auth[1] = nSubAuthority1;
        if (nSubAuthorityCount > 2) sub_auth[2] = nSubAuthority2;
        if (nSubAuthorityCount > 3) sub_auth[3] = nSubAuthority3;
        if (nSubAuthorityCount > 4) sub_auth[4] = nSubAuthority4;
        if (nSubAuthorityCount > 5) sub_auth[5] = nSubAuthority5;
        if (nSubAuthorityCount > 6) sub_auth[6] = nSubAuthority6;
        if (nSubAuthorityCount > 7) sub_auth[7] = nSubAuthority7;
        *pSid = sid;
    }
    return true;
}

extern "C" WIN_ABI bool meinos_CheckTokenMembership(void* TokenHandle, void* SidToCheck, int* IsMember) {
    if (IsMember) *IsMember = 0; // Default to false
    if (SidToCheck) {
        uint8_t* sid = (uint8_t*)SidToCheck;
        uint8_t sub_count = sid[1];
        if (sub_count > 0) {
            uint32_t* sub_auth = (uint32_t*)(sid + 8);
            uint32_t last_rid = sub_auth[sub_count - 1];
            printf("[WIN32] CheckTokenMembership checking RID: %u\n", last_rid);
            if (last_rid == 544 || last_rid == 0x2000) { // DOMAIN_ALIAS_RID_ADMINS or SECURITY_MANDATORY_HIGH_RID
                if (IsMember) *IsMember = 1;
            }
        }
    }
    return true;
}

extern "C" WIN_ABI void* meinos_FreeSid(void* pSid) {
    return 0;
}

extern "C" WIN_ABI int meinos_WldpQueryWindowsLockdownMode(int* lockdownMode) {
    if (lockdownMode) *lockdownMode = 0; // UNLOCKED
    return 0; // S_OK
}

extern "C" WIN_ABI int meinos_NtOpenProcessToken(void* ProcessHandle, uint32_t DesiredAccess, void** TokenHandle) {
    if (TokenHandle) *TokenHandle = (void*)1;
    return 0; // STATUS_SUCCESS
}

extern "C" WIN_ABI int meinos_NtAdjustPrivilegesToken(void* TokenHandle, bool DisableAllPrivileges, void* NewState, uint32_t BufferLength, void* PreviousState, uint32_t* ReturnLength) {
    return 0; // STATUS_SUCCESS
}

extern "C" WIN_ABI int meinos_NtClose(void* Handle) {
    return 0; // STATUS_SUCCESS
}

extern "C" WIN_ABI bool meinos_SetDefaultDllDirectories(uint32_t DirectoryFlags) {
    return true;
}

extern "C" WIN_ABI bool meinos_GetUserNameA(char* lpBuffer, uint32_t* pcbBuffer) {
    if (lpBuffer && pcbBuffer && *pcbBuffer > 4) {
        lpBuffer[0] = 'U'; lpBuffer[1] = 's'; lpBuffer[2] = 'e'; lpBuffer[3] = 'r'; lpBuffer[4] = 0;
        *pcbBuffer = 5;
        return true;
    }
    return false;
}

extern "C" WIN_ABI uint32_t meinos_GetFinalPathNameByHandleW(void* hFile, uint16_t* lpszFilePath, uint32_t cchFilePath, uint32_t dwFlags) {
    if (lpszFilePath && cchFilePath > 0) lpszFilePath[0] = 0;
    return 0;
}

extern "C" WIN_ABI uint32_t meinos_GetSystemDirectoryW(uint16_t* lpBuffer, uint32_t uSize) {
    if (lpBuffer && uSize > 3) {
        lpBuffer[0] = 'C'; lpBuffer[1] = ':'; lpBuffer[2] = '\\'; lpBuffer[3] = 0;
        return 3;
    }
    return 0;
}

extern "C" WIN_ABI bool meinos_SHGetSpecialFolderPathW(void* hwndOwner, uint16_t* lpszPath, int csidl, bool fCreate) {
    if (lpszPath) {
        lpszPath[0] = 'C'; lpszPath[1] = ':'; lpszPath[2] = '\\'; lpszPath[3] = 0;
    }
    return true;
}

extern "C" WIN_ABI uint32_t meinos_GetFileAttributesA(const char* lpFileName) {
    if (!lpFileName) return 0xFFFFFFFF;
    if (lpFileName[0] == 'C' && lpFileName[1] == ':' && lpFileName[2] == '\\' && lpFileName[3] == 0) return 0x10;
    
    char pathA[512];
    int i = 0;
    while(lpFileName[i] && i < 510) { pathA[i] = lpFileName[i]; i++; }
    pathA[i] = 0;
    
    char* final_path = pathA;
    if (pathA[0] == '.' && pathA[1] == '\\') final_path = pathA + 2;
    for(int j=0; final_path[j]; j++) if (final_path[j] == '\\') final_path[j] = '/';
    
    FILE* f = fopen(final_path, "rb");
    if (f) {
        fclose(f);
        return 0x80;
    }
    return 0xFFFFFFFF;
}

extern "C" WIN_ABI bool meinos_IsValidCodePage(uint32_t CodePage) {
    return true;
}

extern "C" WIN_ABI void meinos_InitializeSListHead(void* ListHead) {
    if (ListHead) {
        ((uint64_t*)ListHead)[0] = 0;
        ((uint64_t*)ListHead)[1] = 0;
    }
}

extern "C" WIN_ABI uint16_t meinos_GetUserDefaultUILanguage() {
    return 0x0409; // EN-US
}

extern "C" WIN_ABI bool meinos_GetCPInfo(uint32_t CodePage, void* lpCPInfo) {
    if (lpCPInfo) {
        ((uint32_t*)lpCPInfo)[0] = 1; // MaxCharSize = 1
        ((uint8_t*)lpCPInfo)[4] = '?'; // DefaultChar[0]
        ((uint8_t*)lpCPInfo)[5] = 0;   // DefaultChar[1]
        for (int i=0; i<12; i++) ((uint8_t*)lpCPInfo)[6+i] = 0; // LeadByte
    }
    return true;
}

extern "C" WIN_ABI void meinos_SetLastError(uint32_t dwErrCode) { }
extern "C" WIN_ABI uint32_t meinos_GetLastError() { return 0; }

static char dummy_iob[1024];
extern "C" WIN_ABI void* meinos_iob_func() {
    return dummy_iob;
}

extern "C" WIN_ABI size_t meinos_fwrite(const void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t total = size * nmemb;
    char msg[1024];
    snprintf(msg, sizeof(msg), "[CRT ERROR] %.*s\n", (int)total, (const char*)ptr);
    printf("%s", msg);
    return nmemb;
}

extern "C" WIN_ABI uint32_t meinos_GetTickCount() { return 0; }
extern "C" WIN_ABI uint64_t meinos_GetTickCount64() { return 0; }

extern "C" WIN_ABI bool meinos_PathFileExistsW(const uint16_t* pszPath) {
    return true;
}

extern "C" WIN_ABI void* meinos_SetUnhandledExceptionFilter(void* lpTopLevelExceptionFilter) {
    return 0;
}

extern "C" WIN_ABI int meinos_UnhandledExceptionFilter(void* ExceptionInfo) {
    return 0;
}
extern "C" WIN_ABI int meinos_fputs(const char *str, void *stream) {
    char msg[1024];
    snprintf(msg, sizeof(msg), "[CRT ERROR FPUTS] %s", str);
    printf("%s", msg);
    return 0;
}

extern "C" WIN_ABI void meinos_RaiseException(uint32_t dwExceptionCode, uint32_t dwExceptionFlags, uint32_t nNumberOfArguments, const uint64_t *lpArguments) {
    printf("[SYS] RaiseException called: Code 0x%X\n", dwExceptionCode);
}
uint64_t meinos_last_wndproc = 0;

extern "C" WIN_ABI uint64_t meinos_RegisterClassA(void* lpWndClass) {
    if (lpWndClass) {
        meinos_last_wndproc = *(uint64_t*)((uint8_t*)lpWndClass + 8);
    }
    return 1;
}

extern "C" WIN_ABI uint64_t meinos_RegisterClassExA(void* lpWndClassEx) {
    if (lpWndClassEx) {
        meinos_last_wndproc = *(uint64_t*)((uint8_t*)lpWndClassEx + 8);
    }
    return 1;
}

typedef uint64_t (WIN_ABI *WndProc_t)(uint64_t, uint32_t, uint64_t, uint64_t);

extern "C" WIN_ABI void* meinos_CreateWindowExA(uint32_t dwExStyle, const char* lpClassName, const char* lpWindowName, uint32_t dwStyle, int x, int y, int nWidth, int nHeight, void* hWndParent, void* hMenu, void* hInstance, void* lpParam) {
    if (dwStyle & 0x40000000) { // WS_CHILD
        uint64_t parent_slot = (uint64_t)hWndParent;
        if (parent_slot < 20) {
            int c_idx = -1;
            for (int i = 0; i < windows[parent_slot].num_controls; i++) {
                if (windows[parent_slot].controls[i].type == 0) {
                    c_idx = i;
                    break;
                }
            }
            if (c_idx == -1 && windows[parent_slot].num_controls < 32) {
                c_idx = windows[parent_slot].num_controls;
                windows[parent_slot].num_controls++;
            }
            if (c_idx != -1) {
                windows[parent_slot].controls[c_idx].type = 1; // Temporary so it's not 0

                // Custom simple str_iequals inline for safety
                int type = 0;
                if (lpClassName) {
                    const char* p = lpClassName;
                    if ((p[0]=='B'||p[0]=='b')&&(p[1]=='U'||p[1]=='u')) type = 1; // BUTTON
                    else if ((p[0]=='S'||p[0]=='s')&&(p[1]=='T'||p[1]=='t')) type = 2; // STATIC
                    else if ((p[0]=='m'||p[0]=='M')&&(p[1]=='s')&&(p[2]=='c')) type = 3; // msctls_progress32
                    else if ((p[0]=='C'||p[0]=='c')&&(p[1]=='O'||p[1]=='o')) type = 4; // COMBOBOX
                }
                windows[parent_slot].controls[c_idx].type = type;
                
                if (lpWindowName) {
                    int i=0;
                    while(lpWindowName[i] && i<63) { windows[parent_slot].controls[c_idx].text[i] = lpWindowName[i]; i++; }
                    windows[parent_slot].controls[c_idx].text[i] = 0;
                } else windows[parent_slot].controls[c_idx].text[0] = 0;
                
                windows[parent_slot].controls[c_idx].x = x;
                windows[parent_slot].controls[c_idx].y = y;
                windows[parent_slot].controls[c_idx].w = nWidth;
                windows[parent_slot].controls[c_idx].h = nHeight;
                windows[parent_slot].controls[c_idx].id = (int)(uint64_t)hMenu;
                windows[parent_slot].controls[c_idx].state = 0;
                windows[parent_slot].controls[c_idx].num_items = 0;
                windows[parent_slot].controls[c_idx].selected_item = 0;
                windows[parent_slot].controls[c_idx].is_dropped = 0;
                
                return (void*)(0x10000ULL | ((parent_slot & 0xFF) << 8) | (c_idx & 0xFF)); // Fake HWND
            }
        }
        return 0;
    }

    int slot = -1;
    for (int i = 16; i <= 19; i++) {
        if (!windows[i].open) { slot = i; break; }
    }
    if (slot == -1) return 0; // No window available

    windows[slot].open = true;
    windows[slot].minimized = false;
    windows[slot].x = (x == (int)0x80000000 || x < 0) ? 100 : x;
    windows[slot].y = (y == (int)0x80000000 || y < 0) ? 100 : y;
    windows[slot].w = (nWidth == (int)0x80000000 || nWidth <= 0) ? 400 : nWidth;
    windows[slot].h = (nHeight == (int)0x80000000 || nHeight <= 0) ? 300 : nHeight;
    if (windows[slot].w > 1024) windows[slot].w = 1024;
    if (windows[slot].h > 768) windows[slot].h = 768;
    windows[slot].color = 0x444444;
    windows[slot].cursor_pos = 0;
    
    for(int i = 0; i < 2048; i++) windows[slot].content[i] = 0;
    
    if (windows[slot].pixels != nullptr) {
        delete[] windows[slot].pixels;
    }
    windows[slot].pixel_w = windows[slot].w;
    windows[slot].pixel_h = windows[slot].h;
    windows[slot].pixels = new uint32_t[windows[slot].pixel_w * windows[slot].pixel_h];
    for (int i = 0; i < windows[slot].pixel_w * windows[slot].pixel_h; i++) {
        windows[slot].pixels[i] = 0xF0F0F0; // Windows dialog background gray
    }

    if (lpWindowName) {
        int i=0;
        while(lpWindowName[i] && i<15) { windows[slot].title[i] = lpWindowName[i]; i++; }
        windows[slot].title[i] = 0;
    } else {
        windows[slot].title[0] = 'W'; windows[slot].title[1] = 'I'; windows[slot].title[2] = 'N'; windows[slot].title[3] = 0;
    }
    windows[slot].num_controls = 0;
    windows[slot].wndproc = meinos_last_wndproc;
    windows[slot].needs_paint = _128; // true

    char msg[128];
    snprintf(msg, sizeof(msg), "[WIN32] CreateWindowExA -> HWND %d\n", slot);
    print_win(&windows[15], msg);

    // Call WM_CREATE
    if (windows[slot].wndproc) {
        // Provide a dummy CREATESTRUCT to avoid 0x0 dereferences
        uint8_t dummy_cs[128] = {0};
        *(void**)(&dummy_cs[0]) = lpParam; // lpCreateParams
        *(void**)(&dummy_cs[8]) = hInstance;
        *(void**)(&dummy_cs[16]) = hMenu;
        *(void**)(&dummy_cs[24]) = hWndParent;
        *(uint32_t*)(&dummy_cs[32]) = nHeight;
        *(uint32_t*)(&dummy_cs[36]) = nWidth;
        *(uint32_t*)(&dummy_cs[40]) = y;
        *(uint32_t*)(&dummy_cs[44]) = x;
        *(uint32_t*)(&dummy_cs[48]) = dwStyle;
        *(const char**)(&dummy_cs[56]) = lpWindowName;
        *(const char**)(&dummy_cs[64]) = lpClassName;
        *(uint32_t*)(&dummy_cs[72]) = dwExStyle;

        WndProc_t wndproc = (WndProc_t)windows[slot].wndproc;
        wndproc(slot, 0x0081 /* WM_NCCREATE */, 0, (uint64_t)&dummy_cs[0]);
        wndproc(slot, 0x0001 /* WM_CREATE */, 0, (uint64_t)&dummy_cs[0]);
    }

    return (void*)(uint64_t)slot;
}

extern "C" WIN_ABI int meinos_GetMessageA(void* lpMsg, void* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax) {
    sleep_ms(5);
    if (lpMsg) memset(lpMsg, 0, 48);
    // Only check Win32 app windows (slots 16-19), not MeinOS system windows
    for (int i = 16; i < 20; i++) {
        if (windows[i].open && windows[i].msg_msg != 0) {
            if (lpMsg) {
                *(uint64_t*)lpMsg = (uint64_t)i; // hwnd
                *(uint32_t*)((uint8_t*)lpMsg + 8) = windows[i].msg_msg; // message
                *(uint64_t*)((uint8_t*)lpMsg + 16) = windows[i].msg_wparam; // wParam
                *(uint64_t*)((uint8_t*)lpMsg + 24) = windows[i].msg_lparam; // lParam
            }
            windows[i].msg_msg = 0;
            return 1;
        }
        if (windows[i].open && windows[i].needs_paint) {
            windows[i].needs_paint = _86; // false
            if (lpMsg) {
                *(uint64_t*)lpMsg = (uint64_t)i; // hwnd
                *(uint32_t*)((uint8_t*)lpMsg + 8) = 0x000F; // WM_PAINT
            }
            return 1;
        }
    }
    return 1;
}

extern "C" WIN_ABI int meinos_GetMessageW(void* lpMsg, void* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax) {
    return meinos_GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

extern "C" WIN_ABI int meinos_PeekMessageA(void* lpMsg, void* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax, uint32_t wRemoveMsg) {
    if (lpMsg) memset(lpMsg, 0, 48);
    for (int i = 0; i < 20; i++) {
        if (windows[i].open && windows[i].msg_msg != 0) {
            if (lpMsg) {
                *(uint64_t*)lpMsg = (uint64_t)i; // hwnd
                *(uint32_t*)((uint8_t*)lpMsg + 8) = windows[i].msg_msg; // message
                *(uint64_t*)((uint8_t*)lpMsg + 16) = windows[i].msg_wparam; // wParam
                *(uint64_t*)((uint8_t*)lpMsg + 24) = windows[i].msg_lparam; // lParam
            }
            if (wRemoveMsg) windows[i].msg_msg = 0;
            return 1;
        }
        if (windows[i].open && windows[i].needs_paint) {
            if (wRemoveMsg) windows[i].needs_paint = _86;
            if (lpMsg) {
                *(uint64_t*)lpMsg = (uint64_t)i;
                *(uint32_t*)((uint8_t*)lpMsg + 8) = 0x000F; // WM_PAINT
            }
            return 1;
        }
    }
    return 0;
}

extern "C" WIN_ABI int meinos_PeekMessageW(void* lpMsg, void* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax, uint32_t wRemoveMsg) {
    return meinos_PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

extern "C" WIN_ABI int meinos_TranslateMessage(void* lpMsg) {
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_DispatchMessageA(void* lpMsg) {
    if (lpMsg) {
        uint64_t hwnd = *(uint64_t*)lpMsg;
        if (hwnd >= 0 && hwnd < 20 && windows[hwnd].wndproc) {
            uint32_t msg = *(uint32_t*)((uint8_t*)lpMsg + 8);
            uint64_t wParam = *(uint64_t*)((uint8_t*)lpMsg + 16);
            uint64_t lParam = *(uint64_t*)((uint8_t*)lpMsg + 24);
            
            // Skip WM_NULL - don't call WndProc for empty messages
            if (msg == 0) return 0;
            
            if (msg == 0x0111) { // WM_COMMAND
                char buf[128];
                snprintf(buf, 128, "[WIN32] DispatchMessage: WM_COMMAND (ID: %lu)\n", wParam & 0xFFFF);
                print_win(&windows[15], buf);
            }
            
            WndProc_t wndproc = (WndProc_t)windows[hwnd].wndproc;
            return wndproc(hwnd, msg, wParam, lParam);
        }
    }
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_DispatchMessageW(void* lpMsg) {
    return meinos_DispatchMessageA(lpMsg);
}

extern "C" WIN_ABI uint64_t meinos_DefWindowProcA(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_DefWindowProcW(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_SendMessageA(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        // Child control
        uint32_t parent_slot = (hw >> 8) & 0xFF;
        uint32_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            Control* c = &windows[parent_slot].controls[c_idx];
            if (c->type == 3 && Msg == 0x0402) { // PBM_SETPOS
                c->state = (int)wParam;
                return wParam;
            }
            if (c->type == 4) { // COMBOBOX
                if (Msg == 0x0143) { // CB_ADDSTRING
                    if (c->num_items < 10 && lParam) {
                        const char* str = (const char*)lParam;
                        int i = 0;
                        while(str[i] && i < 31) { c->items[c->num_items][i] = str[i]; i++; }
                        c->items[c->num_items][i] = 0;
                        c->num_items++;
                        return c->num_items - 1;
                    }
                    return -1; // CB_ERR
                }
                if (Msg == 0x014E) { // CB_SETCURSEL
                    if ((int)wParam >= 0 && (int)wParam < c->num_items) {
                        c->selected_item = (int)wParam;
                        return wParam;
                    }
                    return -1;
                }
                if (Msg == 0x0147) { // CB_GETCURSEL
                    return c->selected_item;
                }
            }
        }
        return 0;
    } else {
        return 0;
    }
}

extern "C" WIN_ABI uint64_t meinos_SendMessageW(void* hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    uint64_t hw = (uint64_t)hWnd;
    if ((hw & 0x10000ULL) && Msg == 0x0143) { // CB_ADDSTRING
        uint32_t parent_slot = (hw >> 8) & 0xFF;
        uint32_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            Control* c = &windows[parent_slot].controls[c_idx];
            if (c->type == 4 && c->num_items < 10 && lParam) { // COMBOBOX
                const uint16_t* wstr = (const uint16_t*)lParam;
                int i = 0;
                while(wstr[i] && i < 31) { c->items[c->num_items][i] = (char)wstr[i]; i++; }
                c->items[c->num_items][i] = 0;
                c->num_items++;
                return c->num_items - 1;
            }
        }
    }
    return meinos_SendMessageA(hWnd, Msg, wParam, lParam);
}

extern "C" WIN_ABI void meinos_PostQuitMessage(int nExitCode) {
}

extern "C" WIN_ABI int meinos_ShowWindow(void* hWnd, int nCmdShow) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20) {
        // valid window
        if (nCmdShow == 0) windows[slot].minimized = _128;
        else {
            windows[slot].minimized = _86;
            windows[slot].needs_paint = _128; // Trigger WM_PAINT when showing
        }
        return 1;
    } else if (slot >= 0x10000) {
        uint64_t parent_slot = (slot >> 8) & 0xFF;
        uint64_t c_idx = slot & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            // we could store visibility state, but for now we just return success
            return 1;
        }
    }
    return 0;
}

extern "C" WIN_ABI void* meinos_GetDlgItem(void* hDlg, int nIDDlgItem) {
    uint64_t slot = (uint64_t)hDlg;
    if (slot < 20) {
        for (int i = 0; i < windows[slot].num_controls; i++) {
            if (windows[slot].controls[i].id == nIDDlgItem && windows[slot].controls[i].type != 0) {
                return (void*)(0x10000ULL | ((slot & 0xFF) << 8) | (i & 0xFF));
            }
        }
    }
    // Return a plausible fake HWND so callers don't crash on NULL
    return (void*)(0x20000ULL | (uint64_t)(nIDDlgItem & 0xFFFF));
}

extern "C" WIN_ABI int meinos_CheckDlgButton(void* hDlg, int nIDButton, uint32_t uCheck) {
    uint64_t slot = (uint64_t)hDlg;
    if (slot < 20) {
        for (int i = 0; i < windows[slot].num_controls; i++) {
            if (windows[slot].controls[i].id == nIDButton) {
                windows[slot].controls[i].state = uCheck;
                return 1;
            }
        }
    }
    return 1;
}

extern "C" WIN_ABI uint32_t meinos_IsDlgButtonChecked(void* hDlg, int nIDButton) {
    uint64_t slot = (uint64_t)hDlg;
    if (slot < 20) {
        for (int i = 0; i < windows[slot].num_controls; i++) {
            if (windows[slot].controls[i].id == nIDButton) {
                return windows[slot].controls[i].state;
            }
        }
    }
    return 0;
}

extern "C" WIN_ABI int meinos_DestroyWindow(void* hWnd) {
    uint64_t hw = (uint64_t)hWnd;
    if (hw & 0x10000ULL) {
        // It's a control
        uint64_t parent_slot = (hw >> 8) & 0xFF;
        uint64_t c_idx = hw & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            windows[parent_slot].controls[c_idx].type = 0;
            return 1;
        }
    } else {
        uint64_t slot = hw;
        if (slot < 20) {
            windows[slot].open = false; windows[slot].num_controls = 0;
            if (windows[slot].pixels) {
                delete[] windows[slot].pixels;
                windows[slot].pixels = nullptr;
            }
            return 1;
        }
    }
    return 1;
}

extern "C" WIN_ABI void* meinos_BeginPaint(void* hWnd, void* lpPaint) {
    if (lpPaint) {
        *(uint64_t*)lpPaint = (uint64_t)hWnd; // hdc = hWnd
        uint64_t slot = (uint64_t)hWnd;
        if (slot >= 0 && slot < 20) {
            *(int*)((uint8_t*)lpPaint + 12) = 0; // left
            *(int*)((uint8_t*)lpPaint + 16) = 0; // top
            *(int*)((uint8_t*)lpPaint + 20) = windows[slot].pixel_w; // right
            *(int*)((uint8_t*)lpPaint + 24) = windows[slot].pixel_h; // bottom
        }
    }
    return hWnd;
}
extern "C" WIN_ABI int meinos_EndPaint(void* hWnd, void* lpPaint) { return 1; }
extern "C" WIN_ABI void* meinos_GetDC(void* hWnd) { return hWnd; }
extern "C" WIN_ABI int meinos_ReleaseDC(void* hWnd, void* hDC) { return 1; }

extern "C" WIN_ABI void* meinos_CreateSolidBrush(uint32_t color) {
    return (void*)(uint64_t)(color | 0x01000000); // Avoid returning NULL
}
extern "C" WIN_ABI void* meinos_CreatePen(int iStyle, int cWidth, uint32_t color) {
    return (void*)(uint64_t)(color | 0x02000000); // Avoid returning NULL
}
extern "C" WIN_ABI int meinos_DeleteObject(void* ho) { return 1; }
extern "C" WIN_ABI void* meinos_SelectObject(void* hdc, void* h) { return h; }
extern "C" WIN_ABI int meinos_MoveToEx(void* hdc, int x, int y, void* lppt) {
    if (lppt) { ((int*)lppt)[0] = 0; ((int*)lppt)[1] = 0; }
    return 1;
}
extern "C" WIN_ABI int meinos_LineTo(void* hdc, int x, int y) { return 1; }
extern "C" WIN_ABI void* meinos_CreateFontIndirectW(void* lplf) { return (void*)1; }
extern "C" WIN_ABI uint32_t meinos_GetSysColor(int nIndex) { return 0xFFFFFF; }
extern "C" WIN_ABI int meinos_RedrawWindow(void* hWnd, void* lprcUpdate, void* hrgnUpdate, uint32_t flags) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20) {
        windows[slot].needs_paint = _128;
    }
    return 1;
}

struct WIN32_TEXTMETRICW {
    int32_t tmHeight; int32_t tmAscent; int32_t tmDescent; int32_t tmInternalLeading;
    int32_t tmExternalLeading; int32_t tmAveCharWidth; int32_t tmMaxCharWidth;
    int32_t tmWeight; int32_t tmOverhang; int32_t tmDigitizedAspectX;
    int32_t tmDigitizedAspectY; uint16_t tmFirstChar; uint16_t tmLastChar;
    uint16_t tmDefaultChar; uint16_t tmBreakChar; uint8_t tmItalic;
    uint8_t tmUnderlined; uint8_t tmStruckOut; uint8_t tmPitchAndFamily;
    uint8_t tmCharSet;
};

extern "C" WIN_ABI int meinos_GetTextMetricsW(void* hdc, WIN32_TEXTMETRICW* lptm) {
    if (lptm) {
        for(int i=0; i<sizeof(WIN32_TEXTMETRICW); i++) ((uint8_t*)lptm)[i] = 0;
        lptm->tmHeight = 16; lptm->tmAscent = 12; lptm->tmDescent = 4;
        lptm->tmAveCharWidth = 8; lptm->tmMaxCharWidth = 16; lptm->tmWeight = 400;
        lptm->tmCharSet = 1;
    }
    return 1;
}

extern "C" WIN_ABI int meinos_GetTextExtentPoint32W(void* hdc, const uint16_t* lpString, int c, void* lpSize) {
    if (lpSize) { ((int*)lpSize)[0] = c * 8; ((int*)lpSize)[1] = 16; }
    return 1;
}

extern "C" WIN_ABI int meinos_GetTextExtentPoint32A(void* hdc, const char* lpString, int c, void* lpSize) {
    if (lpSize) { ((int*)lpSize)[0] = c * 8; ((int*)lpSize)[1] = 16; }
    return 1;
}

extern "C" WIN_ABI int meinos_GetTextFaceW(void* hdc, int c, uint16_t* lpName) {
    if (lpName && c > 5) {
        lpName[0] = 'A'; lpName[1] = 'r'; lpName[2] = 'i';
        lpName[3] = 'a'; lpName[4] = 'l'; lpName[5] = 0;
        return 6;
    }
    return 0;
}

extern "C" WIN_ABI int meinos_GdipCreateFromHDC(void* hdc, void** graphics) {
    if (graphics) *graphics = hdc;
    return 0; // Ok
}
extern "C" WIN_ABI int meinos_GdipDeleteGraphics(void* graphics) { return 0; }
extern "C" WIN_ABI int meinos_GdipCreateSolidFill(uint32_t color, void** brush) {
    if (brush) *brush = (void*)(uint64_t)((color & 0xFFFFFF) | 0x01000000);
    return 0;
}
extern "C" WIN_ABI int meinos_GdipDeleteBrush(void* brush) { return 0; }
extern "C" WIN_ABI int meinos_GdipFillRectangleI(void* graphics, void* brush, int x, int y, int width, int height) {
    uint64_t slot = (uint64_t)graphics;
    if (slot >= 0 && slot < 20 && windows[slot].pixels) {
        uint32_t color = (uint32_t)(uint64_t)brush & 0xFFFFFF;
        if (color == 0) color = 0x000001; // Avoid transparent black
        int x2 = x + width; int y2 = y + height;
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x2 > windows[slot].pixel_w) x2 = windows[slot].pixel_w;
        if (y2 > windows[slot].pixel_h) y2 = windows[slot].pixel_h;
        for (int cy = y; cy < y2; cy++) {
            for (int cx = x; cx < x2; cx++) {
                windows[slot].pixels[cy * windows[slot].pixel_w + cx] = color;
            }
        }
    }
    return 0;
}

struct WIN32_RECT { int left; int top; int right; int bottom; };

extern "C" WIN_ABI int meinos_FillRect(uint64_t hdc, const WIN32_RECT* lprc, uint64_t hbr) {
    uint64_t slot = hdc;
    if (slot >= 0 && slot < 20 && windows[slot].pixels) {
        if (!lprc) return 0;
        int x1 = lprc->left;
        int y1 = lprc->top;
        int x2 = lprc->right;
        int y2 = lprc->bottom;
        
        // Clamp to allocated buffer size
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > windows[slot].pixel_w) x2 = windows[slot].pixel_w;
        if (y2 > windows[slot].pixel_h) y2 = windows[slot].pixel_h;
        
        uint32_t color = (uint32_t)hbr & 0xFFFFFF;
        if (color == 0) color = 0x000001; // Avoid transparent black
        
        for (int y = y1; y < y2; y++) {
            for (int x = x1; x < x2; x++) {
                windows[slot].pixels[y * windows[slot].pixel_w + x] = color;
            }
        }
    }
    return 1;
}

extern "C" WIN_ABI int meinos_TextOutA(void* hDC, int x, int y, const char* lpString, int c) {
    uint64_t slot = (uint64_t)hDC;
    if (slot >= 0 && slot < 20 && lpString && windows[slot].pixels) {
        extern const uint8_t font_pixels[96][24][24]; // Declared elsewhere
        int cx = x;
        int cy = y;
        uint32_t text_color = 0xFFFFFF; // default white for now
        
        for(int i=0; i<c && lpString[i]; i++) {
            char ch = lpString[i];
            if (ch < 32 || ch > 126) ch = '?';
            
            for (int fy = 0; fy < 24; fy++) {
                for (int fx = 0; fx < 24; fx++) {
                    uint8_t alpha = font_pixels[ch - 32][fy][fx];
                    if (alpha > 0) {
                        int draw_x = cx + fx;
                        int draw_y = cy + fy;
                        if (draw_x >= 0 && draw_x < windows[slot].pixel_w && draw_y >= 0 && draw_y < windows[slot].pixel_h) {
                            uint32_t bg = windows[slot].pixels[draw_y * windows[slot].pixel_w + draw_x];
                            int br = (bg >> 16) & 0xFF;
                            int bg_g = (bg >> 8) & 0xFF;
                            int bb_b = bg & 0xFF;
                            int cr = (text_color >> 16) & 0xFF;
                            int cg = (text_color >> 8) & 0xFF;
                            int cb = text_color & 0xFF;
                            
                            int out_r = (cr * alpha + br * (255 - alpha)) / 255;
                            int out_g = (cg * alpha + bg_g * (255 - alpha)) / 255;
                            int out_b = (cb * alpha + bb_b * (255 - alpha)) / 255;
                            
                            windows[slot].pixels[draw_y * windows[slot].pixel_w + draw_x] = (out_r << 16) | (out_g << 8) | out_b;
                        }
                    }
                }
            }
            cx += 10; // approximate character width
        }
    }
    return 1;
}

extern "C" WIN_ABI int meinos_GetClientRect(void* hWnd, WIN32_RECT* lpRect) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20 && lpRect) {
        lpRect->left = 0;
        lpRect->top = 0;
        lpRect->right = windows[slot].pixel_w;
        lpRect->bottom = windows[slot].pixel_h;
        return 1;
    }
    return 0;
}

extern "C" WIN_ABI int meinos_MoveWindow(void* hWnd, int X, int Y, int nWidth, int nHeight, int bRepaint) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0x10000) {
        uint64_t parent_slot = (slot >> 8) & 0xFF;
        uint64_t c_idx = slot & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            windows[parent_slot].controls[c_idx].x = X;
            windows[parent_slot].controls[c_idx].y = Y;
            windows[parent_slot].controls[c_idx].w = nWidth;
            windows[parent_slot].controls[c_idx].h = nHeight;
            if (bRepaint) windows[parent_slot].needs_paint = _128;
            return 1;
        }
    } else if (slot < 20) {
        windows[slot].x = X;
        windows[slot].y = Y;
        windows[slot].w = nWidth;
        windows[slot].h = nHeight;
        if (bRepaint) windows[slot].needs_paint = _128;
        return 1;
    }
    return 0;
}

extern "C" WIN_ABI int meinos_SetWindowTextA(void* hWnd, const char* lpString) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20 && lpString) {
        int i = 0;
        while(lpString[i] && i < 15) { windows[slot].title[i] = lpString[i]; i++; }
        windows[slot].title[i] = 0;
        return 1;
    } else if (slot >= 0x10000 && lpString) {
        uint64_t parent_slot = (slot >> 8) & 0xFF;
        uint64_t c_idx = slot & 0xFF;
        if (parent_slot < 20 && c_idx < windows[parent_slot].num_controls) {
            int i = 0;
            while(lpString[i] && i < 63) { windows[parent_slot].controls[c_idx].text[i] = lpString[i]; i++; }
            windows[parent_slot].controls[c_idx].text[i] = 0;
            windows[parent_slot].needs_paint = _128;
            return 1;
        }
    }
    return 0;
}

extern "C" WIN_ABI int meinos_InvalidateRect(void* hWnd, const WIN32_RECT* lpRect, int bErase) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20) {
        windows[slot].needs_paint = _128; // true
    }
    return 1;
}

extern "C" WIN_ABI void meinos_ExitProcess(uint32_t uExitCode) {
    char msg[64];
    snprintf(msg, sizeof(msg), "\n[WIN32 ExitProcess] Code: %u\n", uExitCode);
    print_win(&windows[15], msg);
    siglongjmp(jump_env, 1);
}

extern "C" WIN_ABI int meinos_VirtualProtect(void* lpAddress, uint32_t dwSize, uint32_t flNewProtect, uint32_t* lpflOldProtect) {
    if (lpflOldProtect) *lpflOldProtect = 0x40; // PAGE_EXECUTE_READWRITE
    return 1; // Success
}

extern "C" WIN_ABI uint64_t meinos_VirtualQuery(void* lpAddress, void* lpBuffer, uint64_t dwLength) {
    if (!lpBuffer || dwLength < 48) return 0;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "[APP VIRTUALQUERY]: %p", lpAddress);
    print_win(&windows[15], msg);

    uint64_t addr = (uint64_t)lpAddress;
    addr &= ~0xFFFULL; // Round down to page boundary
    
    // We scan back a reasonable amount to find the 'MZ' header
    int max_pages = 2500; // ~10MB
    while (max_pages-- > 0) {
        // Warning: if we hit an unmapped page on Linux, this will segfault. 
        // But since the PE image is loaded as a single block by mmap, it should be safe.
        if (*(uint16_t*)addr == 0x5A4D) { // 'MZ'
            *(uint64_t*)((uint8_t*)lpBuffer + 0) = addr; // BaseAddress
            *(uint64_t*)((uint8_t*)lpBuffer + 8) = addr; // AllocationBase
            *(uint32_t*)((uint8_t*)lpBuffer + 16) = 0x40; // AllocationProtect (PAGE_EXECUTE_READWRITE)
            *(uint64_t*)((uint8_t*)lpBuffer + 24) = 0x1000000; // RegionSize
            *(uint32_t*)((uint8_t*)lpBuffer + 32) = 0x1000; // State (MEM_COMMIT)
            *(uint32_t*)((uint8_t*)lpBuffer + 36) = 0x40; // Protect (PAGE_EXECUTE_READWRITE)
            *(uint32_t*)((uint8_t*)lpBuffer + 40) = 0x20000; // Type (MEM_PRIVATE)
            return dwLength;
        }
        addr -= 0x1000;
    }
    return 0; // Not found
}

char last_proc_name[128] = {0};
extern "C" WIN_ABI void* meinos_LoadLibraryA(const char* lpLibFileName) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\n[WIN32 LoadLibraryA]: %s\n", lpLibFileName ? lpLibFileName : "NULL");
    print_win(&windows[15], msg);
    return (void*)0x1000; // Dummy handle
}

uint64_t resolve_windows_api_internal(const char* dll_name, const char* func_name, bool silent = false);

// Helper: wrap a 64-bit function address in a 32-bit wow64 thunk
uint32_t wrap_64bit_func_as_thunk(uint64_t func_addr, const char* func_name) {
    extern uint64_t wow64_api_table[4096];
    extern int wow64_api_table_count;
    extern uint32_t wow64_arg_bytes_table[16384];
    extern char* wow64_api_name_table[16384];
    int api_id = wow64_api_table_count++;
    wow64_api_table[api_id] = func_addr;
    wow64_arg_bytes_table[api_id] = get_api_arg_bytes(func_name);
    wow64_api_name_table[api_id] = strdup(func_name);
    return generate_wow64_thunk(api_id);
}

extern "C" WIN_ABI void* meinos_GetProcAddress(void* hModule, const char* lpProcName) {
    if (!lpProcName) return 0;
    if ((uint64_t)lpProcName < 0xFFFF) {
        // Ordinal-based lookup - return a dummy stub safely
        snprintf(last_proc_name, sizeof(last_proc_name), "Ordinal %lu", (uint64_t)lpProcName);
        char msg[128];
        snprintf(msg, sizeof(msg), "\n[WIN32 GetProcAddress]: %s\n", last_proc_name);
        print_win(&windows[15], msg);
        uint64_t stub = generate_dummy_stub(last_proc_name);
        if (is_pe_32bit) return (void*)(uint64_t)wrap_64bit_func_as_thunk(stub, last_proc_name);
        return (void*)stub;
    }
    snprintf(last_proc_name, sizeof(last_proc_name), "%s", lpProcName);
    char msg[128];
    snprintf(msg, sizeof(msg), "\n[WIN32 GetProcAddress]: %s\n", last_proc_name);
    print_win(&windows[15], msg);

    const char* dlls_to_check[] = {
        "kernel32.dll", "user32.dll", "gdi32.dll", "shlwapi.dll",
        "shell32.dll", "advapi32.dll", "comctl32.dll", "msvcrt.dll",
        "ole32.dll", "ws2_32.dll", "gdiplus.dll", "d3d9.dll", nullptr
    };
    for (int i = 0; dlls_to_check[i] != nullptr; i++) {
        uint64_t addr = resolve_windows_api_internal(dlls_to_check[i], lpProcName, true);
        if (addr != 0 && addr != (uint64_t)dummy_stub_array[999]) {
            if (is_pe_32bit) return (void*)(uint64_t)wrap_64bit_func_as_thunk(addr, lpProcName);
            return (void*)addr;
        }
    }

    uint64_t stub = generate_dummy_stub(last_proc_name);
    if (is_pe_32bit) return (void*)(uint64_t)wrap_64bit_func_as_thunk(stub, last_proc_name);
    return (void*)stub;
}

extern "C" {
WIN_ABI void* meinos_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }
WIN_ABI void* meinos_memset(void* s, int c, size_t n) { return memset(s, c, n); }
WIN_ABI void* meinos_memmove(void* dest, const void* src, size_t n) { return memmove(dest, src, n); }
WIN_ABI int meinos_memcmp(const void* s1, const void* s2, size_t n) { return memcmp(s1, s2, n); }

WIN_ABI void* meinos_malloc(size_t size) { 
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) return meinos_alloc32(size);
    return malloc(size); 
}
WIN_ABI void meinos_free(void* ptr) { 
    extern bool is_pe_32bit;
    if (is_pe_32bit) return;
    free(ptr); 
}
WIN_ABI void* meinos_calloc(size_t nmemb, size_t size) { 
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        void* ptr = meinos_alloc32(nmemb * size);
        if (ptr) memset(ptr, 0, nmemb * size);
        return ptr;
    }
    return calloc(nmemb, size); 
}
WIN_ABI void* meinos_realloc(void* ptr, size_t size) { 
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) {
        void* new_ptr = meinos_alloc32(size);
        if (new_ptr && ptr) {
            size_t* metadata = (size_t*)((uint8_t*)ptr - 16);
            size_t old_size = *metadata;
            size_t copy_size = old_size < size ? old_size : size;
            memcpy(new_ptr, ptr, copy_size);
        } else if (new_ptr && !ptr) {
            memset(new_ptr, 0, size);
        }
        return new_ptr;
    }
    return realloc(ptr, size); 
}

WIN_ABI size_t meinos_strlen(const char* s) { return strlen(s); }
WIN_ABI char* meinos_strchr(const char* s, int c) { return (char*)strchr(s, c); }
WIN_ABI int meinos_strcmp(const char* s1, const char* s2) { return strcmp(s1, s2); }
WIN_ABI int meinos_strncmp(const char* s1, const char* s2, size_t n) { return strncmp(s1, s2, n); }

WIN_ABI void meinos_abort() {
    printf("\n[CRT] ABORT CALLED BY APPLICATION!\n");
    while(1) { sleep_ms(100); }
}

WIN_ABI void meinos_exit(int status) {
    printf("\n[CRT] EXIT CALLED: %d\n", status);
    while(1) { sleep_ms(100); }
}

struct {
    uintptr_t handle;
    bool active;
} thread_pool[128] = {0};

struct meinos_thread_args {
    void* (*start_address)(void*) __attribute__((ms_abi));
    void* arglist;
    int slot;
};

void* meinos_thread_runner(void* arg) {
    extern __thread bool in_exe_thread;
    in_exe_thread = true;
    extern void segfault_handler(int, siginfo_t*, void*);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segfault_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
    stack_t altstack;
    altstack.ss_sp = malloc(SIGSTKSZ * 4);
    altstack.ss_size = SIGSTKSZ * 4;
    altstack.ss_flags = 0;
    sigaltstack(&altstack, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);

    meinos_thread_args* args = (meinos_thread_args*)arg;
    int slot = args->slot;
    void* (*start)(void*) __attribute__((ms_abi)) = args->start_address;
    void* a = args->arglist;
    free(args);

    if (sigsetjmp(jump_env, 1) == 0) {
        start(a);
    } else {
        printf("[SYS] BACKGROUND THREAD TERMINATED SAFELY.\n");
    }
    
    if (slot >= 0 && slot < 128) {
        thread_pool[slot].active = false;
    }
    
    in_exe_thread = false;
    return 0;
}

extern "C" WIN_ABI uintptr_t meinos_beginthreadex(void *security, unsigned stack_size, unsigned (*start_address)(void *), void *arglist, unsigned initflag, unsigned *thrdaddr) {
    pthread_t t;
    int slot = -1;
    for(int i=0; i<128; i++) {
        if (!thread_pool[i].active) {
            slot = i;
            thread_pool[i].active = true;
            break;
        }
    }
    if (slot == -1) return 0;

    meinos_thread_args* args = (meinos_thread_args*)malloc(sizeof(meinos_thread_args));
    args->start_address = (void*(*)(void*))start_address;
    args->arglist = arglist;
    args->slot = slot;
    if (pthread_create(&t, NULL, meinos_thread_runner, args) == 0) {
        pthread_detach(t);
        thread_pool[slot].handle = (uintptr_t)t;
        return (uintptr_t)t;
    }
    thread_pool[slot].active = false;
    free(args);
    return 0;
}

extern "C" WIN_ABI uintptr_t meinos_CreateThread(void* lpThreadAttributes, size_t dwStackSize, void* lpStartAddress, void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId) {
    return meinos_beginthreadex(NULL, 0, (unsigned(*)(void*))lpStartAddress, lpParameter, 0, NULL);
}

struct OPENFILENAMEA_x64 {
    uint32_t lStructSize;
    uint32_t padding;
    uint64_t hwndOwner;
    uint64_t hInstance;
    uint64_t lpstrFilter;
    uint64_t lpstrCustomFilter;
    uint32_t nMaxCustFilter;
    uint32_t nFilterIndex;
    char*    lpstrFile;
    uint32_t nMaxFile;
    uint32_t padding2;
};

extern "C" WIN_ABI int meinos_GetOpenFileNameA(void* lpofn) {
    if (!lpofn) return 0;
    dsk_mgr_picker_mode = true;
    dsk_mgr_picked_file[0] = 0;
    windows[4].open = true;
    windows[4].minimized = false;
    focus_window(4);
    
    while (windows[4].open && dsk_mgr_picker_mode) {
        sleep_ms(10);
    }
    dsk_mgr_picker_mode = false;
    
    if (dsk_mgr_picked_file[0] != 0) {
        char* lpstrFile = *(char**)((uint8_t*)lpofn + 48);
        if (lpstrFile) {
            strncpy(lpstrFile, dsk_mgr_picked_file, 255);
            return 1;
        }
    }
    return 0; // Simulate cancel
}

extern "C" WIN_ABI int meinos_GetSaveFileNameA(void* lpofn) {
    if (!lpofn) return 0;
    char* lpstrFile = *(char**)((uint8_t*)lpofn + 48);
    if (lpstrFile) {
        strncpy(lpstrFile, "Z:\new_image.bin", 255);
        return 1;
    }
    return 0;
}

extern "C" WIN_ABI int meinos_GetOpenFileNameW(void* lpofn) {
    if (!lpofn) return 0;
    dsk_mgr_picker_mode = true;
    dsk_mgr_picked_file[0] = 0;
    windows[4].open = true;
    windows[4].minimized = false;
    focus_window(4);
    
    while (windows[4].open && dsk_mgr_picker_mode) {
        sleep_ms(10);
    }
    dsk_mgr_picker_mode = false;
    
    if (dsk_mgr_picked_file[0] != 0) {
        uint16_t* lpstrFile = *(uint16_t**)((uint8_t*)lpofn + 48);
        if (lpstrFile) {
            int i = 0;
            while (dsk_mgr_picked_file[i] && i < 255) {
                lpstrFile[i] = (uint16_t)dsk_mgr_picked_file[i];
                i++;
            }
            lpstrFile[i] = 0;
            return 1;
        }
    }
    return 0; // Simulate cancel
}

extern "C" WIN_ABI int meinos_GetSaveFileNameW(void* lpofn) {
    return 0; // Simulate cancel
}


WIN_ABI void* meinos_acrt_iob_func(unsigned idx) {
    // Return dummy valid pointers that are non-zero
    if (idx == 0) return (void*)1;
    if (idx == 1) return (void*)2;
    if (idx == 2) return (void*)3;
    return (void*)2;
}

WIN_ABI int meinos_stdio_common_vfprintf(uint64_t options, void* stream, const char* format, void* locale, void* arglist) {
    char msg[256];
    snprintf(msg, sizeof(msg), "[APP PRINTF]: %s\n", format ? format : "NULL");
    print_win(&windows[15], msg);
    return 0;
}

int g_meinos_fmode = 0;
int g_meinos_commode = 0;
WIN_ABI int* meinos_p__fmode() { return &g_meinos_fmode; }
WIN_ABI int* meinos_p__commode() { return &g_meinos_commode; }

WIN_ABI void* meinos_set_invalid_parameter_handler(void* handler) { return 0; }
WIN_ABI void meinos_set_app_type(int type) {}
WIN_ABI int meinos_configthreadlocale(int type) { return 0; }
WIN_ABI void meinos_setusermatherr(void* handler) {}
WIN_ABI int meinos_configure_narrow_argv(int mode) { return 0; }
WIN_ABI int meinos_initialize_narrow_environment() { return 0; }
extern "C" WIN_ABI uint64_t meinos_CreateSemaphoreA(void* lpSemaphoreAttributes, int32_t lInitialCount, int32_t lMaximumCount, const char* lpName) { return 10; }
extern "C" WIN_ABI bool meinos_ReleaseSemaphore(uint64_t hSemaphore, int32_t lReleaseCount, int32_t* lpPreviousCount) { if (lpPreviousCount) *lpPreviousCount = 0; return true; }
extern "C" WIN_ABI bool meinos_SetThreadPriority(uint64_t hThread, int nPriority) { return true; }
extern "C" WIN_ABI int meinos_GetThreadPriority(uint64_t hThread) { return 0; }
extern "C" WIN_ABI uint32_t meinos_ResumeThread(uint64_t hThread) { return 1; }
extern "C" WIN_ABI bool meinos_GetHandleInformation(uint64_t hObject, uint32_t* lpdwFlags) { if (lpdwFlags) *lpdwFlags = 0; return true; }

#define GWLP_USERDATA -21
#define GWLP_WNDPROC -4

extern "C" WIN_ABI uint64_t meinos_SetWindowLongPtrA(uint64_t hWnd, int nIndex, uint64_t dwNewLong) {
    uint64_t w = hWnd;
    if (w < 20) {
        if (nIndex == GWLP_USERDATA || nIndex == -21) {
            uint64_t old = windows[w].user_data;
            windows[w].user_data = dwNewLong;
            return old;
        }
    } else {
        int p = (w >> 16) & 0xFFFF;
        int c = w & 0xFFFF;
        if (p < 20 && c < windows[p].num_controls) {
            if (nIndex == GWLP_USERDATA || nIndex == -21) {
                uint64_t old = windows[p].controls[c].user_data;
                windows[p].controls[c].user_data = dwNewLong;
                return old;
            }
            if (nIndex == GWLP_WNDPROC || nIndex == -4) {
                uint64_t old = windows[p].controls[c].wndproc;
                windows[p].controls[c].wndproc = dwNewLong;
                return old;
            }
        }
    }
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_GetWindowLongPtrA(uint64_t hWnd, int nIndex) {
    uint64_t w = hWnd;
    if (w < 20) {
        if (nIndex == GWLP_USERDATA || nIndex == -21) return windows[w].user_data;
    } else {
        int p = (w >> 16) & 0xFFFF;
        int c = w & 0xFFFF;
        if (p < 20 && c < windows[p].num_controls) {
            if (nIndex == GWLP_USERDATA || nIndex == -21) return windows[p].controls[c].user_data;
            if (nIndex == GWLP_WNDPROC || nIndex == -4) return windows[p].controls[c].wndproc;
        }
    }
    return 0;
}

extern "C" WIN_ABI uint64_t meinos_SetWindowLongA(uint64_t hWnd, int nIndex, uint32_t dwNewLong) {
    return meinos_SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
}

extern "C" WIN_ABI uint64_t meinos_GetWindowLongA(uint64_t hWnd, int nIndex) {
    return meinos_GetWindowLongPtrA(hWnd, nIndex);
}

extern "C" WIN_ABI uint64_t meinos_CallWindowProcA(void* lpPrevWndFunc, uint64_t hWnd, uint32_t Msg, uint64_t wParam, uint64_t lParam) {
    if (!lpPrevWndFunc || lpPrevWndFunc == (void*)1) return 0;
    uint64_t (WIN_ABI *wndproc)(uint64_t, uint32_t, uint64_t, uint64_t) = (uint64_t (WIN_ABI *)(uint64_t, uint32_t, uint64_t, uint64_t))lpPrevWndFunc;
    return wndproc(hWnd, Msg, wParam, lParam);
}

} // end extern "C"
uint64_t meinos_main_exe_base = 0;
extern "C" {

extern "C" WIN_ABI void* meinos_FindResourceA(void* hModule, const char* lpName, const char* lpType) {
    printf("[SYS] FindResourceA: Type: %lX, Name: %lX\n", (uint64_t)lpType, (uint64_t)lpName);
    if (!hModule) hModule = (void*)meinos_main_exe_base;
    if (!hModule) return 0;
    
    uint8_t* map = (uint8_t*)hModule;
    uint32_t pe_offset = *(uint32_t*)(map + 0x3C);
    uint16_t magic = *(uint16_t*)(map + pe_offset + 24);
    
    uint32_t res_rva = 0;
    if (magic == 0x010B) {
        res_rva = *(uint32_t*)(map + pe_offset + 120 + 2*8);
    } else {
        res_rva = *(uint32_t*)(map + pe_offset + 136 + 2*8);
    }
    if (res_rva == 0) return 0;
    
    uint8_t* root = map + res_rva;
    uint32_t* dir = (uint32_t*)root;
    uint16_t named_entries = *(uint16_t*)(root + 12);
    uint16_t id_entries = *(uint16_t*)(root + 14);
    
    uint32_t target_type = (uint64_t)lpType < 0xFFFF ? (uint64_t)lpType : 0xFFFFFFFF;
    uint32_t target_name = (uint64_t)lpName < 0xFFFF ? (uint64_t)lpName : 0xFFFFFFFF;
    
    uint32_t* entries = (uint32_t*)(root + 16);
    for(int i=0; i<named_entries + id_entries; i++) {
        uint32_t name = entries[i*2];
        uint32_t offset = entries[i*2+1];
        if ((offset >> 31) && (name == target_type || target_type == 0xFFFFFFFF)) { // Type match
            uint8_t* subdir = root + (offset & 0x7FFFFFFF);
            uint16_t sub_named = *(uint16_t*)(subdir + 12);
            uint16_t sub_id = *(uint16_t*)(subdir + 14);
            uint32_t* subentries = (uint32_t*)(subdir + 16);
            for(int j=0; j<sub_named + sub_id; j++) {
                uint32_t subname = subentries[j*2];
                uint32_t suboffset = subentries[j*2+1];
                if ((suboffset >> 31) && (subname == target_name)) {
                    uint8_t* leafdir = root + (suboffset & 0x7FFFFFFF);
                    uint32_t* leafentries = (uint32_t*)(leafdir + 16);
                    uint32_t leafdata = leafentries[1];
                    uint32_t* data = (uint32_t*)(root + (leafdata & 0x7FFFFFFF));
                    printf("[SYS] FindResourceA MATCH: Found Type %X, Name %X, RVA %X, Size %d\n", (unsigned int)name, (unsigned int)subname, (unsigned int)data[0], (int)data[1]);
                    // Return pointer to IMAGE_RESOURCE_DATA_ENTRY
                    return data;
                }
            }
        }
    }
    return 0;
}
extern "C" WIN_ABI void* meinos_FindResourceW(void* hModule, const uint16_t* lpName, const uint16_t* lpType) {
    return meinos_FindResourceA(hModule, (const char*)lpName, (const char*)lpType);
}
extern "C" WIN_ABI void* meinos_FindResourceExW(void* hModule, const uint16_t* lpType, const uint16_t* lpName, uint16_t wLanguage) {
    return meinos_FindResourceA(hModule, (const char*)lpName, (const char*)lpType);
}
extern "C" WIN_ABI uint32_t meinos_SizeofResource(void* hModule, void* hResInfo) {
    if (!hResInfo) return 0;
    uint32_t* data = (uint32_t*)hResInfo;
    return data[1]; // Size field
}
extern "C" WIN_ABI void* meinos_LoadResource(void* hModule, void* hResInfo) {
    return hResInfo;
}
extern "C" WIN_ABI void* meinos_LockResource(void* hResData) {
    if (!hResData) return 0;
    uint32_t* data = (uint32_t*)hResData;
    if (!meinos_main_exe_base) return 0;
    return (void*)(meinos_main_exe_base + data[0]); // RVA to pointer
}

extern "C" WIN_ABI void* meinos_fopen(const char* filename, const char* mode) {
    if (!filename) return 0;
    char path[256];
    strncpy(path, filename, 255);
    path[255] = 0;
    // Strip Z:\ if present
    char* final_path = path;
    if (path[0] == 'Z' && path[1] == ':' && path[2] == '\\') {
        final_path = path + 3;
    }
    // Convert backward slashes to forward slashes
    for (int i=0; final_path[i]; i++) {
        if (final_path[i] == '\\') final_path[i] = '/';
    }
    return (void*)fopen(final_path, mode);
}

extern "C" WIN_ABI int meinos_system(const char* command) {
    return system(command);
}

extern "C" WIN_ABI void* meinos_SHBrowseForFolderA(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI void* meinos_SHBrowseForFolderW(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListA(void* pidl, char* pszPath) {
    return 0;
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListW(void* pidl, void* pszPath) {
    return 0;
}



extern "C" WIN_ABI void meinos_endthreadex(uint32_t retval) {
    siglongjmp(jump_env, 1);
}

extern "C" WIN_ABI int meinos_EnumChildWindows(uint64_t hWndParent, void* lpEnumFunc, uint64_t lParam) {
    return 1;
}

// Windows RTL_CRITICAL_SECTION is 24 bytes. pthread_mutex_t is 40 bytes on Linux x86_64.
// We CANNOT embed pthread_mutex_t inline or it overflows into adjacent memory!
// Instead, use indirection: store a pointer + magic cookie in the first 8 bytes of the
// 24-byte struct, and heap-allocate the actual pthread_mutex_t.
#define CS_MAGIC 0xCA5ECB01

struct MeinOSCSInternal {
    pthread_mutex_t mutex;
    uint32_t magic;
};

static MeinOSCSInternal* cs_get_internal(void* lpCriticalSection) {
    uint32_t* words = (uint32_t*)lpCriticalSection;
    if (words[1] == CS_MAGIC) {
        // Already initialized - word[0] is a pointer (low 32 bits, since alloc32 returns <4GB)
        return (MeinOSCSInternal*)(uintptr_t)words[0];
    }
    return nullptr;
}

extern "C" WIN_ABI void meinos_InitializeCriticalSection(void* lpCriticalSection) {
    if (!lpCriticalSection) return;
    uint32_t* words = (uint32_t*)lpCriticalSection;
    // Allocate the internal struct in 32-bit addressable memory
    extern void* meinos_alloc32(size_t size);
    MeinOSCSInternal* internal = (MeinOSCSInternal*)meinos_alloc32(sizeof(MeinOSCSInternal));
    if (!internal) {
        internal = (MeinOSCSInternal*)malloc(sizeof(MeinOSCSInternal));
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&internal->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    internal->magic = CS_MAGIC;
    // Store pointer in first DWORD, magic in second DWORD
    words[0] = (uint32_t)(uintptr_t)internal;
    words[1] = CS_MAGIC;
    // Zero the rest of the 24-byte struct to not corrupt anything
    words[2] = 0;
    words[3] = 0;
    words[4] = 0;
    words[5] = 0;
}

extern "C" WIN_ABI void meinos_EnterCriticalSection(void* lpCriticalSection) {
    if (!lpCriticalSection) return;
    MeinOSCSInternal* internal = cs_get_internal(lpCriticalSection);
    if (!internal) {
        meinos_InitializeCriticalSection(lpCriticalSection);
        internal = cs_get_internal(lpCriticalSection);
    }
    if (internal) pthread_mutex_lock(&internal->mutex);
}

extern "C" WIN_ABI void meinos_LeaveCriticalSection(void* lpCriticalSection) {
    if (!lpCriticalSection) return;
    MeinOSCSInternal* internal = cs_get_internal(lpCriticalSection);
    if (internal) pthread_mutex_unlock(&internal->mutex);
}

extern "C" WIN_ABI void meinos_DeleteCriticalSection(void* lpCriticalSection) {
    if (!lpCriticalSection) return;
    MeinOSCSInternal* internal = cs_get_internal(lpCriticalSection);
    if (internal) {
        pthread_mutex_destroy(&internal->mutex);
        internal->magic = 0;
    }
    uint32_t* words = (uint32_t*)lpCriticalSection;
    words[0] = 0;
    words[1] = 0;
}

extern "C" WIN_ABI uint64_t meinos_fread(void* ptr, uint64_t size, uint64_t count, void* stream) {
    return fread(ptr, size, count, (FILE*)stream);
}

extern "C" WIN_ABI int meinos_fclose(void* stream) {
    return fclose((FILE*)stream);
}

extern "C" WIN_ABI int meinos_fseek(void* stream, long offset, int origin) {
    return fseek((FILE*)stream, offset, origin);
}

extern "C" WIN_ABI long meinos_ftell(void* stream) {
    return ftell((FILE*)stream);
}

extern "C" WIN_ABI int meinos_AdjustTokenPrivileges(void* TokenHandle, int DisableAllPrivileges, void* NewState, uint32_t BufferLength, void* PreviousState, uint32_t* ReturnLength) {
    return 1;
}

extern "C" WIN_ABI void meinos_Sleep(uint32_t dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}


extern "C" WIN_ABI int meinos_OpenProcessToken(void* ProcessHandle, uint32_t DesiredAccess, void** TokenHandle) {
    if (TokenHandle) *TokenHandle = (void*)1;
    return 1;
}

extern "C" WIN_ABI int meinos_LookupPrivilegeValueA(const char* lpSystemName, const char* lpName, void* lpLuid) {
    return 1;
}


extern "C" WIN_ABI void* meinos_LoadLibraryExA(const char* lpLibFileName, void* hFile, uint32_t dwFlags) {
    return meinos_LoadLibraryA(lpLibFileName);
}

extern "C" WIN_ABI void* meinos_LoadLibraryW(const uint16_t* lpLibFileName) {
    char buf[256];
    int i=0;
    if (lpLibFileName) {
        while(lpLibFileName[i] && i<255) { buf[i] = (char)lpLibFileName[i]; i++; }
    }
    buf[i] = 0;
    char msg[256];
    snprintf(msg, sizeof(msg), "\n[WIN32 LoadLibraryW]: %s\n", buf);
    print_win(&windows[15], msg);
    return (void*)0x1000;
}

extern "C" WIN_ABI void* meinos_LoadLibraryExW(const uint16_t* lpLibFileName, void* hFile, uint32_t dwFlags) {
    char buf[256];
    int i=0;
    if (lpLibFileName) {
        while(lpLibFileName[i] && i<255) { buf[i] = (char)lpLibFileName[i]; i++; }
    }
    buf[i] = 0;
    char msg[256];
    snprintf(msg, sizeof(msg), "\n[WIN32 LoadLibraryExW]: %s (Flags: %u)\n", buf, dwFlags);
    print_win(&windows[15], msg);
    return (void*)0x1000;
}

extern "C" WIN_ABI int meinos_mb_cur_max_func() {
    return 1;
}

extern "C" WIN_ABI uint32_t meinos_lc_codepage_func() {
    return 1252; // Windows-1252 codepage
}


extern "C" WIN_ABI void meinos_ExitThread(uint32_t dwExitCode) {
    siglongjmp(jump_env, 1);
}

extern "C" WIN_ABI int* meinos_errno() {
    static __thread int thread_local_errno = 0;
    return &thread_local_errno;
}

extern "C" WIN_ABI int meinos_WideCharToMultiByte(uint32_t CodePage, uint32_t dwFlags, const uint16_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar) {
    if (!lpWideCharStr) return 0;
    if (cchWideChar < 0) {
        cchWideChar = 0;
        const uint16_t* p = lpWideCharStr;
        while(*p++) cchWideChar++;
        cchWideChar++; // include null terminator
    }
    
    // Debug print
    printf("[WC2MB] Converting cch=%d, cb=%d. Str hex: %04x %04x %04x %04x %04x %04x %04x %04x\n", 
             cchWideChar, cbMultiByte, 
             lpWideCharStr[0], lpWideCharStr[1], 
             lpWideCharStr[2], lpWideCharStr[3],
             lpWideCharStr[4], lpWideCharStr[5],
             lpWideCharStr[6], lpWideCharStr[7]);

    if (lpUsedDefaultChar) *lpUsedDefaultChar = 0; // FALSE

    if (cbMultiByte == 0) return cchWideChar;
    int len = cchWideChar < cbMultiByte ? cchWideChar : cbMultiByte;
    if (lpMultiByteStr && lpWideCharStr) {
        for(int i=0; i<len; i++) lpMultiByteStr[i] = (char)lpWideCharStr[i];
    }
    return len;
}

extern "C" WIN_ABI int meinos_MultiByteToWideChar(uint32_t CodePage, uint32_t dwFlags, const char* lpMultiByteStr, int cbMultiByte, uint16_t* lpWideCharStr, int cchWideChar) {
    if (!lpMultiByteStr) return 0;
    if (cbMultiByte < 0) {
        cbMultiByte = 0;
        const char* p = lpMultiByteStr;
        while(*p++) cbMultiByte++;
        cbMultiByte++; // include null terminator
    }
    if (cchWideChar == 0) return cbMultiByte;
    int len = cbMultiByte < cchWideChar ? cbMultiByte : cchWideChar;
    if (lpWideCharStr && lpMultiByteStr) {
        for(int i=0; i<len; i++) lpWideCharStr[i] = (uint16_t)lpMultiByteStr[i];
    }
    return len;
}

extern "C" WIN_ABI char* meinos_CharPrevA(const char* lpszStart, const char* lpszCurrent) {
    if (lpszCurrent > lpszStart) return (char*)(lpszCurrent - 1);
    return (char*)lpszStart;
}

extern "C" WIN_ABI int meinos_wgetmainargs(int* _Argc, uint16_t*** _Argv, uint16_t*** _Env, int _DoWildCard, void* _StartInfo) {
    if (_Argc) *_Argc = 1;
    if (_Argv) {
        static uint16_t* argv[2] = {0, 0};
        static uint16_t arg0[] = {'C',':','\\','p','r','o','g','r','a','m','.','e','x','e',0};
        argv[0] = arg0;
        *_Argv = argv;
    }
    if (_Env) {
        static uint16_t* env[1] = {0};
        *_Env = env;
    }
    return 0; // Returns 0 on success in msvcrt
}



extern "C" WIN_ABI void* meinos_CoTaskMemAlloc(uint64_t cb) {
    return malloc(cb);
}
extern "C" WIN_ABI void meinos_CoTaskMemFree(void* pv) {
    if (pv) free(pv);
}
extern "C" WIN_ABI int meinos_EnableWindow(void* hWnd, int bEnable) {
    return 1;
}
extern "C" WIN_ABI void* meinos_CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, uint32_t bItalic, uint32_t bUnderline, uint32_t bStrikeOut, uint32_t iCharSet, uint32_t iOutPrecision, uint32_t iClipPrecision, uint32_t iQuality, uint32_t iPitchAndFamily, const char* pszFaceName) {
    return (void*)1;
}

extern "C" WIN_ABI void* meinos_GetModuleHandleA(const char* lpModuleName) {
    if (!lpModuleName) return (void*)meinos_main_exe_base;
    return (void*)0;
}
extern "C" WIN_ABI void* meinos_GetModuleHandleW(const uint16_t* lpModuleName) {
    if (!lpModuleName) return (void*)meinos_main_exe_base;
    return (void*)0;
}

extern "C" WIN_ABI int meinos_GetModuleHandleExW(uint32_t dwFlags, const uint16_t* lpModuleName, uint32_t* phModule) {
    if (phModule) {
        *phModule = (uint32_t)meinos_main_exe_base;
    }
    return 1;
}

extern "C" WIN_ABI uint32_t meinos_GetCurrentPackageId(uint32_t* bufferLength, uint8_t* buffer) {
    return 15700; // APPMODEL_ERROR_NO_PACKAGE
}

extern "C" WIN_ABI int meinos_HeapSetInformation(void* HeapHandle, int HeapInformationClass, void* HeapInformation, uint64_t HeapInformationLength) {
    return 1; // TRUE
}

extern "C" WIN_ABI int meinos_GetStringTypeW(uint32_t dwInfoType, const uint16_t* lpSrcStr, int cchSrc, uint16_t* lpCharType) {
    if (lpCharType) {
        int len = cchSrc;
        if (len < 0) {
            len = 0;
            while(lpSrcStr[len] != 0) len++;
        }
        for (int i = 0; i < len; i++) {
            lpCharType[i] = 0x01; // C1_UPPER or whatever, just dummy valid type
        }
    }
    return 1; // TRUE
}

extern "C" WIN_ABI int meinos_FreeLibrary(void* hLibModule) {
    return 1; // TRUE
}

extern "C" WIN_ABI void* meinos_GlobalAlloc(uint32_t uFlags, uint64_t dwBytes);
extern "C" WIN_ABI void* meinos_CommandLineToArgvW(const uint16_t* lpCmdLine, int* pNumArgs) {
    if (pNumArgs) *pNumArgs = 1;
    extern bool is_pe_32bit;
    if (is_pe_32bit) {
        uint32_t* arr = (uint32_t*)meinos_GlobalAlloc(0, 2 * sizeof(uint32_t));
        if (arr) {
            extern const uint16_t* meinos_GetCommandLineW();
            arr[0] = (uint32_t)(uint64_t)meinos_GetCommandLineW();
            arr[1] = 0;
        }
        return arr;
    }
    void** arr = (void**)meinos_GlobalAlloc(0, 2 * sizeof(void*));
    if (arr) {
        extern const uint16_t* meinos_GetCommandLineW();
        arr[0] = (void*)meinos_GetCommandLineW();
        arr[1] = 0;
    }
    return arr;
}

extern "C" WIN_ABI void meinos_CorExitProcess(uint32_t exitCode) {
    void meinos_ExitProcess(uint32_t uExitCode);
    meinos_ExitProcess(exitCode);
}

extern "C" WIN_ABI void* meinos_GlobalAlloc(uint32_t uFlags, uint64_t dwBytes) {
    extern bool is_pe_32bit;
    extern void* meinos_alloc32(size_t size);
    if (is_pe_32bit) return meinos_alloc32(dwBytes);
    return malloc(dwBytes);
}
extern "C" WIN_ABI void* meinos_GlobalFree(void* hMem) {
    if (hMem) free(hMem);
    return 0;
}
extern "C" WIN_ABI void* meinos_GlobalLock(void* hMem) {
    return hMem;
}
extern "C" WIN_ABI int meinos_GlobalUnlock(void* hMem) {
    return 1;
}
extern "C" WIN_ABI int meinos_CoCreateInstance(void* rclsid, void* pUnkOuter, uint32_t dwClsContext, void* riid, void** ppv) {
    if (ppv) *ppv = 0;
    return 0x80004002; // E_NOINTERFACE
}
extern "C" WIN_ABI int meinos_CoInitializeEx(void* pvReserved, uint32_t dwCoInit) {
    return 0; // S_OK
}
extern "C" WIN_ABI int meinos_CoInitializeSecurity(void* pSecDesc, int cAuthSvc, void* asAuthSvc, void* pReserved1, uint32_t dwAuthnLevel, uint32_t dwImpLevel, void* pAuthList, uint32_t dwCapabilities, void* pReserved3) {
    return 0; // S_OK
}


extern "C" WIN_ABI void* meinos_GdipAlloc(uint64_t size) {
    return malloc(size);
}
extern "C" WIN_ABI void meinos_GdipFree(void* ptr) {
    if (ptr) free(ptr);
}
extern "C" WIN_ABI int DUMMY_meinos_CreateStreamOnHGlobal(void* hGlobal, int fDeleteOnRelease, void** ppstm) {
    if (ppstm) *ppstm = 0;
    return 0x80004002; // E_NOINTERFACE for now, prevents crash but might fail gracefully
}
extern "C" WIN_ABI int meinos_OutputDebugStringA(const char* lpOutputString) {
    printf("[WIN32 OutputDebugStringA]: %s\n", lpOutputString);
    return 0;
}
extern "C" WIN_ABI int meinos_OutputDebugStringW(const uint16_t* lpOutputString) {
    if (lpOutputString) {
        char buf[256];
        int i=0;
        while(lpOutputString[i] && i<255) { buf[i] = (char)lpOutputString[i]; i++; }
        buf[i] = 0;
        printf("[WIN32 OutputDebugStringW]: %s\n", buf);
    } else {
        printf("[WIN32 OutputDebugStringW]: NULL\n");
    }
    return 0;
}
extern "C" WIN_ABI void meinos_CoUninitialize() {
}



extern "C" WIN_ABI int my_setjmp(void* jmpbuf);
extern "C" WIN_ABI void my_longjmp(void* jmpbuf, int val);
struct FakeIStream {
    void* lpVtbl;
    void* hGlobal;
    int ref_count;
};

extern "C" WIN_ABI uint32_t FakeIStream_Release(FakeIStream* stream) {
    stream->ref_count--;
    if (stream->ref_count == 0) {
        free(stream->lpVtbl);
        free(stream);
        return 0;
    }
    return stream->ref_count;
}

extern "C" WIN_ABI int meinos_CreateStreamOnHGlobal(void* hGlobal, int fDeleteOnRelease, void** ppstm) {
    FakeIStream* stream = (FakeIStream*)malloc(sizeof(FakeIStream));
    stream->hGlobal = hGlobal;
    stream->ref_count = 1;
    
    void** vtbl = (void**)malloc(sizeof(void*) * 14);
    for(int i=0; i<14; i++) vtbl[i] = (void*)0; // NULL by default
    vtbl[2] = (void*)FakeIStream_Release; // Release is index 2 in IStreamVtbl
    
    stream->lpVtbl = vtbl;
    if (ppstm) *ppstm = stream;
    return 0; // S_OK
}

struct MeinosGpImage {
    int w, h;
    uint32_t* pixels;
};

extern "C" WIN_ABI int meinos_GdipCreateBitmapFromStream(FakeIStream* stream, void** bitmap) {
    if (!stream || !bitmap) return 2; // InvalidParameter
    
    size_t size = malloc_usable_size(stream->hGlobal);
    if (size == 0) size = 10000000; // fallback
    
    int w, h, channels;
    uint8_t* data = stbi_load_from_memory((const uint8_t*)stream->hGlobal, size, &w, &h, &channels, 4);
    
    if (!data) {
        printf("[STBI] ERROR: Failed to load image from memory.\n");
        return 3; // OutOfMemory
    }
    
    MeinosGpImage* img = (MeinosGpImage*)malloc(sizeof(MeinosGpImage));
    img->w = w;
    img->h = h;
    img->pixels = (uint32_t*)malloc(w * h * 4);
    
    // Copy and convert RGBA to ARGB/BGRA (stb outputs RGBA)
    // Windows GDI+ usually wants BGRA (0xAARRGGBB in little endian)
    for(int i=0; i<w*h; i++) {
        uint8_t r = data[i*4 + 0];
        uint8_t g = data[i*4 + 1];
        uint8_t b = data[i*4 + 2];
        uint8_t a = data[i*4 + 3];
        img->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    free(data);
    *bitmap = img;
    printf("[STBI] OK: Loaded %dx%d image.\n", w, h);
    return 0; // Ok
}

extern "C" WIN_ABI int meinos_GdipLoadImageFromStream(FakeIStream* stream, void** image) {
    return meinos_GdipCreateBitmapFromStream(stream, image);
}

extern "C" WIN_ABI int meinos_GdipGetImageWidth(MeinosGpImage* image, uint32_t* width) {
    if (image && width) { *width = image->w; return 0; }
    return 2;
}

extern "C" WIN_ABI int meinos_GdipGetImageHeight(MeinosGpImage* image, uint32_t* height) {
    if (image && height) { *height = image->h; return 0; }
    return 2;
}

extern "C" WIN_ABI int meinos_GdipDrawImageRectI(void* graphics, MeinosGpImage* image, int x, int y, int width, int height) {
    if (!graphics || !image) return 2;
    uint64_t slot = (uint64_t)graphics;
    if (slot >= 0 && slot < 20 && windows[slot].pixels) {
        int dst_w = windows[slot].pixel_w;
        int dst_h = windows[slot].pixel_h;
        for (int dy = 0; dy < height; dy++) {
            int sy = (dy * image->h) / height;
            int screen_y = y + dy;
            if (screen_y < 0 || screen_y >= dst_h) continue;
            for (int dx = 0; dx < width; dx++) {
                int sx = (dx * image->w) / width;
                int screen_x = x + dx;
                if (screen_x < 0 || screen_x >= dst_w) continue;
                
                uint32_t src_col = image->pixels[sy * image->w + sx];
                uint8_t a = (src_col >> 24) & 0xFF;
                if (a == 0) continue; // fully transparent
                
                if (a == 255) {
                    windows[slot].pixels[screen_y * dst_w + screen_x] = src_col & 0xFFFFFF;
                } else {
                    uint32_t dst_col = windows[slot].pixels[screen_y * dst_w + screen_x];
                    uint8_t r1 = (src_col >> 16) & 0xFF;
                    uint8_t g1 = (src_col >> 8) & 0xFF;
                    uint8_t b1 = src_col & 0xFF;
                    uint8_t r2 = (dst_col >> 16) & 0xFF;
                    uint8_t g2 = (dst_col >> 8) & 0xFF;
                    uint8_t b2 = dst_col & 0xFF;
                    uint8_t r = (r1 * a + r2 * (255 - a)) / 255;
                    uint8_t g = (g1 * a + g2 * (255 - a)) / 255;
                    uint8_t b = (b1 * a + b2 * (255 - a)) / 255;
                    windows[slot].pixels[screen_y * dst_w + screen_x] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
    return 0;
}

extern "C" WIN_ABI int meinos_GdipDisposeImage(MeinosGpImage* image) {
    if (image) {
        if (image->pixels) free(image->pixels);
        free(image);
    }
    return 0;
}

WIN_ABI void meinos_initterm(void (**pfbegin)(void), void (**pfend)(void)) {
    while (pfbegin < pfend) {
        if (*pfbegin != nullptr) (*pfbegin)();
        pfbegin++;
    }
}
WIN_ABI int meinos_initterm_e(int (**pfbegin)(void), int (**pfend)(void)) {
    while (pfbegin < pfend) {
        if (*pfbegin != nullptr) {
            int res = (*pfbegin)();
            if (res != 0) return res;
        }
        pfbegin++;
    }
    return 0;
}

WIN_ABI void* meinos_C_specific_handler() {
    return 0; // Structured exception handling dummy
}

int g_meinos_argc = 1;
char* g_meinos_argv[] = { (char*)"meinos_app.exe", nullptr };
char** g_meinos_argv_ptr = g_meinos_argv;
char* g_meinos_env[] = { nullptr };
char** g_meinos_env_ptr = g_meinos_env;
char* g_meinos_acmdln = (char*)"meinos_app.exe";

WIN_ABI int* meinos_p___argc() { return &g_meinos_argc; }
WIN_ABI char*** meinos_p___argv() { return &g_meinos_argv_ptr; }
WIN_ABI char*** meinos_p__environ() { return &g_meinos_env_ptr; }
WIN_ABI char*** meinos_p___wargv() { return &g_meinos_argv_ptr; } 
WIN_ABI char*** meinos_p__wenviron() { return &g_meinos_env_ptr; }
WIN_ABI char** meinos_p__acmdln() { return &g_meinos_acmdln; }
WIN_ABI char** meinos_p__wcmdln() { return &g_meinos_acmdln; }

WIN_ABI int meinos_set_new_mode(int mode) { return 0; }
WIN_ABI int meinos_crt_atexit(void* func) { return 0; }
WIN_ABI int meinos_register_onexit_function(void* table, void* func) { return 0; }
WIN_ABI int meinos_initialize_onexit_table(void* table) { return 0; }
WIN_ABI int meinos_execute_onexit_table(void* table) { return 0; }

WIN_ABI uint64_t meinos_time64(uint64_t* t) {
    // Return dummy time
    if (t) *t = 0;
    return 0;
}

WIN_ABI int meinos_ismbblead(unsigned int c) {
    return 0; // basic multibyte check dummy
}
WIN_ABI void* meinos_GetCurrentProcess() { return (void*)-1; }
WIN_ABI void* meinos_GetCurrentThread() { return (void*)-2; }
WIN_ABI int meinos_DuplicateHandle(void* hSourceProcess, void* hSource, void* hTargetProcess, void** lpTarget, uint32_t dwDesiredAccess, int bInherit, uint32_t dwOptions) {
    if (lpTarget) *lpTarget = hSource;
    return 1;
}
WIN_ABI int meinos_CloseHandle(void* hObject) {
    uint64_t handle = (uint64_t)hObject;
    extern FILE* meinos_file_handles[100];
    if (handle >= 1 && handle < 100 && meinos_file_handles[handle]) {
        fclose(meinos_file_handles[handle]);
        meinos_file_handles[handle] = NULL;
    }
    return 1;
}
WIN_ABI void* meinos_CreateEventA(void* lpEventAttributes, int bManualReset, int bInitialState, const char* lpName) {
    return (void*)1; // Dummy handle
}
WIN_ABI void* meinos_CreateEventW(void* lpEventAttributes, int bManualReset, int bInitialState, const char* lpName) {
    return (void*)1; // Dummy handle
}
WIN_ABI int meinos_SetEvent(void* hEvent) { return 1; }
WIN_ABI int meinos_ResetEvent(void* hEvent) { return 1; }
extern "C" WIN_ABI uint32_t meinos_WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds) {
    uintptr_t handle = (uintptr_t)hHandle;
    for (int i = 0; i < 128; i++) {
        if (thread_pool[i].active && thread_pool[i].handle == handle) {
            if (dwMilliseconds == 0) return 0x102; // WAIT_TIMEOUT
            int max_wait = dwMilliseconds;
            if (max_wait == 0xFFFFFFFF) max_wait = 5000;
            while (thread_pool[i].active && max_wait > 0) {
                sleep_ms(5);
                max_wait -= 5;
            }
            if (thread_pool[i].active) return 0x102; // WAIT_TIMEOUT
            return 0; // WAIT_OBJECT_0
        }
    }
    return 0; // WAIT_OBJECT_0
}
WIN_ABI uint32_t meinos_WaitForMultipleObjects(uint32_t nCount, void** lpHandles, int bWaitAll, uint32_t dwMilliseconds) {
    return meinos_WaitForSingleObject(lpHandles ? lpHandles[0] : 0, dwMilliseconds);
}
extern "C" WIN_ABI uint32_t meinos_MsgWaitForMultipleObjects(uint32_t nCount, void** pHandles, bool fWaitAll, uint32_t dwMilliseconds, uint32_t dwWakeMask) {
    uint32_t elapsed = 0;
    while (true) {
        // 1. Check for messages first
        for (int i = 16; i < 20; i++) {
            if (windows[i].open && (windows[i].msg_msg != 0 || windows[i].needs_paint)) {
                return nCount; // WAIT_OBJECT_0 + nCount (indicates message arrived)
            }
        }
        
        // 2. Check handles
        if (nCount > 0 && pHandles) {
            for (uint32_t h = 0; h < nCount; h++) {
                uintptr_t handle = (uintptr_t)pHandles[h];
                bool active = false;
                for (int i = 0; i < 128; i++) {
                    if (thread_pool[i].active && thread_pool[i].handle == handle) {
                        active = true;
                        break;
                    }
                }
                if (!active) return h; // Thread finished, return WAIT_OBJECT_0 + h
            }
        }
        
        // 3. Handle timeout
        if (dwMilliseconds != 0xFFFFFFFF && elapsed >= dwMilliseconds) {
            return 0x102; // WAIT_TIMEOUT
        }
        
        // Sleep and increment elapsed time
        sleep_ms(10);
        if (dwMilliseconds != 0xFFFFFFFF) elapsed += 10;
    }
}
}

uint64_t resolve_windows_api_internal(const char* dll_name, const char* func_name, bool silent) {
    if (!dll_name) return 0;
    if (!func_name) {
        if (!silent) {
            char msg[128];
            snprintf(msg, 128, "[HYBRID] REQUEST: %s -> Ordinal\n", dll_name);
            print_win(&windows[15], msg);
            return generate_dummy_stub("Ordinal");
        }
        return 0;
    }

    char msg[128];
    if (!silent) {
        snprintf(msg, 128, "[HYBRID] REQUEST: %s -> %s\n", dll_name, func_name);
        print_win(&windows[15], msg);
    }

    // 0. The API Translator (MeinOS-WINE)
    if (str_iequals(dll_name, "d3d9.dll")) {
        if (str_iequals(func_name, "Direct3DCreate9")) return (uint64_t)Direct3DCreate9;
    }
    if (str_iequals(dll_name, "user32.dll")) {
        if (str_iequals(func_name, "MessageBoxA")) return (uint64_t)meinos_MessageBoxA;
        if (str_iequals(func_name, "CreateWindowExA")) return (uint64_t)meinos_CreateWindowExA;
        if (str_iequals(func_name, "ShowWindow")) return (uint64_t)meinos_ShowWindow;
        if (str_iequals(func_name, "MoveWindow")) return (uint64_t)meinos_MoveWindow;
        if (str_iequals(func_name, "DestroyWindow")) return (uint64_t)meinos_DestroyWindow;
        if (str_iequals(func_name, "GetDlgItem")) return (uint64_t)meinos_GetDlgItem;
        if (str_iequals(func_name, "CheckDlgButton")) return (uint64_t)meinos_CheckDlgButton;
        if (str_iequals(func_name, "IsDlgButtonChecked")) return (uint64_t)meinos_IsDlgButtonChecked;
        if (str_iequals(func_name, "EnableWindow")) return (uint64_t)meinos_EnableWindow;
        if (str_iequals(func_name, "GetMessageA")) return (uint64_t)meinos_GetMessageA;
        if (str_iequals(func_name, "GetMessageW")) return (uint64_t)meinos_GetMessageW;
        if (str_iequals(func_name, "PeekMessageA")) return (uint64_t)meinos_PeekMessageA;
        if (str_iequals(func_name, "PeekMessageW")) return (uint64_t)meinos_PeekMessageW;
        if (str_iequals(func_name, "TranslateMessage")) return (uint64_t)meinos_TranslateMessage;
        if (str_iequals(func_name, "DispatchMessageA")) return (uint64_t)meinos_DispatchMessageA;
        if (str_iequals(func_name, "DispatchMessageW")) return (uint64_t)meinos_DispatchMessageW;
        if (str_iequals(func_name, "DefWindowProcA")) return (uint64_t)meinos_DefWindowProcA;
        if (str_iequals(func_name, "DefWindowProcW")) return (uint64_t)meinos_DefWindowProcW;
        if (str_iequals(func_name, "SendMessageA")) return (uint64_t)meinos_SendMessageA;
        if (str_iequals(func_name, "SendMessageW")) return (uint64_t)meinos_SendMessageW;
        if (str_iequals(func_name, "PostQuitMessage")) return (uint64_t)meinos_PostQuitMessage;
        if (str_iequals(func_name, "RegisterClassA")) return (uint64_t)meinos_RegisterClassA;
        if (str_iequals(func_name, "RegisterClassExA")) return (uint64_t)meinos_RegisterClassExA;
        if (str_iequals(func_name, "GetClientRect")) return (uint64_t)meinos_GetClientRect;
        if (str_iequals(func_name, "SetWindowTextA")) return (uint64_t)meinos_SetWindowTextA;
        if (str_iequals(func_name, "InvalidateRect")) return (uint64_t)meinos_InvalidateRect;
        if (str_iequals(func_name, "BeginPaint")) return (uint64_t)meinos_BeginPaint;
        if (str_iequals(func_name, "EndPaint")) return (uint64_t)meinos_EndPaint;
        if (str_iequals(func_name, "GetDC")) return (uint64_t)meinos_GetDC;
        if (str_iequals(func_name, "ReleaseDC")) return (uint64_t)meinos_ReleaseDC;
        if (str_iequals(func_name, "FillRect")) return (uint64_t)meinos_FillRect;
        if (str_iequals(func_name, "CreateSolidBrush")) return (uint64_t)meinos_CreateSolidBrush;
        if (str_iequals(func_name, "DeleteObject")) return (uint64_t)meinos_DeleteObject;
        if (str_iequals(func_name, "SelectObject")) return (uint64_t)meinos_SelectObject;
        if (str_iequals(func_name, "GetSystemMetrics")) return (uint64_t)meinos_GetSystemMetrics;
        if (str_iequals(func_name, "CreatePen")) return (uint64_t)meinos_CreatePen;
        if (str_iequals(func_name, "MoveToEx")) return (uint64_t)meinos_MoveToEx;
        if (str_iequals(func_name, "LineTo")) return (uint64_t)meinos_LineTo;
        if (str_iequals(func_name, "CreateFontIndirectW")) return (uint64_t)meinos_CreateFontIndirectW;
        if (str_iequals(func_name, "GetSysColor")) return (uint64_t)meinos_GetSysColor;
        if (str_iequals(func_name, "RedrawWindow")) return (uint64_t)meinos_RedrawWindow;
        if (str_iequals(func_name, "GetTextMetricsW")) return (uint64_t)meinos_GetTextMetricsW;
        if (str_iequals(func_name, "GetTextFaceW")) return (uint64_t)meinos_GetTextFaceW;
        if (str_iequals(func_name, "GetTextExtentPoint32W")) return (uint64_t)meinos_GetTextExtentPoint32W;
        if (str_iequals(func_name, "GetTextExtentPoint32A")) return (uint64_t)meinos_GetTextExtentPoint32A;
    } else if (str_iequals(dll_name, "gdi32.dll")) {
        if (str_iequals(func_name, "TextOutA")) return (uint64_t)meinos_TextOutA;
        if (str_iequals(func_name, "CreateSolidBrush")) return (uint64_t)meinos_CreateSolidBrush;
        if (str_iequals(func_name, "DeleteObject")) return (uint64_t)meinos_DeleteObject;
        if (str_iequals(func_name, "SelectObject")) return (uint64_t)meinos_SelectObject;
        if (str_iequals(func_name, "CreatePen")) return (uint64_t)meinos_CreatePen;
        if (str_iequals(func_name, "MoveToEx")) return (uint64_t)meinos_MoveToEx;
        if (str_iequals(func_name, "LineTo")) return (uint64_t)meinos_LineTo;
        if (str_iequals(func_name, "CreateFontIndirectW")) return (uint64_t)meinos_CreateFontIndirectW;
        if (str_iequals(func_name, "GetTextMetricsW")) return (uint64_t)meinos_GetTextMetricsW;
        if (str_iequals(func_name, "GetTextFaceW")) return (uint64_t)meinos_GetTextFaceW;
        if (str_iequals(func_name, "GetTextExtentPoint32W")) return (uint64_t)meinos_GetTextExtentPoint32W;
        if (str_iequals(func_name, "GetTextExtentPoint32A")) return (uint64_t)meinos_GetTextExtentPoint32A;
        // In some Windows versions, BeginPaint/FillRect is in USER32, sometimes GDI32, we map them too if requested
        if (str_iequals(func_name, "BeginPaint")) return (uint64_t)meinos_BeginPaint;
        if (str_iequals(func_name, "EndPaint")) return (uint64_t)meinos_EndPaint;
        if (str_iequals(func_name, "GetDC")) return (uint64_t)meinos_GetDC;
        if (str_iequals(func_name, "ReleaseDC")) return (uint64_t)meinos_ReleaseDC;
        if (str_iequals(func_name, "FillRect")) return (uint64_t)meinos_FillRect;
    } else if (str_iequals(dll_name, "gdiplus.dll")) {
        if (str_iequals(func_name, "GdipCreateFromHDC")) return (uint64_t)meinos_GdipCreateFromHDC;
        if (str_iequals(func_name, "GdipDeleteGraphics")) return (uint64_t)meinos_GdipDeleteGraphics;
        if (str_iequals(func_name, "GdipCreateSolidFill")) return (uint64_t)meinos_GdipCreateSolidFill;
        if (str_iequals(func_name, "GdipDeleteBrush")) return (uint64_t)meinos_GdipDeleteBrush;
        if (str_iequals(func_name, "GdipFillRectangleI")) return (uint64_t)meinos_GdipFillRectangleI;
        if (str_iequals(func_name, "GdipDrawImageRectI")) return (uint64_t)meinos_GdipDrawImageRectI;
        if (str_iequals(func_name, "GdipGetImageWidth")) return (uint64_t)meinos_GdipGetImageWidth;
        if (str_iequals(func_name, "GdipGetImageHeight")) return (uint64_t)meinos_GdipGetImageHeight;
        if (str_iequals(func_name, "GdipAlloc")) return (uint64_t)meinos_GdipAlloc;
        if (str_iequals(func_name, "GdipFree")) return (uint64_t)meinos_GdipFree;
        if (str_iequals(func_name, "GdipCreateBitmapFromStream")) return (uint64_t)meinos_GdipCreateBitmapFromStream;
        if (str_iequals(func_name, "GdipLoadImageFromStream")) return (uint64_t)meinos_GdipLoadImageFromStream;
        if (str_iequals(func_name, "GdipDisposeImage")) return (uint64_t)meinos_GdipDisposeImage;
        if (str_iequals(func_name, "GdipDrawImageRectI")) return (uint64_t)meinos_GdipDrawImageRectI;
        if (str_iequals(func_name, "GdipGetImageWidth")) return (uint64_t)meinos_GdipGetImageWidth;
        if (str_iequals(func_name, "GdipGetImageHeight")) return (uint64_t)meinos_GdipGetImageHeight;
    } else if (str_iequals(dll_name, "kernel32.dll")) {
        if (str_iequals(func_name, "GetStdHandle")) return (uint64_t)meinos_GetStdHandle;
        if (str_iequals(func_name, "WriteFile")) return (uint64_t)meinos_WriteFile;
        if (str_iequals(func_name, "ExitProcess")) return (uint64_t)meinos_ExitProcess;
        if (str_iequals(func_name, "LoadLibraryA")) return (uint64_t)meinos_LoadLibraryA;
        if (str_iequals(func_name, "GetProcAddress")) return (uint64_t)meinos_GetProcAddress;
        if (str_iequals(func_name, "VirtualProtect")) return (uint64_t)meinos_VirtualProtect;
        if (str_iequals(func_name, "VirtualQuery")) return (uint64_t)meinos_VirtualQuery;
        if (str_iequals(func_name, "GetCurrentProcess")) return (uint64_t)meinos_GetCurrentProcess;
        if (str_iequals(func_name, "GetCurrentThread")) return (uint64_t)meinos_GetCurrentThread;
        if (str_iequals(func_name, "DuplicateHandle")) return (uint64_t)meinos_DuplicateHandle;
        if (str_iequals(func_name, "GetWindowLongPtrA")) return (uint64_t)meinos_GetWindowLongPtrA;
        if (str_iequals(func_name, "SetWindowLongPtrA")) return (uint64_t)meinos_SetWindowLongPtrA;
        if (str_iequals(func_name, "GetWindowLongA")) return (uint64_t)meinos_GetWindowLongA;
        if (str_iequals(func_name, "SetWindowLongA")) return (uint64_t)meinos_SetWindowLongA;
        if (str_iequals(func_name, "CallWindowProcA")) return (uint64_t)meinos_CallWindowProcA;
        if (str_iequals(func_name, "FindResourceA")) return (uint64_t)meinos_FindResourceA;
        if (str_iequals(func_name, "FindResourceW")) return (uint64_t)meinos_FindResourceW;
        if (str_iequals(func_name, "FindResourceExW")) return (uint64_t)meinos_FindResourceExW;
        if (str_iequals(func_name, "SizeofResource")) return (uint64_t)meinos_SizeofResource;
        if (str_iequals(func_name, "LoadResource")) return (uint64_t)meinos_LoadResource;
        if (str_iequals(func_name, "LockResource")) return (uint64_t)meinos_LockResource;

        if (str_iequals(func_name, "CreateSemaphoreA")) return (uint64_t)meinos_CreateSemaphoreA;
        if (str_iequals(func_name, "ReleaseSemaphore")) return (uint64_t)meinos_ReleaseSemaphore;
        if (str_iequals(func_name, "SetThreadPriority")) return (uint64_t)meinos_SetThreadPriority;
        if (str_iequals(func_name, "GetThreadPriority")) return (uint64_t)meinos_GetThreadPriority;
        if (str_iequals(func_name, "ResumeThread")) return (uint64_t)meinos_ResumeThread;
        if (str_iequals(func_name, "GetHandleInformation")) return (uint64_t)meinos_GetHandleInformation;
        if (str_iequals(func_name, "RaiseException")) return (uint64_t)meinos_RaiseException;
        if (str_iequals(func_name, "CloseHandle")) return (uint64_t)meinos_CloseHandle;
        if (str_iequals(func_name, "CreateFileW")) return (uint64_t)meinos_CreateFileW;
        if (str_iequals(func_name, "CreateEventA")) return (uint64_t)meinos_CreateEventA;
        if (str_iequals(func_name, "CreateEventW")) return (uint64_t)meinos_CreateEventW;
        if (str_iequals(func_name, "SetEvent")) return (uint64_t)meinos_SetEvent;
        if (str_iequals(func_name, "ResetEvent")) return (uint64_t)meinos_ResetEvent;
        if (str_iequals(func_name, "WaitForSingleObject")) return (uint64_t)meinos_WaitForSingleObject;
        if (str_iequals(func_name, "WaitForMultipleObjects")) return (uint64_t)meinos_WaitForMultipleObjects;

        // Memory & CRT Stubs
        if (str_iequals(func_name, "FlsAlloc")) return (uint64_t)meinos_FlsAlloc;
        if (str_iequals(func_name, "FlsSetValue")) return (uint64_t)meinos_FlsSetValue;
        if (str_iequals(func_name, "FlsGetValue")) return (uint64_t)meinos_FlsGetValue;
        if (str_iequals(func_name, "FlsGetValue2")) return (uint64_t)meinos_FlsGetValue; // Fallback
        if (str_iequals(func_name, "FlsFree")) return (uint64_t)meinos_TlsFree;
        if (str_iequals(func_name, "TlsAlloc")) return (uint64_t)meinos_TlsAlloc;
        if (str_iequals(func_name, "TlsGetValue")) return (uint64_t)meinos_TlsGetValue;
        if (str_iequals(func_name, "TlsSetValue")) return (uint64_t)meinos_TlsSetValue;
        if (str_iequals(func_name, "TlsFree")) return (uint64_t)meinos_TlsFree;
        if (str_iequals(func_name, "GetLocalTime")) return (uint64_t)meinos_GetLocalTime;
        if (str_iequals(func_name, "GetProcessHeap")) return (uint64_t)meinos_GetProcessHeap;
        if (str_iequals(func_name, "HeapAlloc")) return (uint64_t)meinos_HeapAlloc;
        if (str_iequals(func_name, "HeapFree")) return (uint64_t)meinos_HeapFree;
        if (str_iequals(func_name, "HeapSize")) return (uint64_t)meinos_HeapSize;
        if (str_iequals(func_name, "HeapReAlloc")) return (uint64_t)meinos_HeapReAlloc;
        if (str_iequals(func_name, "HeapCreate")) return (uint64_t)meinos_HeapCreate;
        if (str_iequals(func_name, "HeapDestroy")) return (uint64_t)meinos_HeapDestroy;
        if (str_iequals(func_name, "InitializeCriticalSectionEx")) return (uint64_t)meinos_InitializeCriticalSectionEx;
        if (str_iequals(func_name, "InitializeCriticalSectionAndSpinCount")) return (uint64_t)meinos_InitializeCriticalSectionAndSpinCount;
        if (str_iequals(func_name, "EncodePointer")) return (uint64_t)meinos_EncodePointer;
        if (str_iequals(func_name, "DecodePointer")) return (uint64_t)meinos_EncodePointer;
        if (str_iequals(func_name, "IsProcessorFeaturePresent")) return (uint64_t)meinos_IsProcessorFeaturePresent;

        if (str_iequals(func_name, "GetCommandLineA")) return (uint64_t)meinos_GetCommandLineA;
        if (str_iequals(func_name, "GetCommandLineW")) return (uint64_t)meinos_GetCommandLineW;
        if (str_iequals(func_name, "GetEnvironmentStringsW")) return (uint64_t)meinos_GetEnvironmentStringsW;
        if (str_iequals(func_name, "FreeEnvironmentStringsW")) return (uint64_t)meinos_FreeEnvironmentStringsW;
        if (str_iequals(func_name, "GetACP")) return (uint64_t)meinos_GetACP;
        if (str_iequals(func_name, "GetOEMCP")) return (uint64_t)meinos_GetOEMCP;
        if (str_iequals(func_name, "GetFileType")) return (uint64_t)meinos_GetFileType;
        if (str_iequals(func_name, "GetSystemTimeAsFileTime")) return (uint64_t)meinos_GetSystemTimeAsFileTime;
        if (str_iequals(func_name, "GetStartupInfoW")) return (uint64_t)meinos_GetStartupInfoW;
        if (str_iequals(func_name, "GetStartupInfoA")) return (uint64_t)meinos_GetStartupInfoW;
        if (str_iequals(func_name, "QueryPerformanceCounter")) return (uint64_t)meinos_QueryPerformanceCounter;
        if (str_iequals(func_name, "GetCurrentProcessId")) return (uint64_t)meinos_GetCurrentProcessId;
        if (str_iequals(func_name, "GetCurrentThreadId")) return (uint64_t)meinos_GetCurrentThreadId;
        if (str_iequals(func_name, "GetLastError")) return (uint64_t)meinos_GetLastError;
        if (str_iequals(func_name, "SetLastError")) return (uint64_t)meinos_SetLastError;
        if (str_iequals(func_name, "GetTickCount")) return (uint64_t)meinos_GetTickCount;
        if (str_iequals(func_name, "GetTickCount64")) return (uint64_t)meinos_GetTickCount64;
        if (str_iequals(func_name, "SetUnhandledExceptionFilter")) return (uint64_t)meinos_SetUnhandledExceptionFilter;
        if (str_iequals(func_name, "UnhandledExceptionFilter")) return (uint64_t)meinos_UnhandledExceptionFilter;
        if (str_iequals(func_name, "CreateThread")) return (uint64_t)meinos_CreateThread;
        if (str_iequals(func_name, "QueryPerformanceFrequency")) return (uint64_t)meinos_QueryPerformanceFrequency;
        if (str_iequals(func_name, "GetModuleFileNameA")) return (uint64_t)meinos_GetModuleFileNameA;
        if (str_iequals(func_name, "GetModuleFileNameW")) return (uint64_t)meinos_GetModuleFileNameW;
        if (str_iequals(func_name, "CreateFileA")) return (uint64_t)meinos_CreateFileA;
        if (str_iequals(func_name, "CreateFileW")) return (uint64_t)meinos_CreateFileW;
        if (str_iequals(func_name, "ReadFile")) return (uint64_t)meinos_ReadFile;
        if (str_iequals(func_name, "GetFileSize")) return (uint64_t)meinos_GetFileSize;
        if (str_iequals(func_name, "SetFilePointer")) return (uint64_t)meinos_SetFilePointer;
        if (str_iequals(func_name, "SetFilePointerEx")) return (uint64_t)meinos_SetFilePointerEx;
        if (str_iequals(func_name, "GetSystemInfo")) return (uint64_t)meinos_GetSystemInfo;
        if (str_iequals(func_name, "GlobalMemoryStatusEx")) return (uint64_t)meinos_GlobalMemoryStatusEx;
    } else if (str_iequals(dll_name, "shlwapi.dll")) {
        if (str_iequals(func_name, "PathFileExistsW")) return (uint64_t)meinos_PathFileExistsW;
    } else if (str_iequals(dll_name, "advapi32.dll")) {
        if (str_iequals(func_name, "RegOpenKeyExA")) return (uint64_t)meinos_RegOpenKeyExA;
        if (str_iequals(func_name, "RegOpenKeyExW")) return (uint64_t)meinos_RegOpenKeyExW;
        if (str_iequals(func_name, "RegQueryValueExA")) return (uint64_t)meinos_RegQueryValueExA;
        if (str_iequals(func_name, "RegQueryValueExW")) return (uint64_t)meinos_RegQueryValueExW;
        if (str_iequals(func_name, "RegCreateKeyExA")) return (uint64_t)meinos_RegCreateKeyExA;
        if (str_iequals(func_name, "RegSetValueExA")) return (uint64_t)meinos_RegSetValueExA;
        if (str_iequals(func_name, "RegCloseKey")) return (uint64_t)meinos_RegCloseKey;
    } else if (strncmp(dll_name, "api-ms-win-crt", 14) == 0 || str_iequals(dll_name, "ucrtbase.dll") || str_iequals(dll_name, "msvcrt.dll")) {
        if (str_iequals(func_name, "memcpy")) return (uint64_t)meinos_memcpy;
        if (str_iequals(func_name, "memset")) return (uint64_t)meinos_memset;
        if (str_iequals(func_name, "memmove")) return (uint64_t)meinos_memmove;
        if (str_iequals(func_name, "memcmp")) return (uint64_t)meinos_memcmp;
        if (str_iequals(func_name, "__iob_func")) return (uint64_t)meinos_iob_func;
        if (str_iequals(func_name, "fwrite")) return (uint64_t)meinos_fwrite;
        if (str_iequals(func_name, "fputs")) return (uint64_t)meinos_fputs;
        if (str_iequals(func_name, "malloc")) return (uint64_t)meinos_malloc;
        if (str_iequals(func_name, "free")) return (uint64_t)meinos_free;
        if (str_iequals(func_name, "_free_base")) return (uint64_t)meinos_free;
        if (str_iequals(func_name, "calloc")) return (uint64_t)meinos_calloc;
        if (str_iequals(func_name, "realloc")) return (uint64_t)meinos_realloc;
        if (str_iequals(func_name, "strlen")) return (uint64_t)meinos_strlen;
        if (str_iequals(func_name, "strchr")) return (uint64_t)meinos_strchr;
        if (str_iequals(func_name, "strcmp")) return (uint64_t)meinos_strcmp;
        if (str_iequals(func_name, "strncmp")) return (uint64_t)meinos_strncmp;
        if (str_iequals(func_name, "abort")) return (uint64_t)meinos_abort;
        if (str_iequals(func_name, "exit")) return (uint64_t)meinos_exit;
        if (str_iequals(func_name, "_exit")) return (uint64_t)meinos_exit;
        if (str_iequals(func_name, "terminate")) return (uint64_t)meinos_abort;
        if (str_iequals(func_name, "__acrt_iob_func")) return (uint64_t)meinos_acrt_iob_func;
        if (str_iequals(func_name, "__stdio_common_vfprintf")) return (uint64_t)meinos_stdio_common_vfprintf;
        if (str_iequals(func_name, "__C_specific_handler")) return (uint64_t)meinos_C_specific_handler;
        if (str_iequals(func_name, "_ismbblead")) return (uint64_t)meinos_ismbblead;
        if (str_iequals(func_name, "__p__fmode")) return (uint64_t)meinos_p__fmode;
        if (str_iequals(func_name, "__p__commode")) return (uint64_t)meinos_p__commode;
        if (str_iequals(func_name, "_beginthreadex")) return (uint64_t)meinos_beginthreadex;
        if (str_iequals(func_name, "_set_invalid_parameter_handler")) return (uint64_t)meinos_set_invalid_parameter_handler;
        if (str_iequals(func_name, "_set_app_type")) return (uint64_t)meinos_set_app_type;
        if (str_iequals(func_name, "_configthreadlocale")) return (uint64_t)meinos_configthreadlocale;
        if (str_iequals(func_name, "__setusermatherr")) return (uint64_t)meinos_setusermatherr;
        if (str_iequals(func_name, "_configure_narrow_argv")) return (uint64_t)meinos_configure_narrow_argv;
        if (str_iequals(func_name, "_initialize_narrow_environment")) return (uint64_t)meinos_initialize_narrow_environment;
        if (str_iequals(func_name, "_initterm")) return (uint64_t)meinos_initterm;
        if (str_iequals(func_name, "_initterm_e")) return (uint64_t)meinos_initterm_e;
        if (str_iequals(func_name, "__p___argc")) return (uint64_t)meinos_p___argc;
        if (str_iequals(func_name, "__p___argv")) return (uint64_t)meinos_p___argv;
        if (str_iequals(func_name, "__p___wargv")) return (uint64_t)meinos_p___wargv;
        if (str_iequals(func_name, "__p___wargv")) return (uint64_t)meinos_p___wargv;
        if (str_iequals(func_name, "__p__environ")) return (uint64_t)meinos_p__environ;
        if (str_iequals(func_name, "__p__wenviron")) return (uint64_t)meinos_p__wenviron;
        if (str_iequals(func_name, "__p__acmdln")) return (uint64_t)meinos_p__acmdln;
        if (str_iequals(func_name, "__p__wcmdln")) return (uint64_t)meinos_p__wcmdln;
        if (str_iequals(func_name, "_time64")) return (uint64_t)meinos_time64;
        if (str_iequals(func_name, "_set_new_mode")) return (uint64_t)meinos_set_new_mode;
        if (str_iequals(func_name, "_crt_atexit")) return (uint64_t)meinos_crt_atexit;
        if (str_iequals(func_name, "_register_onexit_function")) return (uint64_t)meinos_register_onexit_function;
        if (str_iequals(func_name, "_initialize_onexit_table")) return (uint64_t)meinos_initialize_onexit_table;
        if (str_iequals(func_name, "_execute_onexit_table")) return (uint64_t)meinos_execute_onexit_table;
    } else if (str_iequals(dll_name, "comdlg32.dll")) {
        if (str_iequals(func_name, "GetOpenFileNameA")) return (uint64_t)meinos_GetOpenFileNameA;
        if (str_iequals(func_name, "GetSaveFileNameA")) return (uint64_t)meinos_GetSaveFileNameA;
        if (str_iequals(func_name, "GetOpenFileNameW")) return (uint64_t)meinos_GetOpenFileNameW;
        if (str_iequals(func_name, "GetSaveFileNameW")) return (uint64_t)meinos_GetSaveFileNameW;
    }

    // 1. Load or retrieve the native DLL from /mnt/windows
    
        if (str_iequals(func_name, "GetWindowLongPtrA")) return (uint64_t)meinos_GetWindowLongPtrA;
        if (str_iequals(func_name, "SetWindowLongPtrA")) return (uint64_t)meinos_SetWindowLongPtrA;
        if (str_iequals(func_name, "GetWindowLongA")) return (uint64_t)meinos_GetWindowLongA;
        if (str_iequals(func_name, "SetWindowLongA")) return (uint64_t)meinos_SetWindowLongA;
        if (str_iequals(func_name, "CallWindowProcA")) return (uint64_t)meinos_CallWindowProcA;
        if (str_iequals(func_name, "FindResourceA")) return (uint64_t)meinos_FindResourceA;
        if (str_iequals(func_name, "FindResourceW")) return (uint64_t)meinos_FindResourceW;
        if (str_iequals(func_name, "FindResourceExW")) return (uint64_t)meinos_FindResourceExW;
        if (str_iequals(func_name, "SizeofResource")) return (uint64_t)meinos_SizeofResource;
        if (str_iequals(func_name, "LoadResource")) return (uint64_t)meinos_LoadResource;
        if (str_iequals(func_name, "LockResource")) return (uint64_t)meinos_LockResource;
        if (str_iequals(func_name, "GetHandleInformation")) return (uint64_t)meinos_GetHandleInformation;
        if (str_iequals(func_name, "fopen")) return (uint64_t)meinos_fopen;
        if (str_iequals(func_name, "system")) return (uint64_t)meinos_system;
        if (str_iequals(func_name, "SHBrowseForFolderA")) return (uint64_t)meinos_SHBrowseForFolderA;
        if (str_iequals(func_name, "SHBrowseForFolderW")) return (uint64_t)meinos_SHBrowseForFolderW;
        if (str_iequals(func_name, "SHGetPathFromIDListA")) return (uint64_t)meinos_SHGetPathFromIDListA;
        if (str_iequals(func_name, "SHGetPathFromIDListW")) return (uint64_t)meinos_SHGetPathFromIDListW;
        if (str_iequals(func_name, "SHGetPathFromIDListA")) return (uint64_t)meinos_SHGetPathFromIDListA;

    
        if (str_iequals(func_name, "_endthreadex")) return (uint64_t)meinos_endthreadex;
        if (str_iequals(func_name, "EnumChildWindows")) return (uint64_t)meinos_EnumChildWindows;
        if (str_iequals(func_name, "InitializeCriticalSection")) return (uint64_t)meinos_InitializeCriticalSection;
        if (str_iequals(func_name, "EnterCriticalSection")) return (uint64_t)meinos_EnterCriticalSection;
        if (str_iequals(func_name, "LeaveCriticalSection")) return (uint64_t)meinos_LeaveCriticalSection;
        if (str_iequals(func_name, "DeleteCriticalSection")) return (uint64_t)meinos_DeleteCriticalSection;
        if (str_iequals(func_name, "fread")) return (uint64_t)meinos_fread;
        if (str_iequals(func_name, "fclose")) return (uint64_t)meinos_fclose;
        if (str_iequals(func_name, "fseek")) return (uint64_t)meinos_fseek;
        if (str_iequals(func_name, "ftell")) return (uint64_t)meinos_ftell;
        if (str_iequals(func_name, "AdjustTokenPrivileges")) return (uint64_t)meinos_AdjustTokenPrivileges;
        if (str_iequals(func_name, "Sleep")) return (uint64_t)meinos_Sleep;

    
        if (str_iequals(func_name, "OpenProcessToken")) return (uint64_t)meinos_OpenProcessToken;
        if (str_iequals(func_name, "LookupPrivilegeValueA")) return (uint64_t)meinos_LookupPrivilegeValueA;
        if (str_iequals(func_name, "LookupPrivilegeValueW")) return (uint64_t)meinos_LookupPrivilegeValueA;

        // Recently added APIs based on logs
        if (str_iequals(func_name, "RegQueryValueExA")) return (uint64_t)meinos_RegQueryValueExA;
        if (str_iequals(func_name, "RegOpenKeyExA")) return (uint64_t)meinos_RegOpenKeyExA;
        if (str_iequals(func_name, "RegCreateKeyExA")) return (uint64_t)meinos_RegCreateKeyExA;
        if (str_iequals(func_name, "GetDeviceCaps")) return (uint64_t)meinos_GetDeviceCaps;
        if (str_iequals(func_name, "GetFileAttributesW")) return (uint64_t)meinos_GetFileAttributesW;
        if (str_iequals(func_name, "MessageBoxW")) return (uint64_t)meinos_MessageBoxW;
        if (str_iequals(func_name, "MessageBoxExW")) return (uint64_t)meinos_MessageBoxExW;
        if (str_iequals(func_name, "SetCurrentDirectoryA")) return (uint64_t)meinos_SetCurrentDirectoryA;
        if (str_iequals(func_name, "UnhookWinEvent")) return (uint64_t)meinos_UnhookWinEvent;
        if (str_iequals(func_name, "DeleteFileA")) return (uint64_t)meinos_DeleteFileA;
        if (str_iequals(func_name, "CreateProcessA")) return (uint64_t)meinos_CreateProcessA;
        if (str_iequals(func_name, "CreateDirectoryA")) return (uint64_t)meinos_CreateDirectoryA;
        if (str_iequals(func_name, "MsgWaitForMultipleObjects")) return (uint64_t)meinos_MsgWaitForMultipleObjects;
        if (str_iequals(func_name, "LoadStringA")) return (uint64_t)meinos_LoadStringA;
        if (str_iequals(func_name, "ExitWindowsEx")) return (uint64_t)meinos_ExitWindowsEx;
        if (str_iequals(func_name, "InitCommonControls")) return (uint64_t)meinos_InitCommonControls;
        if (str_iequals(func_name, "InitCommonControlsEx")) return (uint64_t)meinos_InitCommonControlsEx;

        // More recently added APIs based on logs
        if (str_iequals(func_name, "CoCreateInstance")) return (uint64_t)meinos_CoCreateInstance;
        if (str_iequals(func_name, "GetThreadLocale")) return (uint64_t)meinos_GetThreadLocale;
        if (str_iequals(func_name, "SetThreadLocale")) return (uint64_t)meinos_SetThreadLocale;
        if (str_iequals(func_name, "FormatMessageW")) return (uint64_t)meinos_FormatMessageW;
        if (str_iequals(func_name, "FormatMessageA")) return (uint64_t)meinos_FormatMessageA;
        if (str_iequals(func_name, "GetDC")) return (uint64_t)meinos_GetDC;
        if (str_iequals(func_name, "GetVersionExA")) return (uint64_t)meinos_GetVersionExA;
        if (str_iequals(func_name, "GetVersionExW")) return (uint64_t)meinos_GetVersionExW;
        if (str_iequals(func_name, "LCMapStringEx")) return (uint64_t)meinos_LCMapStringEx;
        if (str_iequals(func_name, "lstrlenA")) return (uint64_t)meinos_lstrlenA;
        if (str_iequals(func_name, "lstrlenW")) return (uint64_t)meinos_lstrlenW;
        if (str_iequals(func_name, "GetFileVersionInfoSizeW")) return (uint64_t)meinos_GetFileVersionInfoSizeW;
        if (str_iequals(func_name, "GetFileVersionInfoW")) return (uint64_t)meinos_GetFileVersionInfoW;
        if (str_iequals(func_name, "VerQueryValueW")) return (uint64_t)meinos_VerQueryValueW;
        if (str_iequals(func_name, "VerQueryValueA")) return (uint64_t)meinos_VerQueryValueA;
        if (str_iequals(func_name, "SetProcessDefaultLayout")) return (uint64_t)meinos_SetProcessDefaultLayout;
        if (str_iequals(func_name, "AllocateAndInitializeSid")) return (uint64_t)meinos_AllocateAndInitializeSid;
        if (str_iequals(func_name, "CheckTokenMembership")) return (uint64_t)meinos_CheckTokenMembership;
        if (str_iequals(func_name, "WldpQueryWindowsLockdownMode")) return (uint64_t)meinos_WldpQueryWindowsLockdownMode;
        if (str_iequals(func_name, "NtOpenProcessToken")) return (uint64_t)meinos_NtOpenProcessToken;
        if (str_iequals(func_name, "NtAdjustPrivilegesToken")) return (uint64_t)meinos_NtAdjustPrivilegesToken;
        if (str_iequals(func_name, "NtClose")) return (uint64_t)meinos_NtClose;
        if (str_iequals(func_name, "CreateMutexA")) return (uint64_t)meinos_CreateMutexA;
        if (str_iequals(func_name, "CreateMutexW")) return (uint64_t)meinos_CreateMutexW;
        if (str_iequals(func_name, "FreeSid")) return (uint64_t)meinos_FreeSid;
        if (str_iequals(func_name, "GetTempFileNameW")) return (uint64_t)meinos_GetTempFileNameW;
        if (str_iequals(func_name, "MessageBoxA")) return (uint64_t)meinos_MessageBoxA;
        if (str_iequals(func_name, "CreateDialogIndirectParamW")) return (uint64_t)meinos_CreateDialogIndirectParamW;
        if (str_iequals(func_name, "EnumFontFamiliesExA")) return (uint64_t)meinos_EnumFontFamiliesExA;
        if (str_iequals(func_name, "IsWindowVisible")) return (uint64_t)meinos_IsWindowVisible;
        if (str_iequals(func_name, "GetKeyState")) return (uint64_t)meinos_GetKeyState;
        if (str_iequals(func_name, "IsDialogMessageW")) return (uint64_t)meinos_IsDialogMessageW;
        if (str_iequals(func_name, "UpdateWindow")) return (uint64_t)meinos_UpdateWindow;
        if (str_iequals(func_name, "ChangeWindowMessageFilterEx")) return (uint64_t)meinos_ChangeWindowMessageFilterEx;
        if (str_iequals(func_name, "LCIDToLocaleName")) return (uint64_t)meinos_LCIDToLocaleName;
        if (str_iequals(func_name, "LoadStringW")) return (uint64_t)meinos_LoadStringW;
        if (str_iequals(func_name, "SetWinEventHook")) return (uint64_t)meinos_SetWinEventHook;
        if (str_iequals(func_name, "SetCurrentDirectoryW")) return (uint64_t)meinos_SetCurrentDirectoryW;
        if (str_iequals(func_name, "DeleteFileW")) return (uint64_t)meinos_DeleteFileW;
        if (str_iequals(func_name, "CoInitialize")) return (uint64_t)meinos_CoInitialize;
        if (str_iequals(func_name, "CoUninitialize")) return (uint64_t)meinos_CoUninitialize;
        if (str_iequals(func_name, "GetEnvironmentVariableA")) return (uint64_t)meinos_GetEnvironmentVariableA;
        if (str_iequals(func_name, "InterlockedExchange")) return (uint64_t)meinos_InterlockedExchange;
        if (str_iequals(func_name, "PathFileExistsW")) return (uint64_t)meinos_PathFileExistsW;
        if (str_iequals(func_name, "__wgetmainargs")) return (uint64_t)meinos_wgetmainargs;
        if (str_iequals(func_name, "AttachConsole")) return (uint64_t)meinos_AttachConsole;
        if (str_iequals(func_name, "GetExitCodeProcess")) return (uint64_t)meinos_GetExitCodeProcess;
        if (str_iequals(func_name, "GetTempPathW")) return (uint64_t)meinos_GetTempPathW;
        if (str_iequals(func_name, "SetDefaultDllDirectories")) return (uint64_t)meinos_SetDefaultDllDirectories;
        if (str_iequals(func_name, "GetUserNameA")) return (uint64_t)meinos_GetUserNameA;
        if (str_iequals(func_name, "GetFinalPathNameByHandleW")) return (uint64_t)meinos_GetFinalPathNameByHandleW;
        if (str_iequals(func_name, "GetSystemDirectoryW")) return (uint64_t)meinos_GetSystemDirectoryW;
        if (str_iequals(func_name, "SHGetSpecialFolderPathW")) return (uint64_t)meinos_SHGetSpecialFolderPathW;
        if (str_iequals(func_name, "GetFileAttributesA")) return (uint64_t)meinos_GetFileAttributesA;
        if (str_iequals(func_name, "IsValidCodePage")) return (uint64_t)meinos_IsValidCodePage;
        if (str_iequals(func_name, "InitializeSListHead")) return (uint64_t)meinos_InitializeSListHead;
        if (str_iequals(func_name, "GetUserDefaultUILanguage")) return (uint64_t)meinos_GetUserDefaultUILanguage;
        if (str_iequals(func_name, "GetCPInfo")) return (uint64_t)meinos_GetCPInfo;

    
        if (str_iequals(func_name, "LoadLibraryExA")) return (uint64_t)meinos_LoadLibraryExA;
        if (str_iequals(func_name, "LoadLibraryW")) return (uint64_t)meinos_LoadLibraryW;
        if (str_iequals(func_name, "LoadLibraryExW")) return (uint64_t)meinos_LoadLibraryExW;
                if (str_iequals(func_name, "_setjmp")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "longjmp")) return (uint64_t)my_longjmp;
        if (str_iequals(func_name, "_setjmpex")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "___mb_cur_max_func")) return (uint64_t)meinos_mb_cur_max_func;
        if (str_iequals(func_name, "___lc_codepage_func")) return (uint64_t)meinos_lc_codepage_func;
        if (str_iequals(func_name, "CharPrevA")) return (uint64_t)meinos_CharPrevA;

    
        if (str_iequals(func_name, "ExitThread")) return (uint64_t)meinos_ExitThread;
        if (str_iequals(func_name, "TerminateThread")) return (uint64_t)meinos_ExitThread;
        if (str_iequals(func_name, "_errno")) return (uint64_t)meinos_errno;
        if (str_iequals(func_name, "WideCharToMultiByte")) return (uint64_t)meinos_WideCharToMultiByte;
        if (str_iequals(func_name, "MultiByteToWideChar")) return (uint64_t)meinos_MultiByteToWideChar;
        if (str_iequals(func_name, "__wgetmainargs")) return (uint64_t)meinos_wgetmainargs;
        if (str_iequals(func_name, "PathFileExistsW")) return (uint64_t)meinos_PathFileExistsW;

    
        if (str_iequals(func_name, "CoTaskMemAlloc")) return (uint64_t)meinos_CoTaskMemAlloc;
        if (str_iequals(func_name, "CoTaskMemFree")) return (uint64_t)meinos_CoTaskMemFree;
        if (str_iequals(func_name, "EnableWindow")) return (uint64_t)meinos_EnableWindow;
        if (str_iequals(func_name, "CreateFontA")) return (uint64_t)meinos_CreateFontA;
        if (str_iequals(func_name, "GetModuleHandleA")) return (uint64_t)meinos_GetModuleHandleA;
        if (str_iequals(func_name, "GetModuleHandleW")) return (uint64_t)meinos_GetModuleHandleW;
        if (str_iequals(func_name, "GetModuleHandleExW")) return (uint64_t)meinos_GetModuleHandleExW;
        if (str_iequals(func_name, "GetCurrentPackageId")) return (uint64_t)meinos_GetCurrentPackageId;
        if (str_iequals(func_name, "HeapSetInformation")) return (uint64_t)meinos_HeapSetInformation;
        if (str_iequals(func_name, "GetStringTypeW")) return (uint64_t)meinos_GetStringTypeW;
        if (str_iequals(func_name, "FreeLibrary")) return (uint64_t)meinos_FreeLibrary;
        if (str_iequals(func_name, "CommandLineToArgvW")) return (uint64_t)meinos_CommandLineToArgvW;
        if (str_iequals(func_name, "CorExitProcess")) return (uint64_t)meinos_CorExitProcess;
        if (str_iequals(func_name, "GlobalAlloc")) return (uint64_t)meinos_GlobalAlloc;
        if (str_iequals(func_name, "GlobalFree")) return (uint64_t)meinos_GlobalFree;
        if (str_iequals(func_name, "GlobalLock")) return (uint64_t)meinos_GlobalLock;
        if (str_iequals(func_name, "GlobalUnlock")) return (uint64_t)meinos_GlobalUnlock;
        if (str_iequals(func_name, "CoCreateInstance")) return (uint64_t)meinos_CoCreateInstance;
        if (str_iequals(func_name, "CoInitializeEx")) return (uint64_t)meinos_CoInitializeEx;
        if (str_iequals(func_name, "CoInitializeSecurity")) return (uint64_t)meinos_CoInitializeSecurity;

    
        if (str_iequals(func_name, "GdipAlloc")) return (uint64_t)meinos_GdipAlloc;
        if (str_iequals(func_name, "GdipFree")) return (uint64_t)meinos_GdipFree;
        if (str_iequals(func_name, "CreateStreamOnHGlobal")) return (uint64_t)meinos_CreateStreamOnHGlobal;
        if (str_iequals(func_name, "OutputDebugStringA")) return (uint64_t)meinos_OutputDebugStringA;
        if (str_iequals(func_name, "OutputDebugStringW")) return (uint64_t)meinos_OutputDebugStringW;
                if (str_iequals(func_name, "_setjmp")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "longjmp")) return (uint64_t)my_longjmp;
        if (str_iequals(func_name, "_setjmpex")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "CoUninitialize")) return (uint64_t)meinos_CoUninitialize;

    
        if (str_iequals(func_name, "GdipCreateBitmapFromStream")) return (uint64_t)meinos_GdipCreateBitmapFromStream;
        if (str_iequals(func_name, "GdipLoadImageFromStream")) return (uint64_t)meinos_GdipLoadImageFromStream;
        if (str_iequals(func_name, "GdipDisposeImage")) return (uint64_t)meinos_GdipDisposeImage;
        if (str_iequals(func_name, "GdipDrawImageRectI")) return (uint64_t)meinos_GdipDrawImageRectI;
        if (str_iequals(func_name, "GdipGetImageWidth")) return (uint64_t)meinos_GdipGetImageWidth;
        if (str_iequals(func_name, "GdipGetImageHeight")) return (uint64_t)meinos_GdipGetImageHeight;

    uint64_t base = load_windows_dll(dll_name);
    if (base != 0) {
        // 2. Parse the true Export Directory of the loaded DLL
        uint64_t func_addr = get_pe_export((uint8_t*)base, func_name);
        if (func_addr != 0) {
            snprintf(msg, 128, "[HYBRID] RESOLVED: %s @ 0x%llX\n", func_name, (unsigned long long)func_addr);
            print_win(&windows[15], msg);
            return func_addr;
        }
    }

    if (!silent) {
        snprintf(msg, 128, "[HYBRID] RESOLVED (DUMMY): %s IN %s\n", func_name, dll_name);
        print_win(&windows[15], msg);
        return generate_dummy_stub(func_name);
    }
    return 0;
}

uint64_t resolve_windows_api(const char* dll_name, const char* func_name) {
    return resolve_windows_api_internal(dll_name, func_name, false);
}

extern "C" uint8_t __bss_start[];
extern "C" uint8_t __bss_end[];

// ==========================================
// WINE: MOCK API TABLE
// ==========================================
extern "C" void Mock_ExitProcess(uint32_t code) {
    while(1) { sleep_ms(100); }
}

extern "C" int Mock_MessageBoxA(void* hwnd, const char* text, const char* caption, uint32_t type) {
    if (text) {
        Window* w = &windows[2];
        print_win(w, "\n[WINE] MessageBoxA: ");
        print_win(w, text);
        print_win(w, "\n");
    }
    return 1;
}

BootInfo* global_boot_info = 0;
extern "C" void hs_start_sniffer();

extern "C" void kernel_main64(BootInfo* boot_info) {
	// Sag dem Kernel SOFORT, dass er tote Kind-Prozesse (Zombies) wegräumen soll
	signal(SIGCHLD, SIG_IGN);
    hs_start_sniffer();
    global_boot_info = boot_info;
    
#ifdef __linux__
    // BARE METAL FIX: Laptop-CPU auf Höchstleistung zwingen!
    // Verhindert, dass der Linux Power-Manager die FPS auf 20-30 drosselt.
    ::system("for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > $f 2>/dev/null; done");
#endif
    /// ==========================================
    /// BARE METAL FIX: KOMPLETTEN BSS-BEREICH NULLEN!
    /// ==========================================
#ifndef __linux__
    uint8_t* bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }
#endif

#ifdef __linux__
    // AUTO-LAUNCH EAappInstaller.exe
    {
        extern bool disk_read_file(const char* name, uint8_t* buf, uint32_t buf_sz);
        uint32_t fsize = 0;
        FILE* fsize_f = fopen("/opt/meinos/virtual_hdd/EAappInstaller.exe", "rb");
        if (fsize_f) {
            fseek(fsize_f, 0, SEEK_END);
            fsize = ftell(fsize_f);
            fclose(fsize_f);
        }
        if (fsize > 0 && fsize < 100*1024*1024) {
            uint8_t* exe_buf = (uint8_t*)malloc(fsize);
            if (exe_buf) {
                FILE* f = fopen("/opt/meinos/virtual_hdd/EAappInstaller.exe", "rb");
                if (f) {
                    fread(exe_buf, 1, fsize, f);
                    fclose(f);
                    extern void pe_execute(uint8_t* pe_data, uint32_t pe_sz);
                    pe_execute(exe_buf, fsize);
                }
            }
        }
    }
#endif
    
    // Ab hier ist float erlaubt!
    // init_heap();
	screen_w     = boot_info->screen_width;      
    screen_h     = boot_info->screen_height;
    screen_pitch = boot_info->framebuffer_pitch; 
    #ifndef __linux__
    fb           = (uint32_t*)(uint64_t)boot_info->framebuffer_addr;
#endif
    
    // LINUX FIX: Allocate bb if it is nullptr!
    if (!bb) bb = new uint32_t[1920 * 1080];

    cartridge_start = boot_info->cartridge_start;
    cartridge_end   = boot_info->cartridge_end;

    if (screen_pitch == 0) {
        screen_pitch = screen_w * 4; 
    }
    //fb = (_89*)(uint64_t)sys_info->framebuffer_addr;

    /// ==========================================
    /// 1. IDT (Interrupts) SAUBER AUFBAUEN
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
    
    ;
#endif
    
    /// TASK MANAGER INITIALISIEREN
    tasks[0].active = true;         /// Wir deklarieren Cosmos OS als Task 0
	// Anstatt des Test-Tasks starten wir jetzt einen sauberen Hintergrund-Task
    // Dieser Task kümmert sich um System-Metriken (z.B. Zeit, CPU-Last, etc.)
#ifndef __linux__
	create_task(sys_idle_task);
#endif
    //create_task(background_task);   /// Wir feuern unseren ersten echten Hintergrund-Task ab!
    #ifndef __linux__
    ; /// ZÜNDUNG: Multitasking beginnt!
#endif
    /// ==========================================
    /// DEIN ORIGINAL-CODE STARTET AB HIER WIEDER:
    /// ==========================================
	// Irgendwo in deiner Hintergrund-Schleife (z.B. nach dem Zeichnen der Sterne):
    read_rtc();
    get_cpu_brand();
    usb_mouse_callback = update_mouse_position;
    init_mouse();
	
    /// Sterne generieren
    _39(_43 i=0; i<200; i++) {
        stars[i].x = (cosmos_random() % 1599) - 799;
        stars[i].y = (cosmos_random() % 1199) - 599;
        stars[i].z = (cosmos_random() % 1000) + 1;
        stars[i].type = cosmos_random() % 1000; 
        /// BARE METAL FIX: Speed auf einen sauberen Mittelwert (4 bis 9)
        stars[i].speed = (cosmos_random() % 6) + 4;
    }
    /// ==========================================
    /// BARE METAL FIX: FENSTER SAUBER INITIALISIEREN
    /// ==========================================
    _39(_43 i=0; i<20; i++) { 
        windows[i].id = i; 
        windows[i].open = _86; 
        windows[i].minimized = _86; 
        windows[i].cursor_pos = 0; 
        mem_set((uint8_t*)windows[i].content, 0, 2048); 
    }
    mem_set((uint8_t*)cmd_input_buf, 0, 64);
    cmd_input_idx = 0;
    mem_set((uint8_t*)app_installer_buf, 0, 256);
    app_installer_idx = 0;
    mem_set((uint8_t*)sys_ota_buf, 0, 256);
    sys_ota_idx = 0;
    
    str_cpy(windows[0].title, "NOTEPAD");  windows[0].x=0; windows[0].y=0; windows[0].w=600; windows[0].h=300; windows[0].color=0xEEEEEE; 
    str_cpy(windows[1].title, "APPS");     windows[1].x=0; windows[1].y=300; windows[1].w=600; windows[1].h=300; windows[1].color=0xDDDDDD; 
    str_cpy(windows[2].title, "SAVE AS..."); windows[2].x=0; windows[2].y=150; windows[2].w=300; windows[2].h=200; windows[2].color=0xDDDDDD;
    str_cpy(windows[3].title, "SYSTEM");   windows[3].x=0; windows[3].y=0;  windows[3].w=600; windows[3].h=300; windows[3].color=0xFFD700;
    str_cpy(windows[4].title, "DISK MGR"); windows[4].x=0; windows[4].y=0; windows[4].w=600; windows[4].h=300; windows[4].color=0x888888; 
    str_cpy(windows[5].title, "CMD");      windows[5].x=0; windows[5].y=0; windows[5].w=600; windows[5].h=300; windows[5].color=0x111111;
    str_cpy(windows[6].title, "EMULATOR"); windows[6].x=0; windows[6].y=0; windows[6].w=600; windows[6].h=300; windows[6].color=0x222222;
    str_cpy(windows[7].title, "DEBUG"); windows[7].x=200; windows[7].y=150; windows[7].w=120; windows[7].h=170; windows[7].color=0x004488;
    str_cpy(windows[8].title, "ACCESS");   windows[8].x=300; windows[8].y=100; windows[8].w=250; windows[8].h=300; windows[8].color=0x44AAAA;
    str_cpy(windows[9].title, "IMAGE VIEWER"); windows[9].x=0; windows[9].y=0; windows[9].w=600; windows[9].h=300; windows[9].color=0x111111;
    str_cpy(windows[10].title, "GERAETE MGR"); windows[10].x=100; windows[10].y=80; windows[10].w=600; windows[10].h=300; windows[10].color=0x111111;
    str_cpy(windows[11].title, "ARCADE: PONG"); windows[11].x=200; windows[11].y=150; windows[11].w=600; windows[11].h=300; windows[11].color=0x112211;
    str_cpy(windows[12].title, "ARCADE: BLOBBY"); windows[12].x=250; windows[12].y=100; windows[12].w=600; windows[12].h=300; windows[12].color=0x4488FF;
    str_cpy(windows[14].title, "SMASH CATS 3D"); windows[14].x=250; windows[14].y=100; windows[14].w=600; windows[14].h=300; windows[14].color=0x4488FF;
    str_cpy(windows[15].title, "HYBRID KERNEL"); windows[15].x=0; windows[15].y=300; windows[15].w=600; windows[15].h=300; windows[15].color=0x000088;
	
    /// BARE METAL FIX: Alle Orakel-Variablen sofort befüllen!
	scan_pci_drives(&windows[4]);
    
    /// 1. Puffer komplett nullen (RAM Müll vernichten!)
    mem_set(hw_storage, 0, 256);
    mem_set(hw_net, 0, 256);
    mem_set(hw_gpu, 0, 256);

    /// 2. Die EINZIGE Scan-Schleife ausführen (pci.cpp)
#ifndef __linux__
    pci_scan_all();
#endif 
#ifndef __linux__
    pci_scan_advanced();
#endif
	check_incoming();
	//system_init_usb();
    /// 3. Bildschirm-Abschneider (Gegen das Worträtsel)
    hw_storage[35] = 0;
    hw_net[35] = 0;
    hw_gpu[35] = 0;
    hw_usb[35] = 0;
    /// Planeten initialisieren
    _43 clock_dirs[] = {213, 0, 42, 85, 128, 170}; 
    _39(_43 i=0; i<6; i++) { 
        planets[i].ang = clock_dirs[i]; planets[i].dist = 10;
        planets[i].cur_x = 400.0f; planets[i].cur_y = 300.0f;
        planets[i].real_ang = clock_dirs[i] * (3.14159f / 128.0f);
        str_cpy(planets[i].name, (i==0?(_30*)"TXT":i==1?(_30*)"APP":i==2?(_30*)"SYS":i==3?(_30*)"DSK":i==4?(_30*)"CMD":(_30*)"DEB")); 
    }
    _43 map_ids[]={0,1,3,4,5,6}; /// Verknüpfung Planet -> Fenster ID
    
    // ============================================================
    // HYBRID KERNEL INIT: PRE-LOAD NATIVE WINDOWS COMPONENTS
    // ============================================================
    windows[15].open = _128; // Open the Hybrid window by default
    windows[15].minimized = _86;
    print_win(&windows[15], "[HYBRID] KERNEL STARTING...\n");
    print_win(&windows[15], "[HYBRID] MOUNTING /dev/sdb1 to /mnt/windows...\n");
    load_windows_dll("ntoskrnl.exe");
    load_windows_dll("ntdll.dll");
    load_windows_dll("kernel32.dll");
    print_win(&windows[15], "[HYBRID] KERNEL READY. WAITING FOR TASKS...\n");
    // ============================================================

    _114(1) {
#ifndef __linux__
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
            if (!(d & 0x80)) {
                key_new = _128;
                last_app_key = get_ascii_qwertz(d);
            }
        }
#endif
        _15(input_cooldown > 0) input_cooldown--;
        click_consumed = _86; z_blocked = _86; _44 mouse_handled = _86;

        /// ==========================================
        /// BARE METAL FIX: Unser "DHCP-Button" (Taste D)
        /// ==========================================
        _15((last_app_key EQ 'd' OR last_app_key EQ 'D') AND input_cooldown EQ 0) {
            send_dhcp_discover();
            last_app_key = 0;       /// Taste als "verarbeitet" markieren
            input_cooldown = 25;    /// Kurz warten, damit er nicht 1000 Pakete feuert
        }

        _15(frame % 10 EQ 0) check_incoming();
        
        /// ==========================================
        /// BARE METAL FIX: PACKAGE MANAGER EXECUTION
        /// ==========================================
        static _43 pkg_last_download_len = 0;
        static _43 pkg_idle_frames = 0;
        static _43 pkg_syn_retry_cooldown = 0;
        
        if (pkg_download_active) {
            if (browser_tcp.state == 0) {
                pkg_syn_retry_cooldown++;
                if (pkg_syn_retry_cooldown > 60) {
                    pkg_syn_retry_cooldown = 0;
                    Window* p_win = pkg_output_win_ptr ? (Window*)pkg_output_win_ptr : &windows[5];
                    print_win(p_win, "DEBUG: SENDING TCP SYN...\n");
                    if (pkg_target_ip != 0) {
                        send_tcp_syn(pkg_target_ip, pkg_target_port);
                    } else if (browser_tcp.remote_ip != 0) {
                        send_tcp_syn(browser_tcp.remote_ip, pkg_target_port);
                    }
                }
            } else if (browser_download_complete == 0 && browser_download_len > 0) {
                if (browser_download_len != pkg_last_download_len) {
                    if ((browser_download_len / 1024) != (pkg_last_download_len / 1024)) {
                        Window* p_win = pkg_output_win_ptr ? (Window*)pkg_output_win_ptr : &windows[5];
                        print_win(p_win, "DOWNLOADING: ");
                        _30 tmp[20];
                        int_to_str(browser_download_len / 1024, tmp);
                        print_win(p_win, tmp);
                        print_win(p_win, " KB");
                        if (browser_content_length > 0) {
                            print_win(p_win, " / ");
                            _30 tmp2[20];
                            int_to_str(browser_content_length / 1024, tmp2);
                            print_win(p_win, tmp2);
                            print_win(p_win, " KB (");
                            _30 tmp3[10];
                            int_to_str((browser_download_len * 100) / browser_content_length, tmp3);
                            print_win(p_win, tmp3);
                            print_win(p_win, "%)");
                        }
                        print_win(p_win, "...\n");
                    }
                    pkg_last_download_len = browser_download_len;
                    pkg_idle_frames = 0;
                } else {
                    pkg_idle_frames++;
                    if (pkg_idle_frames > 300) {
                        browser_download_complete = 1;
                    }
                }
            } else {
                pkg_idle_frames++;
                if (pkg_idle_frames > 300) {
                    browser_download_complete = 1;
                }
            }
        }

        if (pkg_download_active && browser_download_complete) {
            pkg_download_active = 0;
            Window* c_win = pkg_output_win_ptr ? (Window*)pkg_output_win_ptr : &windows[5];
            
            if (browser_download_len > 0) {
                print_win(c_win, "DOWNLOAD COMPLETE. SAVING TO DISK...\n");
            } else {
                print_win(c_win, "DOWNLOAD FAILED: CONNECTION REFUSED!\n");
            }
            
            uint32_t payload_start = 0;
            for(uint32_t i=0; i<browser_download_len-4; i++) {
                if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' &&
                    browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') {
                    payload_start = i + 4;
                    break;
                }
                if (browser_download_buffer[i] == '\n' && browser_download_buffer[i+1] == '\n') {
                    payload_start = i + 2;
                    break;
                }
            }
            
            if (payload_start > 0 && payload_start < browser_download_len) {
                // Finde LBA und Slot für die neue Datei
                uint32_t next_lba = 11000;
                int free_slot = -1;
                for (int i = 0; i < 28; i++) {
                    if (cfs_files[i].exists && cfs_files[i].start_lba >= next_lba) {
                        next_lba = cfs_files[i].start_lba + ((cfs_files[i].size + 511) / 512) + 1;
                    }
                    if (cfs_files[i].exists == 0 && free_slot == -1) {
                        free_slot = i;
                    }
                }

                if (free_slot != -1) {
                    uint64_t buf_dir = (uint64_t)global_buf_dir; 
                    ahci_read_sectors(1002, buf_dir);
                    sleep_ms(20);
                    
                    CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)(unsigned long long)buf_dir;
                    dir[free_slot].type = 1;
                    dir[free_slot].file_size = browser_download_len - payload_start;
                    dir[free_slot].start_lba = next_lba;
                    
                    for(int n=0; n<11; n++) { dir[free_slot].filename[n] = 0; cfs_files[free_slot].name[n] = 0; }
                    
                    int slash_idx = 0;
                    for (int k=0; k<100; k++) {
                        if (browser_url[k] == '/') slash_idx = k + 1;
                        if (browser_url[k] == 0) break;
                    }
                    str_cpy(dir[free_slot].filename, &browser_url[slash_idx]);
                    str_cpy(cfs_files[free_slot].name, &browser_url[slash_idx]);
                    
                    cfs_files[free_slot].start_lba = next_lba;
                    cfs_files[free_slot].size = dir[free_slot].file_size;
                    cfs_files[free_slot].is_folder = _86;
                    cfs_files[free_slot].exists = 1;
                    
                    ahci_write_sectors(1002, (uint64_t)buf_dir);
                    sleep_ms(20);
                    
                    uint32_t payload_len = browser_download_len - payload_start;
                    uint32_t sectors = (payload_len + 511) / 512;
                    for (uint32_t s=0; s<sectors; s++) {
                        uint64_t ram_addr = (uint64_t)&browser_download_buffer[payload_start + s*512];
                        ahci_write_sectors(next_lba + s, ram_addr);
                    }
                    print_win(c_win, "SAVED TO VIRTUAL HDD!\n");
                    if (pkg_download_execute) {
                        uint64_t pe_entry = 0;
                        uint16_t pe_machine = 0;
                        uint16_t pe_sections = 0;
                        if (parse_and_inspect_pe(&browser_download_buffer[payload_start], payload_len, &pe_entry, &pe_machine, &pe_sections)) {
                            char pe_msg[128];
                            snprintf(pe_msg, 128, "BINGO! WINDOWS EXE ERKANNT!\nENTRY: 0x%lx\nARCH: 0x%x SECT: %d\n", (unsigned long)pe_entry, pe_machine, pe_sections);
                            print_win(c_win, pe_msg);
                            
                            print_win(c_win, "[SYS] LOADING PE SECTIONS TO VIRTUAL RAM...\n");
                            uint64_t final_entry = load_and_resolve_pe(&browser_download_buffer[payload_start], payload_len);
                            if (final_entry != 0) {
                                print_win(c_win, "[SYS] PE IAT RESOLVED. STARTING WINE ENVIRONMENT...\n");
                                create_task((void (*)()) final_entry);
                            } else {
                                print_win(c_win, "[ERR] WINE: FAILED TO MAP PE INTO MEMORY.\n");
                            }
                        } else {
                            print_win(c_win, "EXECUTING ELF...\n");
                            uint64_t entry_point = parse_and_load_elf(&browser_download_buffer[payload_start]);
                            if (entry_point != 0) {
                                create_task((void (*)()) entry_point);
                                print_win(c_win, "BINGO! ELF IS RUNNING!\n");
                            } else {
                                print_win(c_win, "ERROR: INVALID ELF OR EXE FILE.\n");
                            }
                        }
                    } else {
                        print_win(c_win, "DOWNLOAD SUCCESSFUL!\n");
                    }
                } else {
                    print_win(c_win, "ERROR: DISK FULL (NO SLOTS).\n");
                }
            } else {
                print_win(c_win, "ERROR: HTTP PAYLOAD NOT FOUND.\n");
            }
        }
        /// ==========================================
        /// ABTEILUNG: TASTATUR (BACK TO OS1 STABILITY)
        /// ==========================================
        if (key_ready) {
            key_ready = _86; /// Flag sofort zurücksetzen
            _184 sc = key_scancode;

            _15(!(sc & 0x80)) { /// Taste wurde GEDRÜCKT (Make Code)
                
                /// BARE METAL FIX: Die Taste als ASCII für unsere Apps kopieren!
                last_app_key = get_ascii_qwertz(sc);
                key_new = _128;
                /// ==========================================
                /// HIER KOMMT DEIN DEBUGGER REIN!
                /// Er überschreibt bei JEDEM Tastendruck den Status unten rechts!
                /// ==========================================
                _30 dbg_str[30] = "KEY: ";
                _30 h_str[5]; 
                byte_to_hex(sc, h_str);
                str_cat(dbg_str, h_str);
                str_cpy(cmd_status, dbg_str);
                
                /// 1. Globale Hotkeys (F-Tasten für Apps)
                _15(sc EQ 0x3B) system("mousepad &");        // F1 = TXT
                _15(sc EQ 0x3C) toggle_app(1);               // F2 = APP
                _15(sc EQ 0x3D) toggle_app(3);               // F3 = SYS
                _15(sc EQ 0x3E) system("pcmanfm &");         // F4 = DSK
                _15(sc EQ 0x3F) system("xfce4-terminal &");  // F5 = CMD
                _15(sc EQ 0x40) system("xfce4-appfinder &"); // F6 = DEB
                _15(sc EQ 0x44) toggle_app(7); /// F10 = SYSTEM DEBUG POPUP

                /// 2. Welches Fenster ist ganz oben (Fokus)?
                _43 fw_id = win_z[19]; 
                Window* fw = &windows[fw_id];
                
                _15(fw AND fw->open AND !fw->minimized) {
                    /// --- TASTATUR STEUERT DAS SYSTEM FENSTER (ID 3) ---
                    _15(fw->id EQ 3) {
                        _15(sc EQ 0x50) { // Pfeil Runter
                            if (sys_selected_item < 4) sys_selected_item++; 
                        } 
                        
                        _15(sc EQ 0x48) { // Pfeil Hoch
                            if (sys_selected_item > 0) sys_selected_item--; 
                        } 
                        
                        _15(sc EQ 0x1C) { // ENTER-TASTE
                            
                            /// INDEX 0: DER GROSSE ROTE ORAKEL-BUTTON OBEN
                            if (sys_selected_item == 0) {
                                mirror_count = 0;
                                for(uint32_t b=0; b<256; b++) {
                                    for(uint32_t d=0; d<32; d++) {
                                        for(uint32_t f=0; f<28; f++) {
                                            uint32_t id = pci_read(b, d, f, 0);
                                            if((id & 0xFFFF) != 0xFFFF && id != 0 && mirror_count < 30) {
                                                mirror_list[mirror_count].bus = b;
                                                mirror_list[mirror_count].dev = d;
                                                mirror_list[mirror_count].func = f;
                                                mirror_list[mirror_count].vendor = id & 0xFFFF;
                                                mirror_list[mirror_count].device = id >> 16;
                                                
                                                uint32_t bar0_l = pci_read(b, d, f, 0x10);
                                                uint32_t bar0_h = pci_read(b, d, f, 0x14);
                                                
                                                uint32_t class_rev = pci_read(b, d, f, 0x08);
                                                uint32_t cls = (class_rev >> 24) & 0xFF;
                                                uint32_t sub = (class_rev >> 16) & 0xFF;
                                                
                                                /// Ist es eine echte 64-Bit Adresse?
                                                if((bar0_l & 0x06) == 0x04) { 
                                                    mirror_list[mirror_count].bar0 = ((uint64_t)bar0_h << 32) | (bar0_l & 0xFFFFFFF0);
                                                } else {
                                                    mirror_list[mirror_count].bar0 = bar0_l & 0xFFFFFFF0;
                                                }
                                                
                                                if(cls == 0x0C && sub == 0x03) str_cpy(mirror_list[mirror_count].name, "USB 3.0 (xHCI)");
                                                else if(cls == 0x01 && sub == 0x06) str_cpy(mirror_list[mirror_count].name, "SATA (AHCI)");
                                                else if(cls == 0x02) str_cpy(mirror_list[mirror_count].name, "NETWORK");
                                                else if(cls == 0x03) str_cpy(mirror_list[mirror_count].name, "GRAPHICS");
                                                else str_cpy(mirror_list[mirror_count].name, "SYSTEM DEVICE");
                                                
                                                mirror_count++;
                                            }
                                        }
                                    }
                                }
                                /// WICHTIG: Das Orakel darf sich NUR hier beim Enter-Druck öffnen!
                                show_oracle = true; 
                            }
                            
                            /// INDEX 1, 2, 3: DIE ANDEREN HARDWARE-SCANS (Nur bei Enter!)
                            else if (sys_selected_item == 1) { scan_pci_class(0x01, hw_storage, "CTRL"); }
                            else if (sys_selected_item == 2) { scan_pci_class(0x02, hw_net, "NIC"); }
                            else if (sys_selected_item == 3) { scan_pci_class(0x03, hw_gpu, "GPU"); }
                                
                            /// INDEX 4: DER USB HOST START! (Nur bei Enter!)
                            else if (sys_selected_item == 4) {
                                system_init_usb(); 
                            }
                        }
                    }
					// ==========================================
					// ULTIMATIVER F9 BYPASS FÜR DIE 3D ENGINE
					// ==========================================
					_15(key_new AND key_scancode EQ 0x43) { // 0x43 = F9
						_15(windows[14].open EQ _86) {
							// Fenster ist zu -> ÖFFNEN!
							windows[14].id = 14;   
							windows[14].w = 800;   
							windows[14].h = 600;   
							windows[14].x = (screen_w - 800) / 2; // Zentriert  
							windows[14].y = (screen_h - 600) / 2;   
							
							// === DER WICHTIGSTE TEIL ===
							// Mache das Fenster für Cosmos "offiziell" sichtbar!
							windows[14].open = _128;
							windows[14].minimized = _86;
							windows[14].color = 0x4488FF;
							str_cpy(windows[14].title, "Super Smash Cats 3D");
							
							os_app_x = windows[14].x;
							os_app_y = windows[14].y + 35;
							os_app_w = windows[14].w;
							os_app_h = windows[14].h - 35;
							
							app_window_active = _128; 
							focus_window(14);
						} _41 {
							// Fenster ist offen -> SCHLIESSEN!
							windows[14].open = _86;
							app_window_active = _86;
						}
						key_new = _86; // Taste sofort konsumieren!
						last_app_key = 0;
						key_scancode = 0;
					}
                    /// --- NOTEPAD (ID 0) PURE OS1 LOGIC ---
                    _15(fw->id EQ 0) { 
                        _15(sc EQ 0x0E) { _15(fw->cursor_pos > 0) { fw->cursor_pos--; fw->content[fw->cursor_pos] = 0; } } 
                        _41 _15(sc EQ 0x1C) { _15(fw->cursor_pos < 2000) { fw->content[fw->cursor_pos++] = '\n'; fw->content[fw->cursor_pos] = 0; } } 
                        _41 { 
                            _30 c = get_ascii_qwertz(sc);
                            _15(c AND fw->cursor_pos < 2000) { fw->content[fw->cursor_pos++] = c; fw->content[fw->cursor_pos] = 0; } 
                        }
                    }
                    
                    /// --- CMD (ID 5) ---
                    _15(fw->id EQ 5) { 
                        _15(sc EQ 0x0E) { _15(cmd_input_idx > 0) { cmd_input_idx--; cmd_input_buf[cmd_input_idx] = 0; } } 
                        _41 _15(sc EQ 0x1C) { process_cmd(cmd_input_buf, fw); cmd_input_idx = 0; cmd_input_buf[0] = 0; } 
                        _41 { 
                            _30 c = get_ascii_qwertz(sc);
                            _15(c AND cmd_input_idx < 60) { cmd_input_buf[cmd_input_idx++] = c; cmd_input_buf[cmd_input_idx] = 0; } 
                        }
                    }
                    /// --- APP INSTALLER (ID 7) ---
                    _15(fw->id EQ 7) { 
                        _15(sc EQ 0x0E) { _15(app_installer_idx > 0) { app_installer_idx--; app_installer_buf[app_installer_idx] = 0; } } 
                        _41 _15(sc EQ 0x1C) { /* Enter tut vorerst nichts, User muss Button klicken */ } 
                        _41 { 
                            _30 c = get_ascii_qwertz(sc);
                            // We allow standard ascii for names
                            if (sc == 0x34) c = '.'; // Allow dots
                            else if (sc == 0x0C) c = '-'; // Allow minus
                            _15(c AND app_installer_idx < 250) { app_installer_buf[app_installer_idx++] = c; app_installer_buf[app_installer_idx] = 0; } 
                        }
                    }
                    /// --- SYS WINDOW OTA (ID 3) ---
                    _15(fw->id EQ 3) { 
                        _15(sc EQ 0x0E) { _15(sys_ota_idx > 0) { sys_ota_idx--; sys_ota_buf[sys_ota_idx] = 0; } } 
                        _41 _15(sc EQ 0x1C) { } 
                        _41 { 
                            _30 c = get_ascii_qwertz(sc);
                            if (sc == 0x34) c = '.'; 
                            else if (sc == 0x0C) c = '-'; 
                            else if (sc == 0x35) c = '_'; 
                            else if (sc == 0x1A) c = ':'; 
                            else if (sc == 0x0D) c = '=';
                            else if (sc == 0x2B) c = '~'; // or ? maybe. we'll rely mostly on PASTE for complex URLs.
                            _15(c AND sys_ota_idx < 250) { sys_ota_buf[sys_ota_idx++] = c; sys_ota_buf[sys_ota_idx] = 0; } 
                        }
                    }
                }
				/// --- SAVE AS DIALOG (ID 2) ---
                _15(fw->id EQ 2) { 
                    _15(sc EQ 0x0E) { /// Backspace
                        _15(save_step EQ 1 AND save_name_idx > 0) save_filename[--save_name_idx] = 0;
                        _41 _15(save_step EQ 2 AND folder_name_idx > 0) new_folder_name[--folder_name_idx] = 0;
                    } 
                    _41 { 
                        _30 c = get_ascii_qwertz(sc);
                        _15(c >= 32) { 
                            _15(save_step EQ 1 AND save_name_idx < 10) { 
                                save_filename[save_name_idx++] = c; 
                                save_filename[save_name_idx] = 0; 
                            }
                            _41 _15(save_step EQ 2 AND folder_name_idx < 10) { 
                                new_folder_name[folder_name_idx++] = c; 
                                new_folder_name[folder_name_idx] = 0; 
                            }
                        } 
                    }
                }
            }
        }
        _15(frame % 100 EQ 0) {
            read_rtc();
        }
        /// ==========================================
        /// FENSTER INTERAKTION (DRAG, RESIZE, BUTTONS)
        /// ==========================================
        _15(mouse_down) {
             _15(drag_win NEQ -1) { 
                 windows[drag_win].x = mouse_x - drag_off_x; 
                 windows[drag_win].y = mouse_y - drag_off_y; 
                 mouse_handled=_128; click_consumed=_128; 
             } 
             _41 _15(resize_win NEQ -1) { 
                 _43 nw = mouse_x - windows[resize_win].x; 
                 _43 nh = mouse_y - windows[resize_win].y; 
                 _15(nw > 100) windows[resize_win].w = (nw > screen_w) ? screen_w : nw; 
                 _15(nh > 100) windows[resize_win].h = (nh > screen_h) ? screen_h : nh; 
                 mouse_handled=_128; click_consumed=_128; 
             }
        } _41 { drag_win = -1; resize_win = -1; }
        
        _15(!mouse_handled) {
            _39(_43 i=19; i>=0; i--) { 
                _43 k = win_z[i]; Window* win=&windows[k];
                _15(win->open AND !win->minimized) {
                    _43 wx=(win->fullscreen?0:win->x); _43 wy=(win->fullscreen?0:win->y); _43 ww=(win->fullscreen?screen_w:win->w); _43 wh=(win->fullscreen?screen_h:win->h);
                    _15(mouse_x>=wx AND mouse_x<=wx+ww AND mouse_y>=wy AND mouse_y<=wy+wh) {
                        z_blocked = _128;
                        _15(mouse_just_pressed) {
                           click_consumed = _128; 
                           focus_window(k);
                           _43 bx = wx + ww/2; 
                           
                           /// DIE TITELLEISTE WIRD GEKLICKT
                           _15(mouse_y < wy+30) { 
                               
                               /// MINIMIEREN
                               _15(mouse_x > bx-70 AND mouse_x < bx-30) {
                                   _15((system_ticks - last_window_click) > 250) { 
                                       win->minimized=_128; last_window_click = system_ticks; 
                                   }
                               } 
                               /// FULLSCREEN
                               _41 _15(mouse_x > bx-20 AND mouse_x < bx+40) {
                                   _15((system_ticks - last_window_click) > 250) { 
                                       win->fullscreen = !win->fullscreen; last_window_click = system_ticks; 
                                   }
                               } 
                               /// SCHLIESSEN
                               _41 _15(mouse_x > bx+45 AND mouse_x < bx+70) {
                                   _15((system_ticks - last_window_click) > 250) { 
                                       win->open=_86; last_window_click = system_ticks; 
                                   }
                               }
                               /// DRAG & DROP (Greift sofort, wenn man keinen Button erwischt)
                               _41 { 
                                   drag_win = k; drag_off_x = mouse_x - wx; drag_off_y = mouse_y - wy; 
                               }
                           }
                           
                           /// FENSTERGRÖSSE ÄNDERN
                           _15(mouse_x > wx+ww-20 AND mouse_y > wy+wh-20) { resize_win = k; }
                           
                           /// WIN32 CHILD CONTROLS CLICK
                           int title_bar_c = 30;
                           for (int ci = 0; ci < win->num_controls; ci++) {
                               Control* c = &win->controls[ci];
                               if (c->type == 1 || c->type == 4) { // BUTTON or COMBOBOX
                                   int cx = wx + c->x;
                                   int cy = wy + title_bar_c + c->y;
                                   
                                   if (c->type == 4) {
                                       int box_h = 20;
                                       if (c->is_dropped) {
                                           int drop_h = c->num_items * 20;
                                           if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy + box_h && mouse_y <= cy + box_h + drop_h) {
                                               int clicked_item = (mouse_y - (cy + box_h)) / 20;
                                               if (clicked_item >= 0 && clicked_item < c->num_items) {
                                                   c->selected_item = clicked_item;
                                                   c->is_dropped = 0;
                                                   input_cooldown = 20;
                                                   // Send CBN_SELCHANGE via message queue
                                                   win->msg_msg = 0x0111; // WM_COMMAND
                                                   win->msg_wparam = (uint64_t)c->id | (1ULL << 16); // CBN_SELCHANGE
                                                   win->msg_lparam = 0x10000ULL | ((k & 0xFF) << 8) | (ci & 0xFF);
                                                   continue;
                                               }
                                           }
                                       }
                                       if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy && mouse_y <= cy + box_h) {
                                           c->is_dropped = !c->is_dropped;
                                           input_cooldown = 20;
                                           continue;
                                       }
                                   } else if (mouse_x >= cx && mouse_x <= cx + c->w && mouse_y >= cy && mouse_y <= cy + c->h) {
                                       // Button clicked - send via message queue
                                       win->msg_msg = 0x0111; // WM_COMMAND
                                       win->msg_wparam = (uint64_t)c->id; // high word is 0 for BN_CLICKED
                                       win->msg_lparam = 0x10000ULL | ((k & 0xFF) << 8) | (ci & 0xFF);
                                       input_cooldown = 15;
                                   }
                               }
                           }
                        }
                        mouse_handled = _128; _37; 
                    }
                }
            }
        }
        /// ==========================================
        /// 1. DER ABSOLUT SCHWARZE WELTRAUM
        /// ==========================================
        _39(_43 i = 0; i < screen_w*screen_h; i++) bb[i] = 0x000000;
		app_window_active = _86; // Setzt die 3D Engine in jedem Frame standardmaessig auf AUS (_86 = false)
        if (cfg_stars_on) {
            _39(_43 i=0; i<cfg_star_count; i++) {
                _43 t = stars[i].type;
                if (!cfg_galaxy_on && t >= 995) continue;
                
                _43 current_speed = stars[i].speed * cfg_speed;
                _15(t >= 995) stars[i].z -= 1 * cfg_speed; _41 _15(t >= 980) stars[i].z -= current_speed; _41 _15(t >= 950) stars[i].z -= (current_speed + 4*cfg_speed); _41 stars[i].z -= current_speed;
                _15(stars[i].z <= 0) { 
                    stars[i].z = 1000; stars[i].x = (cosmos_random() % 1599) - 799; stars[i].y = (cosmos_random() % 1199) - 599;
                    stars[i].type = cosmos_random() % 1000; stars[i].speed = (cosmos_random() % 10) + 8;
                }
                _43 sx = v_cx + (stars[i].x * 256) / stars[i].z; _43 sy = v_cy + (stars[i].y * 256) / stars[i].z;
                _15(sx >= 0 AND sx < screen_w AND sy >= 0 AND sy < screen_h) {
                    _15(t < 900) {
                        _43 intensity = 255 - (stars[i].z / 4); _15(intensity < 0) intensity = 0; _15(intensity > 255) intensity = 255;
                        _89 col = (intensity << 16) | (intensity << 8) | (intensity); Put(sx, sy, col);
                    } _41 _15(t < 950) {
                        _43 pulse = (Sin((frame * 5) + i) + 256) / 2; _15(pulse > 255) pulse = 255;
                        _89 col = (pulse << 16) | (pulse << 8) | 255; Put(sx, sy, col);
                    } _41 _15(t < 980) {
                        Put(sx, sy, 0xFFFFFF); 
                        _43 tail1_x = v_cx + (stars[i].x * 256) / (stars[i].z + 20); _43 tail1_y = v_cy + (stars[i].y * 256) / (stars[i].z + 20); Put(tail1_x, tail1_y, 0xFF8800); 
                    _43 tail2_x = v_cx + (stars[i].x * 256) / (stars[i].z + 40); _43 tail2_y = v_cy + (stars[i].y * 256) / (stars[i].z + 40); Put(tail2_x, tail2_y, 0xAA0000);
                } _41 _15(t < 995) {
                    _43 r = 3000 / stars[i].z; 
                    _15(r > 0 AND r < 40) {
                        _43 r2 = r*r; _43 focus_x = (sx - v_cx) * r / 400; _43 focus_y = (sy - v_cy) * r / 300;
                        _39(_43 cy_a=-r; cy_a<=r; cy_a++) _39(_43 cx_a=-r; cx_a<=r; cx_a++) _15(cx_a*cx_a+cy_a*cy_a <= r2) {
                            _43 pos_x = cx_a + r; _43 pos_y = cy_a + r; _43 noise = ((pos_x * 17 + pos_y * 31) % 40);
                            _43 lx = cx_a + focus_x; _43 ly = cy_a + focus_y; _43 l_dist = int_sqrt(lx*lx + ly*ly);
                            _43 diffuse = 30; _15(l_dist < r) diffuse += 225 - (l_dist * 225 / r);
                            _43 gray = ((50 + noise) * diffuse) / 256; _15(gray > 255) gray = 255;
                            Put(sx+cx_a, sy+cy_a, (gray<<16)|((gray*9)/10<<8)|((gray*8)/10)); 
                        }
                    }
                } _41 {
                    _43 r = 6000 / stars[i].z;
                    _15(r > 0 AND r < 60) {
                        _39(_43 j=0; j<30; j++) {
                            _43 ang1 = (j * 15 + (frame/3)) % 256; _43 dist = (j * r) / 30;
                            _43 gx1 = sx + (Cos(ang1)*dist)/84; _43 gy1 = sy + (Sin(ang1)*dist*3/4)/84; PutAlpha(gx1, gy1, 0xAA22AA); 
                            _43 ang2 = (ang1 + 128) % 256; _43 gx2 = sx + (Cos(ang2)*dist)/84; _43 gy2 = sy + (Sin(ang2)*dist*3/4)/84; PutAlpha(gx2, gy2, 0x2288AA); 
                        }
                        Put(sx, sy, 0xFFFFFF); 
                    }
                }
            }
        }
        }
        /// Zentrifuge & Sonne rendern
        _15(!z_blocked AND mouse_just_pressed AND !click_consumed AND is_over(mouse_x, mouse_y, v_cx, v_cy, 50)) {
            galaxy_open = !galaxy_open; click_consumed = _128;
        }
        _15(galaxy_open AND galaxy_expansion < 320) galaxy_expansion += 24;
        _15(!galaxy_open AND galaxy_expansion > 0) galaxy_expansion -= 30;
        
        /// ==========================================
        /// 2B. PLANETEN (HINTER DER SONNE)
        /// ==========================================
        _39(_43 i=0; i<6; i++) {
            Window* win = &windows[map_ids[i]];
            _15(galaxy_expansion > 10 OR win->minimized OR win->open) {
                _43 px = planets[i].cur_x; _43 py = planets[i].cur_y;
                _43 base_radius = 20;
                
                _44 hov = is_over(mouse_x, mouse_y, px, py, base_radius);
                _89 p_col = 0x888888;         
                _15(i EQ 0) p_col = 0xA05566; _15(i EQ 1) p_col = 0x44AA88; 
                _15(i EQ 2) p_col = 0x6677CC; _15(i EQ 3) p_col = 0xCC9955; 
                _15(i EQ 4) p_col = 0x8899AA;
                _15(hov AND !z_blocked) {
                    _43 hr = ((p_col >> 16) & 0xFF) + 40; _15(hr>255) hr=255;
                    _43 hg = ((p_col >> 8) & 0xFF) + 40;  _15(hg>255) hg=255;
                    _43 hb = (p_col & 0xFF) + 40;         _15(hb>255) hb=255;
                    
                    _15(i EQ 1 OR i EQ 3) { // Back half of Saturn ring
                        _43 rx = base_radius * 2 + 5; _43 ry = base_radius / 2 + 2;
                        for(int a=128; a<255; a++) {
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84, 0xAA7733);
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84 + 1, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84 + 1, 0x885522);
                        }
                    }
                    DrawOrganicPlanet(px, py, base_radius + 2, (hr<<16)|(hg<<8)|hb);
                    _15(i EQ 1 OR i EQ 3) { // Front half of Saturn ring
                        _43 rx = base_radius * 2 + 5; _43 ry = base_radius / 2 + 2;
                        for(int a=0; a<128; a++) {
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84, 0xEEAA55);
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84 + 1, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84 + 1, 0xCC8833);
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84 + 2, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84 + 2, 0xEEAA55);
                        }
                    }
                } _41 {
                    _15(i EQ 1 OR i EQ 3) { // Back half of Saturn ring
                        _43 rx = base_radius * 2 + 5; _43 ry = base_radius / 2 + 2;
                        for(int a=128; a<255; a++) {
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84, 0x885522);
                        }
                    }
                    DrawOrganicPlanet(px, py, base_radius, p_col);
                    _15(i EQ 1 OR i EQ 3) { // Front half of Saturn ring
                        _43 rx = base_radius * 2 + 5; _43 ry = base_radius / 2 + 2;
                        for(int a=0; a<128; a++) {
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84, 0xCC8833);
                            DrawLine(px + (Cos(a)*rx)/84, py + (Sin(a)*ry)/84 + 1, px + (Cos(a+1)*rx)/84, py + (Sin(a+1)*ry)/84 + 1, 0xAA6622);
                        }
                    }
                }
                _15(win->open) { 
                    DrawOrganicPlanet(px-30, py, 5, 0x8899AA); 
                    DrawOrganicPlanet(px+30, py, 5, 0x8899AA); 
                }
                TextC(px, py-4, planets[i].name, 0xFFFFFF, _128);
                _15(!z_blocked AND mouse_just_pressed AND !click_consumed AND hov) { 
                    if (i == 0) { system("mousepad &"); } // TXT
                    else if (i == 3) { system("pcmanfm &"); } // DSK
                    else if (i == 4) { system("xfce4-terminal &"); } // CMD
                    else if (i == 5) { system("xfce4-appfinder &"); } // DEB
                    else {
                        _15(win->minimized) win->minimized = _86; 
                        _41 { win->open = !_86; focus_window(win->id); }
                    }
                    click_consumed = _128;
                }
            }
        }
        
        DrawDenseGalaxy(v_cx, v_cy, galaxy_expansion);
        // ==========================================
        // 3D ORBIT-PHYSIK & Z-SORTIERUNG (ECHTES SYSTEM)
        // ==========================================
        
        // 1. Die Geschwindigkeiten (Je weiter draußen, desto langsamer!)
        float t_mer = frame * 0.041f;
        float t_ven = frame * 0.016f;
        float t_ear = frame * 0.010f;
        float t_mar = frame * 0.005f;
        float t_jup = frame * 0.001f;
        float t_sat = frame * 0.0005f;
        float t_ura = frame * 0.0002f;
        float t_nep = frame * 0.0001f;

        // 2. Die Umlaufbahnen (Leichte Neigungen für einen organischen 3D-Look)
        Vec3 p1_mer = { bare_cos(t_mer) * 140.0f,  5.0f * bare_sin(t_mer), bare_sin(t_mer) * 140.0f };
        Vec3 p2_ven = { bare_cos(t_ven) * 180.0f, -3.0f * bare_sin(t_ven), bare_sin(t_ven) * 180.0f };
        Vec3 p3_ear = { bare_cos(t_ear) * 230.0f,  0.0f,                   bare_sin(t_ear) * 230.0f };
        Vec3 p4_mar = { bare_cos(t_mar) * 280.0f,  8.0f * bare_sin(t_mar), bare_sin(t_mar) * 280.0f };
        Vec3 p5_jup = { bare_cos(t_jup) * 400.0f, -10.0f* bare_sin(t_jup), bare_sin(t_jup) * 400.0f };
        Vec3 p6_sat = { bare_cos(t_sat) * 520.0f,  15.0f* bare_sin(t_sat), bare_sin(t_sat) * 520.0f };
        Vec3 p7_ura = { bare_cos(t_ura) * 640.0f, -5.0f * bare_sin(t_ura), bare_sin(t_ura) * 640.0f };
        Vec3 p8_nep = { bare_cos(t_nep) * 750.0f,  2.0f * bare_sin(t_nep), bare_sin(t_nep) * 750.0f };

        // 3. Das Universum auflisten (9 Objekte!)
        RenderObj system[9] = {
            { 0, {0, 0, 0}, 80,  0 },                  // Sonne (Kocht!)
            { 1, p1_mer,     3,  0xAAAAAA },           // Merkur (Winzig, Grau)
            { 1, p2_ven,     7,  0xFFDD88 },           // Venus (Hellgelb)
            { 1, p3_ear,     8,  0x0088FF },           // Erde (Blau)
            { 1, p4_mar,     5,  0xFF4400 },           // Mars (Rostrot)
            { 1, p5_jup,    24,  0xDDAA77 },           // Jupiter (Gigantisch, Braun/Orange)
            { 1, p6_sat,    20,  0xEEDD99 },           // Saturn (Blassgelb)
            { 1, p7_ura,    12,  0x66CCFF },           // Uranus (Hellblau)
            { 1, p8_nep,    11,  0x2244AA }            // Neptun (Dunkelblau)
        };

        // ==========================================
        // 4. DIE INTERAKTIVE KAMERA (Rechte Maustaste)
        // ==========================================
        
        // Wir fragen einfach unsere neue globale Variable ab!
        bool right_mouse_held = mouse_right_down; 

        // Nur wenn die rechte Maustaste gedrückt ist, drehen wir das Universum!
        if (right_mouse_held) {
            global_cam_rot_y += ((int)mouse_x - last_mouse_x) * -0.005f; 
            global_cam_rot_x += ((int)mouse_y - last_mouse_y) * -0.005f; 
        }
        
        last_mouse_x = (int)mouse_x;
        last_mouse_y = (int)mouse_y;
        
        // Sinus/Kosinus aus den globalen Winkeln ziehen
        float sy = bare_sin(global_cam_rot_y); float cy = bare_cos(global_cam_rot_y);
        float sx = bare_sin(global_cam_rot_x); float cx = bare_cos(global_cam_rot_x);

        // Die 9 Objekte rotieren
        for (int i = 0; i < 9; i++) {
            Vec3 p = system[i].pos;
            float nx = p.x * cy - p.z * sy;
            float nz = p.x * sy + p.z * cy;
            p.x = nx; p.z = nz;

            float ny = p.y * cx - p.z * sx;
            nz = p.y * sx + p.z * cx;
            p.y = ny; p.z = nz;
            
            system[i].pos = p; 
        }

        // 5. BUBBLE SORT FÜR 9 OBJEKTE (Z-Tiefe)
        // Die innere Schleife geht bis 8 (j+1 erreicht maximal 8)
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 8; j++) {
                if (system[j].pos.z < system[j+1].pos.z) {
                    RenderObj temp = system[j];
                    system[j] = system[j+1];
                    system[j+1] = temp;
                }
            }
        }

        // 6. ZEICHNEN der sortierten Liste
        for (int i = 0; i < 9; i++) {
            if (system[i].type == 0) {
                Vec2 s_2d = Project3D(system[i].pos, v_cx, v_cy);
                if (s_2d.x != -1) {
                    DrawActiveSun(s_2d.x, s_2d.y, system[i].radius);
                }
            } else {
                if (cfg_planets_on) DrawPlanet3D(system[i].pos, system[i].radius, system[i].color, v_cx, v_cy);
            }
        }
		static uint64_t fps_last_time = 0;
		static int fps_frames = 0;
		static int fps_current = 0;
		fps_frames++;
		if (system_ticks - fps_last_time >= 1000) {
			fps_current = fps_frames;
			fps_frames = 0;
			fps_last_time = system_ticks;
		}

		// --- FPS ANZEIGE (Zentral ueber COSMOS) ---
		char fps_buffer[32];
		sprintf(fps_buffer, "FPS: %d", fps_current);
		TextC(screen_w/2, screen_h/2 - 40, fps_buffer, 0x000000, _128); 
		
        /// ==========================================
        /// LIVE RTC (DATUM UND UHRZEIT) ALS HUD ÜBER ALLEM
        /// ==========================================
        TextC(v_cx, v_cy-15, "COSMOS", 0x000000, _128);
        TextC(v_cx, v_cy+5,  "SYSTEM", 0x000000, _128);

#ifdef __linux__
        // Fragt den Linux-Kernel nach der echten Systemzeit
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        rtc_h = t->tm_hour;
        rtc_m = t->tm_min;
        rtc_day = t->tm_mday;
        rtc_mon = t->tm_mon + 1;       // Linux zählt Monate von 0-11, also +1
        rtc_year = t->tm_year + 1900;  // Linux zählt Jahre ab 1900, also +1900
#endif

        _30 ts[]="00:00"; 
        ts[0]='0'+rtc_h/10; ts[1]='0'+rtc_h%10; 
        ts[3]='0'+rtc_m/10; ts[4]='0'+rtc_m%10; 
        TextC(v_cx, v_cy+20, ts, 0x000000, _128);
        
        _30 ds[]="00.00.2000"; 
        ds[0]='0'+rtc_day/10; ds[1]='0'+rtc_day%10; 
        ds[3]='0'+rtc_mon/10; ds[4]='0'+rtc_mon%10; 
        ds[8]='0'+(rtc_year%100)/10; ds[9]='0'+rtc_year%10; 
        TextC(v_cx, v_cy+35, ds, 0x000000, _128);
        /// ==========================================
        /// 2A. PLANETEN UPDATE
        /// ==========================================
        _39(_43 i=0; i<6; i++) {
            Window* win = &windows[map_ids[i]];
            float target_x, target_y;
            _15(win->minimized) { 
                target_x = (screen_w / 2) - 140 + (i * 70); target_y = screen_h - 40; 
            } _41 _15(win->open) { 
                _43 orbit_dist = 120 + i*110; 
                target_x = v_cx + (bare_cos(planets[i].real_ang) * orbit_dist);
                target_y = v_cy + (bare_sin(planets[i].real_ang) * orbit_dist * 0.75f);
            } _41 {
                _15(galaxy_expansion >= 100) { 
                    _15(planets[i].dist < 120 + i*110) planets[i].dist += 6; 
                    _15(planets[i].dist > 50) { 
                         planets[i].real_ang += 0.001f;
                    } 
                } _41 { 
                    _15(planets[i].dist > 10) planets[i].dist -= 15; 
                }
                target_x = v_cx + (bare_cos(planets[i].real_ang) * planets[i].dist);
                target_y = v_cy + (bare_sin(planets[i].real_ang) * planets[i].dist * 0.75f);
            }
            planets[i].cur_x += (target_x - planets[i].cur_x) * 0.05f; 
            planets[i].cur_y += (target_y - planets[i].cur_y) * 0.05f;
        }
        /// ==========================================
        /// 3. ACRYLIC GLASS WINDOW RENDERING
        /// ==========================================
		/// BARE METAL FIX: Modal-Status berechnen, BEVOR die Fenster gezeichnet werden!
        /// Wenn Fenster ID 2 (Save As) offen und sichtbar ist, ist der Modus aktiv.
        _44 is_modal_blocked = (windows[2].open AND !windows[2].minimized);
        _39(_43 i=0; i<20; i++) {
            _43 k = win_z[i]; 
            Window* win = &windows[k];
            _15(!win->open OR win->minimized) continue;
            _43 wx=(win->fullscreen?0:win->x); 
            _43 wy=(win->fullscreen?0:win->y); 
            _43 ww=(win->fullscreen?screen_w:win->w); 
            _43 wh=(win->fullscreen?screen_h:win->h);
			/// =========================================================
            /// BARE METAL FIX: BLOCKED-STATUS FÜR ALLE FENSTER DEFINIEREN!
            /// =========================================================
            _44 blocked = (is_modal_blocked AND k NEQ 2);
            /// Milchglas-Hintergrund für offene Fenster zeichnen
            DrawGlassRect(wx, wy, ww, wh, 12, win->color);
            /// Rahmen & Highlights
            DrawRoundedRect(wx+12, wy, ww-24, 1, 0, 0x999999);
            DrawRoundedRect(wx, wy+12, 1, wh-24, 0, 0x999999);
            _89 txt_color = (win->color > 0x888888) ? 0x000000 : 0xFFFFFF;
            Text(wx+15, wy+15, win->title, txt_color, _128);
            /// Fenster-Buttons
            _43 bx = wx + ww/2; 
            Text(bx-60, wy+15, "MIN", 0x555555, _128);
            Text(bx-10, wy+15, "FULL", 0x555555, _128); 
            Text(bx+50, wy+15, "X", 0xFF0000, _128);
            /// =========================================================
            /// BARE METAL FIX: WIN32 PIXEL BUFFER (GDI)
            /// =========================================================
            if (win->pixels) {
                // Render pixel buffer leaving space for title bar (e.g. 30px)
                int title_bar = 30;
                for (int y = 0; y < wh - title_bar; y++) {
                    for (int x = 0; x < ww; x++) {
                        if (x < win->pixel_w && y < win->pixel_h) {
                            uint32_t col = win->pixels[y * win->pixel_w + x];
                            if (col != 0x000000) { // treat true black as transparent or just draw
                                Put(wx + x, wy + title_bar + y, col);
                            }
                        }
                    }
                }
            }
            
            // Draw Win32 Child Controls
            int title_bar = 30;
            for (int i = 0; i < win->num_controls; i++) {
                Control* c = &win->controls[i];
                int cx = wx + c->x;
                int cy = wy + title_bar + c->y;
                int cw = c->w;
                int ch = c->h;
                
                if (c->type == 1) { // BUTTON
                    bool hovered = is_over_rect(mouse_x, mouse_y, cx, cy, cw, ch);
                    DrawRoundedRect(cx, cy, cw, ch, 0, hovered ? 0x666666 : 0x555555); // Background
                    DrawRoundedRect(cx+1, cy+1, cw-2, ch-2, 0, 0x888888); // Border
                    // Center text horizontally and vertically
                    int tw = 0; while (c->text[tw]) tw++; tw *= 8;
                    int tx = cx + (cw - tw) / 2;
                    int ty = cy + (ch - 16) / 2;
                    Text(tx, ty, c->text, 0xFFFFFF, _128);
                } else if (c->type == 2) { // STATIC
                    Text(cx, cy + 2, c->text, 0xFFFFFF, _128);
                } else if (c->type == 3) { // PROGRESS
                    DrawRoundedRect(cx, cy, cw, ch, 0, 0x333333);
                    int pw = (cw * c->state) / 100;
                    if (pw > cw) pw = cw;
                    if (pw > 0) DrawRoundedRect(cx, cy, pw, ch, 0, 0x00FF00);
                } else if (c->type == 4) { // COMBOBOX
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
                }
            }
            
            // Draw Dropped Comboboxes on top
            for (int i = 0; i < win->num_controls; i++) {
                Control* c = &win->controls[i];
                if (c->type == 4 && c->is_dropped) {
                    int cx = wx + c->x;
                    int cy = wy + title_bar + c->y;
                    int cw = c->w;
                    int box_h = 20;
                    int drop_h = c->num_items * 20;
                    DrawRoundedRect(cx, cy + box_h, cw, drop_h, 0, 0x111111);
                    DrawRoundedRect(cx+1, cy + box_h + 1, cw-2, drop_h-2, 0, 0x444444);
                    for (int j = 0; j < c->num_items; j++) {
                        int item_y = cy + box_h + (j * 20);
                        Text(cx + 5, item_y + 2, c->items[j], 0xAAAAAA, _128);
                    }
                }
            }
            
            /// =========================================================
            /// APP: WEB BROWSER (ID 8)
            /// =========================================================
            _15(win->id EQ 8) {
                run_browser_engine(wx, wy, ww, wh, blocked);
            }
			/// =========================================================
            /// APP: IMAGE VIEWER (ID 9)
            /// =========================================================
            _15(win->id EQ 9) {
                _15(win->content[0] EQ 'B' AND win->content[1] EQ 'M' AND win->content[2] EQ 'P') {
                    _184* bmp = (_184*)0x0C000000;
                    _15(bmp[0] EQ 'B' AND bmp[1] EQ 'M') {
                        _43 pixel_offset = bmp[10] | (bmp[11]<<8) | (bmp[12]<<16) | (bmp[13]<<24);
                        _43 img_w = bmp[18] | (bmp[19]<<8) | (bmp[20]<<16) | (bmp[21]<<24);
                        _43 img_h = bmp[22] | (bmp[23]<<8) | (bmp[24]<<16) | (bmp[25]<<24);
                        _182 bpp = bmp[28] | (bmp[29]<<8);
                        
                        _15(bpp EQ 24 AND img_w > 0 AND img_h > 0 AND img_w < 1920 AND img_h < 1080) {
                            _43 row_padded = (img_w * 3 + 3) & (~3);
                            _43 start_x = wx + 10;
                            _43 start_y = wy + 40;
                            
                            _39(_43 y = 0; y < img_h; y++) {
                                _43 draw_y = start_y + (img_h - 1 - y);
                                _15(draw_y > wy + wh - 10) continue; 
                                
                                _43 row_offset = y * row_padded;
                                _39(_43 x = 0; x < img_w; x++) {
                                    _43 draw_x = start_x + x;
                                    _15(draw_x > wx + ww - 10) break;
                                    
                                    _43 p_idx = pixel_offset + row_offset + (x * 3);
                                    _89 r = bmp[p_idx + 2];
                                    _89 g = bmp[p_idx + 1];
                                    _89 b = bmp[p_idx];
                                    _89 col = (r << 16) | (g << 8) | b;
                                    Put(draw_x, draw_y, col);
                                }
                            }
                        } _41 {
                            TextC(wx + ww/2, wy + wh/2, "UNSUPPORTED BMP (24-BIT ONLY)", 0xCC4444, _128);
                        }
                    } _41 {
                        TextC(wx + ww/2, wy + wh/2, "INVALID BMP FILE", 0xCC4444, _128);
                    }
                }
            }
			
			/// =========================================================
            /// APP: PONG ARCADE (ID 11)
            /// =========================================================
            _15(win->id EQ 11) {
                run_pong_engine(wx, wy, ww, wh, blocked);
            }
            /// =========================================================
            /// APP: BLOBBY VOLLEY ARCADE (ID 12)
            /// =========================================================
            _15(win->id EQ 12) {
                run_blobby_engine(wx, wy, ww, wh, blocked);
            }
            /// =========================================================
            /// APP: SUPER SMASH CATS 3D (ID 14)
            /// =========================================================
            _15(win->id EQ 14) {
                os_app_x = wx; os_app_y = wy; os_app_w = ww; os_app_h = wh;
                app_window_active = _128; 
                
                // MUSS HIER STEHEN, SONST IST DAS FENSTER LEER!
                run_smash_cats_engine(wx, wy, ww, wh, blocked);
            }
            /// =========================================================
            /// APP: COSMOS DEFENDER (ID 10)
            /// =========================================================
            _15(win->id EQ 10) {
                run_holyspirit(wx, wy, ww, wh, blocked);
            }
            
			/// =========================================================
            /// APP: EMULATOR run_emulator_engine (ID 13)
            /// =========================================================
            _15(win->id EQ 6) {
                run_emulator_engine(wx, wy, ww, wh, blocked);
            }
			/// --- SYSTEM FENSTER ZEICHNEN (ID 3) ---
            _15(win->id EQ 3) {
                _43 mid = wx + ww/2;
                _43 btn_y = wy + 45;
                
                /// FIX: Nur Klicks zulassen, wenn das Fenster GANZ OBEN liegt!
                _44 is_active = (win_z[19] EQ win->id);
                
                /// 1. THEME & LANG TOGGLES
                _30 lang_lbl[20], theme_lbl[30];
                _15(sys_lang EQ 0) str_cpy(lang_lbl, "[ LANG: EN ]"); _41 str_cpy(lang_lbl, "[ SPR: DE ]");
                _15(sys_lang EQ 0) { _15(sys_theme EQ 0) str_cpy(theme_lbl, "[ THEME: COMPUTER ]"); _41 str_cpy(theme_lbl, "[ THEME: GENESIS ]"); } 
                _41 { _15(sys_theme EQ 0) str_cpy(theme_lbl, "[ THEMA: COMPUTER ]"); _41 str_cpy(theme_lbl, "[ THEMA: GENESIS ]"); }
                /// 2. MOUSE SENSITIVITY SLIDER (System: Klick-zum-Setzen)
                _43 slider_x = wx + 20;
                _43 slider_y = wy + 140; // Unter dem Trennstrich
                _43 slider_w = 150;
                
                // Hintergrund-Schiene
                DrawRoundedRect(slider_x, slider_y, slider_w, 8, 4, 0x444444);
                
                // Knob-Position (Berechnung basierend auf mouse_sens)
                // Range: 10 bis 300
                _43 knob_x = slider_x + ((mouse_sens - 10) * slider_w / 290);
                DrawRoundedRect(knob_x - 5, slider_y - 2, 10, 12, 4, 0xFFFFFF);
                
                // Label & Wert-Anzeige
                Text(wx + 180, slider_y - 2, "MOUSE SENSITIVITY:", 0xAAAAAA, _128);
                // Hier müsstest du ggf. noch einen Int-to-String Konverter nutzen, 
                // falls du die Zahl anzeigen willst.

                // Interaction: Klick-Abfrage für den Slider
                _44 mouse_klick_slider = (mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, slider_x, slider_y, slider_w, 20));
                
                _15(input_cooldown EQ 0 AND mouse_klick_slider) {
                    // Berechnung: Maus-X relativ zum Slider-Anfang
                    _43 rel_x = mouse_x - slider_x;
                    
                    // Umrechnung auf den Bereich 10 - 300
                    mouse_sens = (rel_x * 290 / slider_w) + 10;
                    
                    // Bounds Check
                    _15(mouse_sens < 10) mouse_sens = 10;
                    _15(mouse_sens > 300) mouse_sens = 300;
                    
                    input_cooldown = 10; // Kurzer Cooldown gegen "Flackern"
                }
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+5, btn_y, 140, 20)) { sys_lang = !sys_lang; input_cooldown = 25; }
                Text(wx+10, btn_y+4, lang_lbl, 0x000000, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+5, btn_y+30, 200, 20)) { sys_theme = !sys_theme; input_cooldown = 25; }
                Text(wx+10, btn_y+34, theme_lbl, 0x000000, _128);
                
                /// Leerer Bereich
                DrawRoundedRect(wx+20, wy+120, ww-40, 2, 0, 0xAAAAAA);
				
				#ifdef __linux__
				// ==========================================
                // FEATURE: UPDATE / INSTALL BUTTON
                // ==========================================
                int btn_inst_w = 180;
                int btn_inst_x = wx + ww - btn_inst_w - 20; 
                int btn_inst_y = wy + 40; 
                
                DrawRoundedRect(btn_inst_x, btn_inst_y, btn_inst_w, 25, 4, 0xAA0000);
                TextC(btn_inst_x + (btn_inst_w / 2), btn_inst_y + 8, "UPDATE / INSTALL", 0xFFFFFF, _86);
                
                if (input_cooldown == 0 && mouse_just_pressed && is_over_rect(mouse_x, mouse_y, btn_inst_x, btn_inst_y, btn_inst_w, 25)) {
                    
                    // WSL-FIX: '-hold' hält das Fenster bei Fehlern offen! 
                    // './install_cosmos.sh' sucht das Skript in deinem aktuellen lokalen Ordner!
                    ::system("xterm -hold -bg black -fg '#00FF00' -fa 'Monospace' -fs 11 -title 'CosmOS Installer' -e 'bash ./install_cosmos.sh' &");
                    
                    input_cooldown = 30;
                }
				#endif
                
                /// 2. CPU & USER INFO
                TextC(mid, wy+140, user_name, 0x222222, _128); 
                TextC(mid, wy+160, cpu_brand, 0x0000FF, _128);
                /// --- BARE METAL FIX: NETZWERK-DATEN ANZEIGEN ---
                _43 right_x = wx + ww - 160;
                /// Zeile 1: Deine Laptop IP (z.B. 192.168.14.100)
                Text(right_x, wy+140, ip_address, 0x222222, _128); 
                /// Zeile 2: Subnetzmaske
                Text(right_x, wy+155, net_mask, 0x555555, _86); 
                /// Zeile 3: Gateway (FritzBox)
                Text(right_x, wy+170, gateway_ip, 0x555555, _86);
                
                /// 3. HARDWARE STATUS
                TextC(mid, wy+200, "HARDWARE STATUS", 0x000000, _128);
                Text(wx+30, wy+230, "CORE:", 0x555555, _128); Text(wx+130, wy+230, "64-BIT PROTECTED MODE", 0x00AA00, _128);
                Text(wx+30, wy+250, "MEM:", 0x555555, _128); Text(wx+130, wy+250, "4 GB ADDRESS SPACE", 0x00AA00, _128);
                
                _30 l_disk[20] = "STORAGE:"; _30 l_net[20] = "NETWORK:"; _30 l_gpu[20] = "GRAPHIC:"; _30 l_usb[20] = "USB HOST:";
                _15(sys_lang NEQ 0) { str_cpy(l_disk, "FESTPLATTE:"); str_cpy(l_net, "NETZWERK:"); str_cpy(l_gpu, "GRAFIK:"); }
                
                /// --- DYNAMISCHE FARBEN FÜR TASTATUR-FOKUS ---
                uint32_t c_st_lbl = 0x555555, c_st_val = 0x0044CC;
                uint32_t c_nt_lbl = 0x555555, c_nt_val = 0x0044CC;
                uint32_t c_gp_lbl = 0x555555, c_gp_val = 0x0044CC;
                uint32_t c_us_lbl = 0x555555, c_us_val = 0x0044CC;
                uint32_t btn_color = 0x444444;

                /// BARE METAL FIX: Index verschoben, Button ist ganz oben!
                if (sys_selected_item == 0) { btn_color = 0xAA0000; }
                if (sys_selected_item == 1) { c_st_lbl = 0xCC4444; c_st_val = 0xCC4444; }
                if (sys_selected_item == 2) { c_nt_lbl = 0xCC4444; c_nt_val = 0xCC4444; }
                if (sys_selected_item == 3) { c_gp_lbl = 0xCC4444; c_gp_val = 0xCC4444; }
                if (sys_selected_item == 4) { c_us_lbl = 0xCC4444; c_us_val = 0xCC4444; }
				/// ==========================================
				/// DER GROSSE ORAKEL-BUTTON (GANZ OBEN)
				/// ==========================================
				_43 btn_scan_x = wx + 20;
				_43 btn_scan_y = wy + 150; /// Wieder nach oben geschoben!
				
				uint32_t btn_oracle_color = 0x444444; 
				/// Wenn er mit den Pfeiltasten angewählt ist (Index 0), leuchtet er Rot!
				if (sys_selected_item == 0) { btn_oracle_color = 0xAA0000; }
				
				DrawRoundedRect(btn_scan_x, btn_scan_y, 250, 30, 4, btn_oracle_color);
				Text(btn_scan_x + 10, btn_scan_y + 8, "OPEN 64-BIT ORACLE", 0xFFFFFF, _128);
				
				/// Klick-Abfrage für die MAUS
				_44 mouse_klick_oracle = (mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, btn_scan_x, btn_scan_y, 250, 30));
				
				_15(input_cooldown EQ 0 AND is_active AND mouse_klick_oracle) {
					if (sys_selected_item == 0) {
                        scan_pci_class(0x01, hw_storage, "CTRL");
                        scan_pci_class(0x02, hw_net, "NIC");
                        scan_pci_class(0x03, hw_gpu, "GPU");
                        system_init_usb();
                    } else if (sys_selected_item == 1) { scan_pci_class(0x01, hw_storage, "CTRL"); }
                    else if (sys_selected_item == 2) { scan_pci_class(0x02, hw_net, "NIC"); }
                    else if (sys_selected_item == 3) { scan_pci_class(0x03, hw_gpu, "GPU"); }
                    else if (sys_selected_item == 4) { system_init_usb(); }
					input_cooldown = 25;
				}

                /// KLICKBAR: STORAGE 
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+30, wy+270, 300, 20)) {
                    scan_pci_class(0x01, hw_storage, "CTRL");
                    input_cooldown = 25;
                }
                Text(wx+30, wy+275, l_disk, c_st_lbl, _128); Text(wx+130, wy+275, hw_storage, c_st_val, _128);
                
                /// KLICKBAR: NETWORK (Hardware Scan)
                /// Breite auf 180 reduziert, damit es nicht mit DHCP kollidiert!
                _44 net_hov = is_over_rect(mouse_x, mouse_y, wx+30, wy+295, 180, 20);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND net_hov) {
                    scan_pci_class(0x02, hw_net, "NIC");
                    input_cooldown = 25;
                }
                Text(wx+30, wy+300, l_net, c_nt_lbl, _128); 
                Text(wx+130, wy+300, hw_net, net_hov ? 0xFFFFFF : c_nt_val, _128);
                
                /// BARE METAL FIX: DHCP START BUTTON
                _44 dhcp_hov = is_over_rect(mouse_x, mouse_y, wx+ww-130, wy+295, 100, 20);
                DrawRoundedRect(wx+ww-130, wy+295, 100, 20, 3, dhcp_hov ? 0xFF8800 : 0xAA5500);
                TextC(wx+ww-80, wy+302, "DHCP REQ", 0xFFFFFF, _128);
                
                /// WLAN & BT BUTTONS
                _44 hov_wifi = is_over_rect(mouse_x, mouse_y, wx+ww-240, wy+295, 50, 20);
                DrawRoundedRect(wx+ww-240, wy+295, 50, 20, 3, hov_wifi ? 0x2288EE : 0x113355);
                TextC(wx+ww-215, wy+302, "WLAN", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND hov_wifi) {
#ifdef __linux__
                    if (fork() == 0) { execlp("xfce4-terminal", "xfce4-terminal", "--disable-server", "-e", "nmtui", NULL); exit(1); }
#endif
                    input_cooldown = 25;
                }

                _44 hov_bt = is_over_rect(mouse_x, mouse_y, wx+ww-185, wy+295, 50, 20);
                DrawRoundedRect(wx+ww-185, wy+295, 50, 20, 3, hov_bt ? 0x2288EE : 0x113355);
                TextC(wx+ww-160, wy+302, "BT", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND hov_bt) {
#ifdef __linux__
                    if (fork() == 0) { execlp("blueman-manager", "blueman-manager", NULL); exit(1); }
#endif
                    input_cooldown = 25;
                }

                /// NEU: Hier zeichnen wir den Status-String direkt unter den Button!
                TextC(wx+ww-80, wy+325, cmd_status, 0xCC4444, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND dhcp_hov) {
                    str_cpy(cmd_status, "BROADCASTING DHCP DISCOVER...");
                    _15(os2_net_bar0 EQ 0) os2_smart_scan();
                    net_check_link();
                    send_dhcp_discover();
                    input_cooldown = 25;
                }
                Text(wx+30, wy+300, l_net, c_nt_lbl, _128); Text(wx+130, wy+300, hw_net, c_nt_val, _128);
                
                /// KLICKBAR: GRAPHIC (NEUES UI)
                Text(wx+30, wy+325, l_gpu, c_gp_lbl, _128); 

                // PLANETS BUTTON
                _44 hov_pl = is_over_rect(mouse_x, mouse_y, wx+110, wy+320, 80, 20);
                DrawRoundedRect(wx+110, wy+320, 80, 20, 3, hov_pl ? 0x0066CC : 0x004488);
                TextC(wx+150, wy+327, cfg_planets_on ? "PLANETS ON" : "PLANETS OFF", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_pl) { cfg_planets_on = !cfg_planets_on; input_cooldown = 25; }

                // GALAXY BUTTON
                _44 hov_gx = is_over_rect(mouse_x, mouse_y, wx+195, wy+320, 75, 20);
                DrawRoundedRect(wx+195, wy+320, 75, 20, 3, hov_gx ? 0x0066CC : 0x004488);
                TextC(wx+232, wy+327, cfg_galaxy_on ? "GALAXY ON" : "GALAXY OFF", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_gx) { cfg_galaxy_on = !cfg_galaxy_on; input_cooldown = 25; }

                // STARS BUTTON
                _44 hov_st = is_over_rect(mouse_x, mouse_y, wx+275, wy+320, 70, 20);
                DrawRoundedRect(wx+275, wy+320, 70, 20, 3, hov_st ? 0x0066CC : 0x004488);
                char sbuf[16]; str_cpy(sbuf, "STARS "); 
                if (!cfg_stars_on) { str_cat(sbuf, "OFF"); } else { int_to_str(cfg_star_count, sbuf+6); }
                TextC(wx+310, wy+327, sbuf, 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_st) { 
                    if (!cfg_stars_on) { cfg_stars_on = _128; cfg_star_count = 50; }
                    else { cfg_star_count += 50; if (cfg_star_count > 200) cfg_stars_on = _86; }
                    input_cooldown = 25; 
                }

                // SPEED BUTTON
                _44 hov_sp = is_over_rect(mouse_x, mouse_y, wx+350, wy+320, 50, 20);
                DrawRoundedRect(wx+350, wy+320, 50, 20, 3, hov_sp ? 0x0066CC : 0x004488);
                char spbuf[16]; str_cpy(spbuf, "SPD "); int_to_str(cfg_speed, spbuf+4); str_cat(spbuf, "X");
                TextC(wx+375, wy+327, spbuf, 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_sp) { 
                    cfg_speed++; if (cfg_speed > 5) cfg_speed = 1; 
                    input_cooldown = 25; 
                }

                // NO MORE BALANCER BUTTON

                // RESET BUTTON
                _44 hov_rs = is_over_rect(mouse_x, mouse_y, wx+490, wy+320, 50, 20);
                DrawRoundedRect(wx+490, wy+320, 50, 20, 3, hov_rs ? 0xCC0000 : 0x880000);
                TextC(wx+515, wy+327, "RESET", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_rs) { 
                    cfg_planets_on = _128; cfg_galaxy_on = _128; cfg_stars_on = _128;
                    cfg_star_count = 200; cfg_speed = 1; 
                    input_cooldown = 25; 
                }
                
                /// KLICKBAR: USB (DAS IST JETZT DIE ZÜNDUNG!)
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+30, wy+345, 300, 20)) {
                    system_init_usb(); 
                    input_cooldown = 25;
                }
                Text(wx+30, wy+350, l_usb, c_us_lbl, _128); Text(wx+130, wy+350, hw_usb, c_us_val, _128);
                
                /// KLICKBAR: SOUND MUTE & TEST
                
                /// Mute Button
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+30, wy+370, 100, 20)) {
                    sound_muted = !sound_muted;
                    _15(sound_muted) play_freq(0); /// Sofort stumm!
                    input_cooldown = 25;
                }
                Text(wx+30, wy+375, sound_muted ? "SOUND: MUTED" : "SOUND: ON", sound_muted ? 0xCC4444 : 0xAAAAAA, _128);
                
                /// Test Button
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+150, wy+370, 100, 20)) {
                    play_sound(1200, 10);
                    input_cooldown = 25;
                }
                DrawRoundedRect(wx+145, wy+370, 90, 18, 3, 0x555555);
                Text(wx+150, wy+375, "TEST BEEP", 0xFFFFFF, _128);
                
                /// Surreal Button
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+250, wy+370, 110, 20)) {
                    extern bool cfg_sound_surreal;
                    cfg_sound_surreal = !cfg_sound_surreal;
                    input_cooldown = 25;
                }
                DrawRoundedRect(wx+245, wy+370, 110, 18, 3, cfg_sound_surreal ? 0x00AA00 : 0xAA0000);
                Text(wx+250, wy+375, cfg_sound_surreal ? "SURREAL: ON " : "SURREAL: OFF", 0xFFFFFF, _128);

                // RESOLUTION BUTTONS
                _44 hov_res1 = is_over_rect(mouse_x, mouse_y, wx+365, wy+370, 50, 18);
                DrawRoundedRect(wx+365, wy+370, 50, 18, 3, hov_res1 ? 0x0066CC : 0x004488);
                TextC(wx+390, wy+375, "720p", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_res1) { 
                    if (global_boot_info) {
                        global_boot_info->screen_width = 1280; global_boot_info->screen_height = 720; global_boot_info->framebuffer_pitch = 1280 * 4;
                        screen_w = 1280; screen_h = 720; screen_pitch = 1280 * 4;
                        v_cx = screen_w / 2; v_cy = screen_h / 2;
                        for(int _i=0; _i<20; _i++) if(windows[_i].fullscreen) { windows[_i].w = screen_w; windows[_i].h = screen_h; }
                    }
                    input_cooldown = 25; 
                }

                _44 hov_res2 = is_over_rect(mouse_x, mouse_y, wx+425, wy+370, 60, 18);
                DrawRoundedRect(wx+425, wy+370, 60, 18, 3, hov_res2 ? 0x0066CC : 0x004488);
                TextC(wx+455, wy+375, "1080p", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_res2) { 
                    if (global_boot_info) {
                        global_boot_info->screen_width = 1920; global_boot_info->screen_height = 1080; global_boot_info->framebuffer_pitch = 1920 * 4;
                        screen_w = 1920; screen_h = 1080; screen_pitch = 1920 * 4;
                        v_cx = screen_w / 2; v_cy = screen_h / 2;
                        for(int _i=0; _i<20; _i++) if(windows[_i].fullscreen) { windows[_i].w = screen_w; windows[_i].h = screen_h; }
                    }
                    input_cooldown = 25; 
                }
                
                _44 hov_res3 = is_over_rect(mouse_x, mouse_y, wx+495, wy+370, 50, 18);
                DrawRoundedRect(wx+495, wy+370, 50, 18, 3, hov_res3 ? 0x0066CC : 0x004488);
                TextC(wx+520, wy+375, "SVGA", 0xFFFFFF, _86);
                _15(input_cooldown == 0 && mouse_just_pressed && is_active && hov_res3) { 
                    if (global_boot_info) {
                        global_boot_info->screen_width = 800; global_boot_info->screen_height = 600; global_boot_info->framebuffer_pitch = 800 * 4;
                        screen_w = 800; screen_h = 600; screen_pitch = 800 * 4;
                        v_cx = screen_w / 2; v_cy = screen_h / 2;
                        for(int _i=0; _i<20; _i++) if(windows[_i].fullscreen) { windows[_i].w = screen_w; windows[_i].h = screen_h; }
                    }
                    input_cooldown = 25; 
                }
                
                /// HDA DEBUG INFO (zeigt was der Treiber wirklich findet)
                extern uint32_t hda_base_addr;
                extern uint32_t hda_dac_nid;
                extern uint32_t hda_pin_nid;
                extern uint32_t hda_output_stream_offset;
                extern uint32_t hda_codec_id;
                extern uint32_t pci_dev_count;
                
                char hda_dbg[64];
                _15(hda_base_addr == 0) {
                    str_cpy(hda_dbg, "HDA: NOT FOUND (SCANNED: ");
                    char tmp[8]; int_to_str(pci_dev_count, tmp);
                    str_cat(hda_dbg, tmp);
                    str_cat(hda_dbg, " DEV)");
                } _41 _15(hda_dac_nid == 0) {
                    str_cpy(hda_dbg, "HDA: NO CODEC! C0=");
                    char tmp[12]; hex_to_str(hda_debug_c0_resp, tmp);
                    str_cat(hda_dbg, tmp);
                } _41 {
                    /// Zeige: BASE | CODEC | DAC | PIN
                    str_cpy(hda_dbg, "HDA:OK C=");
                    char tmp[8]; int_to_str(hda_codec_id, tmp);
                    str_cat(hda_dbg, tmp);
                    str_cat(hda_dbg, " D=");
                    int_to_str(hda_dac_nid, tmp);
                    str_cat(hda_dbg, tmp);
                    str_cat(hda_dbg, " P=");
                    int_to_str(hda_pin_nid, tmp);
                    str_cat(hda_dbg, tmp);
                }
                Text(wx+30, wy+395, hda_dbg, hda_dac_nid ? 0xAAAAAA : 0xFF4400, _86);
				/// ==========================================
                /// 5. BARE METAL TASK MANAGER (LIVE-ANZEIGE)
                /// ==========================================
                TextC(mid, wy+405, "LIVE TASK SCHEDULER", 0x000000, _128);
                
                _43 task_y = wy + 425;
                _39(_43 t = 0; t < 4; t++) {
                    _15(tasks[t].active) {
                        char s_id[5]; int_to_str(t, s_id);
                        char* t_name = (char*)((t == 0) ? "COSMOS KERNEL" : "BACKGROUND TASK");
                        
                        /// Ist dieser Task genau in diesem Frame aktiv auf der CPU? -> GRÜN!
                        _89 c_box = (current_task == t) ? 0x00AA00 : (tasks[t].paused ? 0xAA8800 : 0x555555); 
                        
                        DrawRoundedRect(wx+30, task_y, 290, 20, 3, c_box);
                        Text(wx+40, task_y+4, "TASK", 0xFFFFFF, _128);
                        Text(wx+80, task_y+4, s_id, 0xFFFFFF, _128);
                        Text(wx+110, task_y+4, t_name, 0xFFFFFF, _128);
                        
                        _15(t > 0) {
                            /// Pause Button
                            DrawRoundedRect(wx+250, task_y+2, 26, 16, 2, tasks[t].paused ? 0x00AA00 : 0x222222);
                            Text(wx+254, task_y+4, "||", 0xFFFFFF, _86);
                            _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+250, task_y+2, 26, 16)) {
                                tasks[t].paused = !tasks[t].paused;
                                if (tasks[t].paused) play_freq(0);
                                input_cooldown = 20;
                            }
                            
                            /// Kill Button
                            DrawRoundedRect(wx+280, task_y+2, 26, 16, 2, 0xAA0000);
                            Text(wx+288, task_y+4, "X", 0xFFFFFF, _86);
                            _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+280, task_y+2, 26, 16)) {
                                tasks[t].active = _86;
                                tasks[t].paused = _86;
                                play_freq(0);
                                input_cooldown = 20;
                            }
                        }
                        
                        task_y += 25;
                    }
                }
                
                /// 4. POWER BUTTONS
                DrawRoundedRect(wx+30, wy+wh-50, 120, 30, 4, 0xAA0000); TextC(wx+90, wy+wh-40, "REBOOT", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+30, wy+wh-50, 120, 30)) { system_reboot(); }
                DrawRoundedRect(wx+ww-150, wy+wh-50, 120, 30, 4, 0x000000); TextC(wx+ww-90, wy+wh-40, "SHUT DOWN", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-150, wy+wh-50, 120, 30)) { system_shutdown(); }
            }
			/// =========================================================
            /// APP LAUNCHER (ID 1)
            /// =========================================================
            _15(win->id EQ 1) { 
                TextC(wx+ww/2, wy+40, "INSTALLED APPS", 0x222222, _128);

                // --- DAS RASTER-SYSTEM (Einmal definieren, überall nutzen!) ---
                int bw = 150; // Button Breite (kleiner & eleganter)
                int bh = 35;  // Button Höhe
                
                // Spalten (X-Koordinaten)
                int col1 = wx + 20;
                int col2 = wx + 180;
                int col3 = wx + 340;
                
                // Zeilen (Y-Koordinaten)
                int row1 = wy + 70;
                int row2 = wy + 115;
                int row3 = wy + 160;

                // ==========================================
                // ZEILE 1 (Deine Spiele)
                // ==========================================
                DrawRoundedRect(col1, row1, bw, bh, 4, 0x3388FF); 
                TextC(col1+bw/2, row1+10, "COSMOS DEFENDER", 0xFFFFFF, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col1, row1, bw, bh)) {
                    windows[10].open = _128; windows[10].minimized = _86; str_cpy(windows[10].title, "COSMOS DEFENDER"); focus_window(10);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                DrawRoundedRect(col2, row1, bw, bh, 4, 0x3388FF); 
                TextC(col2+bw/2, row1+10, "PLAY PONG", 0xFFFFFF, _128); 
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col2, row1, bw, bh)) {
                    windows[11].open = _128; windows[11].minimized = _86; str_cpy(windows[11].title, "PONG"); focus_window(11);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                DrawRoundedRect(col3, row1, bw, bh, 4, 0x3388FF); 
                TextC(col3+bw/2, row1+10, "PLAY BLOBBY", 0xFFFFFF, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col3, row1, bw, bh)) {
                    windows[12].open = _128; windows[12].minimized = _86; str_cpy(windows[12].title, "BLOBBY"); focus_window(12);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                // ==========================================
                // ZEILE 2 (Emulator & Defender)
                // ==========================================
                DrawRoundedRect(col1, row2, bw, bh, 4, 0x8800CC); 
                TextC(col1+bw/2, row2+10, "EMULATOR", 0xFFFFFF, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col1, row2, bw, bh)) {
                    windows[13].open = _128; windows[13].minimized = _86; str_cpy(windows[13].title, "EMULATOR"); focus_window(13);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                DrawRoundedRect(col2, row2, bw, bh, 4, 0x8800CC); 
                TextC(col2+bw/2, row2+10, "SMASH CATS 3D", 0xFFFFFF, _128);
                
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col2, row2, bw, bh)) {
                    windows[14].open = _128; windows[14].minimized = _86; str_cpy(windows[14].title, "SMASH CATS 3D"); focus_window(14);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                DrawRoundedRect(col3, row2, bw, bh, 4, 0xAA5500);
                TextC(col3+bw/2, row2+10, "BARE METAL DSK", 0xFFFFFF, _128);

                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col3, row2, bw, bh)) {
                    windows[4].open = _128; windows[4].minimized = _86; focus_window(4);
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86;
                }

                // ==========================================
                // ZEILE 3 (Die neuen Linux-Workstation Apps!)
                // ==========================================
                
                /// --- WEB EXPLORER (Col 1, Row 3) ---
                DrawRoundedRect(col1, row3, bw, bh, 4, 0x334455); 
                TextC(col1+bw/2, row3+10, "WEB EXPLORER", 0xFFFFFF, _128); 
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col1, row3, bw, bh)) { 
#ifdef __linux__
                    // 1. Spalte einen neuen Prozess vom Hauptsystem ab
                    pid_t pid = fork(); 
                    if (pid == 0) {
                        // Das hier ist der "Kind-Prozess"
                        execlp("firefox", "firefox", NULL);
                        exit(1); 
                    }
#endif
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86; 
                }

                /// --- TERMINAL (Col 2, Row 3) ---
                DrawRoundedRect(col2, row3, bw, bh, 4, 0x334455);
                TextC(col2+bw/2, row3+10, "CMD PROMPT", 0xFFFFFF, _128); 
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col2, row3, bw, bh)) { 
#ifdef __linux__
                    if (fork() == 0) { execlp("xfce4-terminal", "xfce4-terminal", "--disable-server", NULL); exit(1); }
#endif
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86; 
                }

                /// --- MEDIA PLAYER (Col 3, Row 3) ---
                DrawRoundedRect(col3, row3, bw, bh, 4, 0x334455);
                TextC(col3+bw/2, row3+10, "MEDIA PLAYER", 0xFFFFFF, _128); 
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col3, row3, bw, bh)) { 
#ifdef __linux__
                    if (fork() == 0) { execlp("mpv", "mpv", "--player-operation-mode=pseudo-gui", NULL); exit(1); }
#endif
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86; 
                }
                int row4 = row3 + 60;
                /// --- KI ANTIGRAVITY (Col 3, Row 4) ---
                DrawRoundedRect(col3, row4, bw, bh, 4, 0x6600FF); 
                TextC(col3+bw/2, row4+10, "KI ANTIGRAVITY", 0xFFFFFF, _128); 
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND !blocked AND is_over_rect(mouse_x, mouse_y, col3, row4, bw, bh)) { 
#ifdef __linux__
                    if (fork() == 0) {
                        chdir("/opt/meinos/Antigravity IDE");
                        execl("./antigravity-ide", "antigravity-ide", "--no-sandbox", "--disable-gpu", "--disable-dev-shm-usage", NULL);
                        exit(1);
                    }
#endif
                    input_cooldown = 25; windows[1].open = _86; windows[2].open = _86; 
                }

            }
			
			/// ==========================================
            /// DISK MANAGER (FENSTER ID 4) - ULTIMATE HDD FIX + LOG VIEW
            /// ==========================================
            _15(win->id EQ 4) {
                /// ONE-SHOT SCANNER
                static _44 drives_scanned = _86;
                _15(!drives_scanned) {
                    drives_scanned = _128;
                    drive_count = 0;
                    ahci_mount_drive();
                    
                    if (cartridge_start > 0) {
                        drives[drive_count].type = 5;
                        drives[drive_count].size_mb = (cartridge_end - cartridge_start) / (1024 * 1024);
                        if (drives[drive_count].size_mb == 0) drives[drive_count].size_mb = 1;
                        drives[drive_count].base_port = 0;
                        str_cpy(drives[drive_count].model, "OS CARTRIDGE");
                        drive_count++;
                    }
                    ahci_read_mbr();
                }
                
                static _44 usb_scanned = _86;
                _44 is_active = (win_z[19] EQ win->id);
                txt_color = (win->color > 0x888888) ? 0x000000 : 0xFFFFFF;
                uint64_t buf_mbr = (uint64_t)global_buf_mbr;
                uint64_t buf_dir = (uint64_t)global_buf_dir;
                static _44 is_ntfs_drive = _86;
                static _44 is_fat32_drive = _86;
                static _44 need_ui_refresh = _86;
                static int current_page_offset = 0;
                static uint32_t active_ntfs_folder_lba = 5; 
                static uint64_t global_ntfs_lba = 0;
                static uint32_t global_ntfs_spc = 8;
                static uint32_t active_fat32_folder_lba = 0;
                static _44 clipboard_active = _86;
                static char clipboard_name[12] = {0};
                static _44 clipboard_is_folder = _86;
                static uint32_t clipboard_size = 0; 
                
                /// BARE METAL FIX: GLOBALE MEMORY-CACHES UM OOM-CRASHES ZU VERHINDERN!
                static uint8_t* global_mft_cache = 0;
                // FIX: 60 MB statt 10 MB, damit die 50.000 NTFS Records reinpassen!
                if (!global_mft_cache) global_mft_cache = new uint8_t[60*1024*1024]; 
                
                static uint8_t* tba_comp_buffer = 0;
                static uint8_t* tba_decomp_buffer = 0;
                if (!tba_comp_buffer) tba_comp_buffer = new uint8_t[10*1024*1024];
                if (!tba_decomp_buffer) tba_decomp_buffer = new uint8_t[10*1024*1024];
                
                /// ------------------------------------------
                /// VIEW 2: GEÖFFNETES LAUFWERK
                /// ------------------------------------------
                _15(dsk_mgr_opened) {
                    DrawRoundedRect(wx+15, wy+45, 180, 55, 4, 0x222222); Text(wx+25, wy+50, "DRIVE CAPACITY:", 0xAAAAAA, _128);
                    char s_cap[10]; int_to_str(drive_total_gb, s_cap); char s_kb[10]; int_to_str(drive_used_kb, s_kb);
                    char* cap_lbl = (char*)((selected_drive_idx == 99) ? "MB TOTAL" : "GB TOTAL");
                    Text(wx+25, wy+65, s_cap, 0xAAAAAA, _128); Text(wx+55, wy+65, cap_lbl, 0xAAAAAA, _128);
                    Text(wx+25, wy+80, s_kb, 0xFF8800, _128); Text(wx+55, wy+80, "KB USED", 0xFF8800, _128);
                    
                    _15(!is_ntfs_drive AND !is_fat32_drive) {
                        /// "+ FILE" Button
                        DrawRoundedRect(wx+280, wy+45, 80, 25, 4, 0xAA5500); TextC(wx+320, wy+53, "+ FILE", 0xFFFFFF, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+280, wy+45, 80, 25)) {
                            _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                            _15(selected_drive_idx != 99) {
                                active_sata_port = drives[selected_drive_idx].base_port;
                                ahci_read_sectors(active_sata_port, 1002, 1, (uint64_t)buf_dir);
                                _39(_192 _43 wait = 0; wait < 1000000; wait++) ;
                                CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                                _39(int i=0; i<28; i++) {
                                    _15(dir[i].type == 0) {
                                        dir[i].type = 1; dir[i].file_size = 5120; dir[i].start_lba = 10000;
                                        _39(int n=0; n<11; n++) { dir[i].filename[n] = 0; cfs_files[i].name[n] = 0; }
                                        str_cpy(dir[i].filename, "APP.BIN"); 
                                        str_cpy(cfs_files[i].name, "APP.BIN");
                                        cfs_files[i].start_lba = 10000;
                                        cfs_files[i].size = 5120;
                                        cfs_files[i].is_folder = _86;
                                        
                                        ahci_write_sectors(active_sata_port, 1002, 1, (uint64_t)buf_dir);
                                        _39(_192 _43 wait2 = 0; wait2 < 1000000; wait2++) ;
                                        cfs_files[i].exists = 1;
                                        _37;
                                    }
                                }
                            }
                            input_cooldown = 15;
                        }
                        
                        /// "+ FOLD" Button
                        DrawRoundedRect(wx+280, wy+75, 80, 25, 4, 0xAA00AA); TextC(wx+320, wy+83, "+ FOLD", 0xFFFFFF, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+280, wy+75, 80, 25)) {
                            _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                            _15(selected_drive_idx != 99) {
                                active_sata_port = drives[selected_drive_idx].base_port;
                                ahci_read_sectors(active_sata_port, 1002, 1, (uint64_t)buf_dir);
                                _39(_192 _43 wait = 0; wait < 1000000; wait++) ;
                                CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                                _39(int i=0; i<28; i++) {
                                    _15(dir[i].type == 0) {
                                        dir[i].type = 2; dir[i].file_size = 0; dir[i].start_lba = 0;
                                        dir[i].parent_idx = current_folder_id;
                                        _39(int n=0; n<11; n++) { dir[i].filename[n] = 0; cfs_files[i].name[n] = 0; }
                                        str_cpy(dir[i].filename, "roms"); 
                                        str_cpy(cfs_files[i].name, "roms");
                                        cfs_files[i].start_lba = 0;
                                        cfs_files[i].size = 0;
                                        cfs_files[i].is_folder = _128;
                                        cfs_files[i].parent_idx = current_folder_id;
                                        
                                        ahci_write_sectors(active_sata_port, 1002, 1, (uint64_t)buf_dir);
                                        _39(_192 _43 wait2 = 0; wait2 < 1000000; wait2++) ;
                                        cfs_files[i].exists = 1;
                                        _37;
                                    }
                                }
                            }
                            input_cooldown = 15;
                        }
                    }
                    
                    /// PASTE BUTTON (ZWISCHENABLAGE EINFÜGEN)
                    _15(clipboard_active) {
                        DrawRoundedRect(wx+370, wy+45, 80, 25, 4, 0x00AA55); 
                        TextC(wx+410, wy+53, "PASTE", 0xFFFFFF, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+370, wy+45, 80, 25)) {
                            _15(!is_ntfs_drive AND !is_fat32_drive) {
                                uint64_t tmp_dir = (uint64_t)global_tmp_dir;
                                _39(int k = 0; k < 512; k++) ((char*)tmp_dir)[k] = 0;
                                
                                _43 target_port = drives[selected_drive_idx].base_port;
                                ahci_read_sectors(target_port, 1002, 1, tmp_dir);
                                _39(_192 _43 wait = 0; wait < 100000; wait++) ;
                                
                                CFS_DIR_ENTRY* entries = (CFS_DIR_ENTRY*)tmp_dir;
                                int free_slot = -1;
                                _39(int s=0; s<28; s++) { if(entries[s].type == 0) { free_slot = s; break; } }
                                
                                _15(free_slot != -1) {
                                    
                                    _15(!clipboard_is_folder) {
                                        uint32_t target_sec = 11000;
                                        for (int i = 0; i < 28; i++) {
                                            if (cfs_files[i].exists && cfs_files[i].start_lba >= target_sec) {
                                                target_sec = cfs_files[i].start_lba + ((cfs_files[i].size + 511) / 512) + 1;
                                            }
                                        }
                                        uint32_t target_lba = target_sec;
                                        
                                        uint32_t copy_sec = (clipboard_size + 511) / 512;
                                        if (copy_sec == 0) copy_sec = 10;
                                        
                                        uint64_t copy_ram_buffer = get_ram_addr_copy();
                                        _39(uint32_t s=0; s<copy_sec; s++) {
                                            ahci_write_sectors(target_port, target_lba + s, 1, copy_ram_buffer + (s * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
                                        entries[free_slot].type = 1;
                                        entries[free_slot].file_size = clipboard_size;
                                        entries[free_slot].start_lba = target_lba;
                                    } _41 {
                                        entries[free_slot].type = 2; 
                                        entries[free_slot].file_size = 0; 
                                        entries[free_slot].start_lba = 0;
                                    }
                                    
                                    entries[free_slot].parent_idx = current_folder_id; 
                                    _39(int n=0; n<11; n++) entries[free_slot].filename[n] = 0;
                                    str_cpy(entries[free_slot].filename, clipboard_name);
                                    
                                    ahci_write_sectors(target_port, 1002, 1, tmp_dir);
                                    _39(_192 _43 wait = 0; wait < 200000; wait++) ;
                                    
                                    print_win(win, "\n[OK] PASTED FROM CLIPBOARD.\n");
                                    need_ui_refresh = _128;
                                    clipboard_active = _86;
                                } _41 {
                                    print_win(win, "\n[ERR] FOLDER IS FULL.\n");
                                }
                            } _41 {
                                print_win(win, "\n[ERR] PASTE ONLY ON CFS NOW.\n");
                            }
                            input_cooldown = 25;
                        }
                    }
                    
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
                    
                    DrawRoundedRect(wx+210, wy+45, 60, 25, 4, 0x444444); TextC(wx+240, wy+53, "BACK", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+210, wy+45, 60, 25)) {
                        dsk_mgr_opened = _86; input_cooldown = 15; 
                    }
                    
                    /// ==========================================
                    /// ON-DEMAND RAM SCRAPER (NO MORE OUT-OF-MEMORY CRASHES!)
                    /// ==========================================
                    _15(is_ntfs_drive AND need_ui_refresh) {
                        _43 target_mft_id = (current_folder_id EQ 255) ? 5 : active_ntfs_folder_lba;
                        _39(int f=0; f<28; f++) { cfs_files[f].exists = 0; cfs_files[f].name[0] = 0; } 
                        int file_idx = 0; int skipped = 0;
                        uint8_t* mft_cache = global_mft_cache; /// FIX: Nutzt jetzt die echten Daten!
                        
                        _39(int rec = 25; rec < 25000; rec++) {
                            if (file_idx >= 28) break; 
                            uint8_t* mft_rec = mft_cache + (rec * 1024); 
                            
                            if (mft_rec[0] == 'F' && mft_rec[1] == 'I' && mft_rec[2] == 'L' && mft_rec[3] == 'E' && (*(uint16_t*)&mft_rec[22] & 0x01)) {
                                int attr_pos = *(uint16_t*)&mft_rec[20];
                                
                                cfs_files[file_idx].exists = 0;
                                cfs_files[file_idx].start_lba = rec; 
                                uint32_t found_data_lba = 0;
                                bool skip_this_file = false;

                                _114(attr_pos > 0 && attr_pos < 1000) { 
                                    uint32_t attr_type = *(uint32_t*)&mft_rec[attr_pos];
                                    uint32_t attr_len = *(uint32_t*)&mft_rec[attr_pos + 4];
                                    if (attr_type == 0xFFFFFFFF || attr_len <= 0) break; 
                                    
                                    if (attr_type == 0x30 && mft_rec[attr_pos + 8] == 0) {
                                        int fn_base = attr_pos + *(uint16_t*)&mft_rec[attr_pos + 20];
                                        if (fn_base < 0 || fn_base + 80 >= 1024 || *(uint32_t*)&mft_rec[fn_base + 0] != target_mft_id || mft_rec[fn_base + 65] == 2) { attr_pos += attr_len; continue; } 

                                        if (skipped < current_page_offset) { skipped++; skip_this_file = true; break; }

                                        uint8_t name_len = mft_rec[fn_base + 64];
                                        cfs_files[file_idx].exists = 1; cfs_files[file_idx].parent_idx = current_folder_id; 
                                        cfs_files[file_idx].is_folder = (*(uint16_t*)&mft_rec[22] & 0x02) ? 1 : 0;
                                        cfs_files[file_idx].size = *(uint32_t*)&mft_rec[fn_base + 48];

                                        int chars_to_copy = (name_len > 60) ? 60 : name_len;
                                        _39(int c=0; c<chars_to_copy; c++) {
                                            char ch = mft_rec[fn_base + 66 + (c * 2)];
                                            if (ch >= 'a' && ch <= 'z') ch -= 32;
                                            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-' || ch == ' ') { cfs_files[file_idx].name[c] = ch; } 
                                            else { cfs_files[file_idx].name[c] = '_'; }
                                        }
                                        cfs_files[file_idx].name[chars_to_copy] = 0;
                                    } else if (attr_type == 0x80 && mft_rec[attr_pos + 8] == 1) { 
                                        int run_off = *(uint16_t*)&mft_rec[attr_pos + 32];
                                        uint8_t* run = &mft_rec[attr_pos + run_off];
                                        uint8_t header = run[0];
                                        if (header != 0) {
                                            int len_bytes = header & 0x0F;
                                            int offset_bytes = (header >> 4) & 0x0F;
                                            int64_t lcn_offset = 0;
                                            for (int k = 0; k < offset_bytes; k++) lcn_offset |= ((int64_t)run[1 + len_bytes + k]) << (k * 8);
                                            if (offset_bytes > 0 && (run[1 + len_bytes + offset_bytes - 1] & 0x80)) {
                                                for (int k = offset_bytes; k < 8; k++) lcn_offset |= ((int64_t)0xFF) << (k * 8);
                                            }
                                            found_data_lba = global_ntfs_lba + (lcn_offset * global_ntfs_spc);
                                        }
                                    }
                                    attr_pos += attr_len;
                                }
                                if (!skip_this_file && cfs_files[file_idx].exists) {
                                    if (!cfs_files[file_idx].is_folder && found_data_lba != 0) {
                                        cfs_files[file_idx].start_lba = found_data_lba;
                                    } else {
                                        cfs_files[file_idx].start_lba = rec;
                                    }
                                    file_idx++;
                                } else {
                                    cfs_files[file_idx].exists = 0;
                                }
                            }
                        }
                        need_ui_refresh = _86; 
                    }

                    _15(is_fat32_drive AND need_ui_refresh) {
                        _39(int f=0; f<28; f++) { cfs_files[f].exists = 0; cfs_files[f].name[0] = 0; }
                        uint8_t* dir_buf = (uint8_t*)buf_dir;
                        ahci_read_sectors(drives[selected_drive_idx].base_port, active_fat32_folder_lba, 1, (uint64_t)dir_buf);
                        _39(_192 _43 w = 0; w < 500000; w++) ;
                        
                        FAT32_ParsedFile pfiles[28];
                        if (fat32_list_dir(active_fat32_folder_lba, dir_buf, pfiles, 28, current_folder_id, current_page_offset)) {
                            _39(int i=0; i<28; i++) {
                                cfs_files[i].exists = pfiles[i].exists;
                                cfs_files[i].parent_idx = pfiles[i].parent_idx;
                                cfs_files[i].is_folder = pfiles[i].is_folder;
                                cfs_files[i].size = pfiles[i].size;
                                cfs_files[i].start_lba = pfiles[i].start_lba;
                                _39(int n=0; n<64; n++) cfs_files[i].name[n] = pfiles[i].name[n];
                            }
                        }
                        need_ui_refresh = _86;
                    }
#ifdef __linux__
                    _15(selected_drive_idx == -1 AND need_ui_refresh) {
                        _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                        simulator_page_offset = current_page_offset; disk_read_auto(1002, (uint64_t)buf_dir);
                        CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                        _39(int i=0; i<28; i++) {
                            cfs_files[i].exists = 0;
                            cfs_files[i].is_folder = 0;
                            cfs_files[i].parent_idx = 255;
                            _15(dir[i].type != 0) {
                                cfs_files[i].exists = 1;
                                cfs_files[i].is_folder = (dir[i].type == 2) ? 1 : 0;
                                cfs_files[i].size = dir[i].file_size;
                                cfs_files[i].start_lba = dir[i].start_lba;
                                _39(int n=0; n<11; n++) cfs_files[i].name[n] = dir[i].filename[n];
                                cfs_files[i].name[11] = 0;
                            }
                        }
                        need_ui_refresh = _86;
                    }
#endif

                    /// DATEI-LISTE RENDERN
                    _43 y_off = wy + 170; 
                    _43 visible_count = 0;
                    _39(_43 i=0; i<28; i++) {
                        _15(cfs_files[i].exists AND cfs_files[i].parent_idx EQ current_folder_id) {
                            if (visible_count >= 10) _37; 
                            
                            _44 is_hov = is_over_rect(mouse_x, mouse_y, wx+15, y_off, 265, 20);
                            _89 icon_col = cfs_files[i].is_folder ? 0xFFAA00 : (is_hov ? 0x00AAFF : 0x0088FF);
                            DrawRoundedRect(wx+15, y_off, 16, 16, 2, icon_col);
                            Text(wx+40, y_off+4, cfs_files[i].name, is_hov ? 0xAAAAAA : 0xFFFFFF, _86);
                            
                            DrawRoundedRect(wx+190, y_off, 40, 16, 2, 0x0055AA);
                            Text(wx+198, y_off+4, "OPEN", 0xFFFFFF, _86);
                            
                            DrawRoundedRect(wx+235, y_off, 40, 16, 2, 0xAA5500);
                            Text(wx+241, y_off+4, "COPY", 0xFFFFFF, _86);
                            
                            /// ==========================================
                            /// OPEN BUTTON (FIXED BLOCK-DECODER)
                            /// ==========================================
                            _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_over_rect(mouse_x, mouse_y, wx+190, y_off, 40, 16)) {
                                if (!is_active) focus_window(win->id);
                                if (dsk_mgr_picker_mode) {
                                    char full_z_path[256];
                                    snprintf(full_z_path, 256, "Z:\\%s", cfs_files[i].name);
                                    strncpy(dsk_mgr_picked_file, full_z_path, 255);
                                    dsk_mgr_picker_mode = false;
                                    windows[4].open = false;
                                    input_cooldown = 25;
                                } else {
                                _43 active_port = drives[selected_drive_idx].base_port;
                                
                                _15(cfs_files[i].is_folder) {
                                    print_win(win, "\n[SYS] ENTERING FOLDER...\n");
                                    active_ntfs_folder_lba = cfs_files[i].start_lba; 
                                    active_fat32_folder_lba = cfs_files[i].start_lba;
                                    current_folder_id = i;
                                    current_page_offset = 0; 
                                    need_ui_refresh = _128;
                                } _41 {
                                    _44 is_tba = _86; _44 is_jpg_png = _86; _44 is_bmp = _86; _44 is_ch8 = _86; _44 is_bin = _86; _44 is_wav = _86; _44 is_exe = _86;
                                    
                                    int c = 8; { 
                                        if(cfs_files[i].name[c] == 'B' && cfs_files[i].name[c+1] == 'I' && cfs_files[i].name[c+2] == 'N') is_bin = _128;
                                        if(cfs_files[i].name[c] == 'b' && cfs_files[i].name[c+1] == 'i' && cfs_files[i].name[c+2] == 'n') is_bin = _128;
                                        if(cfs_files[i].name[c] == 'C' && cfs_files[i].name[c+1] == 'H' && cfs_files[i].name[c+2] == '8') is_ch8 = _128;
                                        if(cfs_files[i].name[c] == 'c' && cfs_files[i].name[c+1] == 'h' && cfs_files[i].name[c+2] == '8') is_ch8 = _128;
                                        if((cfs_files[i].name[c] == 'W' && cfs_files[i].name[c+1] == 'A' && cfs_files[i].name[c+2] == 'V') ||
                                           (cfs_files[i].name[c] == 'w' && cfs_files[i].name[c+1] == 'a' && cfs_files[i].name[c+2] == 'v')) is_wav = _128;
                                        if((cfs_files[i].name[c] == 'B' && cfs_files[i].name[c+1] == 'M' && cfs_files[i].name[c+2] == 'P') ||
                                           (cfs_files[i].name[c] == 'b' && cfs_files[i].name[c+1] == 'm' && cfs_files[i].name[c+2] == 'p')) is_bmp = _128;
                                        if((cfs_files[i].name[c] == 'J' && cfs_files[i].name[c+1] == 'P' && cfs_files[i].name[c+2] == 'G') ||
                                           (cfs_files[i].name[c] == 'j' && cfs_files[i].name[c+1] == 'p' && cfs_files[i].name[c+2] == 'g') ||
                                           (cfs_files[i].name[c] == 'P' && cfs_files[i].name[c+1] == 'N' && cfs_files[i].name[c+2] == 'G') ||
                                           (cfs_files[i].name[c] == 'p' && cfs_files[i].name[c+1] == 'n' && cfs_files[i].name[c+2] == 'g')) is_jpg_png = _128;
                                        if((cfs_files[i].name[c] == 'T' && cfs_files[i].name[c+1] == 'B' && cfs_files[i].name[c+2] == 'A') ||
                                           (cfs_files[i].name[c] == 't' && cfs_files[i].name[c+1] == 'b' && cfs_files[i].name[c+2] == 'a')) is_tba = _128;
                                        if((cfs_files[i].name[c] == 'E' && cfs_files[i].name[c+1] == 'X' && cfs_files[i].name[c+2] == 'E') ||
                                           (cfs_files[i].name[c] == 'e' && cfs_files[i].name[c+1] == 'x' && cfs_files[i].name[c+2] == 'e')) is_exe = _128;
                                    }

                                    _15(is_tba) {                                    
                                        print_win(win, "\n[SYS] DECOMPRESSING TBA CONTAINER...\n");
                                        uint32_t tba_lba = cfs_files[i].start_lba;
                                        uint32_t tba_size_bytes = cfs_files[i].size;
                                        _15(tba_size_bytes == 0) tba_size_bytes = 1024 * 1024;
                                        
                                        uint8_t* comp_buffer = tba_comp_buffer; 
                                        uint8_t* decomp_buffer = tba_decomp_buffer; 
                                        
                                        uint32_t tba_sectors = (tba_size_bytes + 511) / 512;
                                        _39(uint32_t s = 0; s < tba_sectors; s++) {
                                            ahci_read_sectors(active_port, tba_lba + s, 1, (uint64_t)comp_buffer + (s * 512));
                                        }
                                        _39(_192 _43 w = 0; w < 500000; w++) ;
                                        
                                        TBAHeader* hdr = (TBAHeader*)comp_buffer;
                                        _15(hdr->magic[0] == 'T' && hdr->magic[1] == 'B' && hdr->magic[2] == 'A' && hdr->magic[3] == '4') {
                                            uint32_t file_count = hdr->fileCount;
                                            uint64_t buf_dir = (uint64_t)global_buf_dir;
                                            ahci_read_sectors(active_port, 1002, 1, buf_dir);
                                            _39(_192 _43 w = 0; w < 500000; w++) ;
                                            CFS_DIR_ENTRY* cfs_dir = (CFS_DIR_ENTRY*)buf_dir;
                                            uint32_t next_free_lba = 12000; 
                                            
                                            _39(uint32_t f = 0; f < file_count; f++) {
                                                uint8_t* entry_ptr = comp_buffer + sizeof(TBAHeader) + (f * sizeof(TBAFileEntry));
                                                TBAFileEntry* current_entry = (TBAFileEntry*)entry_ptr;
                                                
                                                int name_start = 0; int path_len = 0;
                                                _39(int k = 0; k < 512; k++) {
                                                    char c = current_entry->name[k];
                                                    if (c == 0) break;
                                                    if (c == '/' || c == '\\') { name_start = k + 1; path_len = k; }
                                                }
                                                
                                                int true_name_len = 0;
                                                while (current_entry->name[name_start + true_name_len] != 0 && 
                                                       current_entry->name[name_start + true_name_len] != ' ' && 
                                                       (name_start + true_name_len) < 512) {
                                                    true_name_len++;
                                                }
                                                
                                                uint8_t target_parent = current_folder_id;
                                                
                                                _15(path_len > 0) {
                                                    char folder_name[11];
                                                    _39(int n = 0; n < 11; n++) folder_name[n] = 0;
                                                    int f_start = 0;
                                                    for (int k = path_len - 1; k >= 0; k--) {
                                                        if (k > 0 && (current_entry->name[k-1] == '/' || current_entry->name[k-1] == '\\')) { f_start = k; break; }
                                                    }
                                                    int fn_len = path_len - f_start;
                                                    if (fn_len > 11) fn_len = 11;
                                                    _39(int n = 0; n < fn_len; n++) {
                                                        char ch = current_entry->name[f_start + n];
                                                        if (ch >= 'a' && ch <= 'z') ch -= 32;
                                                        folder_name[n] = ch;
                                                    }
                                                    int found_folder_slot = -1;
                                                    _39(int cs = 0; cs < 28; cs++) {
                                                        if (cfs_dir[cs].type == 2 && cfs_dir[cs].parent_idx == current_folder_id) {
                                                            _44 match = _128;
                                                            _39(int n=0; n<11; n++) { if (cfs_dir[cs].filename[n] != folder_name[n]) match = _86; }
                                                            if (match) { found_folder_slot = cs; break; }
                                                        }
                                                    }
                                                    if (found_folder_slot == -1) {
                                                        _39(int cs = 0; cs < 28; cs++) { if (cfs_dir[cs].type == 0) { found_folder_slot = cs; break; } }
                                                        if (found_folder_slot != -1) {
                                                            cfs_dir[found_folder_slot].type = 2; 
                                                            cfs_dir[found_folder_slot].start_lba = 0;
                                                            cfs_dir[found_folder_slot].file_size = 0;
                                                            cfs_dir[found_folder_slot].parent_idx = current_folder_id;
                                                            cfs_dir[found_folder_slot].reserved = 0;
                                                            _39(int n=0; n<11; n++) cfs_dir[found_folder_slot].filename[n] = folder_name[n];
                                                            
                                                            cfs_files[found_folder_slot].exists = 1;
                                                            cfs_files[found_folder_slot].is_folder = 1;
                                                            cfs_files[found_folder_slot].parent_idx = current_folder_id;
                                                            cfs_files[found_folder_slot].size = 0;
                                                            cfs_files[found_folder_slot].start_lba = 0;
                                                            _39(int n = 0; n < 11; n++) cfs_files[found_folder_slot].name[n] = folder_name[n];
                                                            cfs_files[found_folder_slot].name[11] = 0;
                                                        }
                                                    }
                                                    if (found_folder_slot != -1) target_parent = found_folder_slot;
                                                }
                                                
                                                uint32_t comp_sz = (uint32_t)current_entry->compressedSize;
                                                uint32_t orig_sz = (uint32_t)current_entry->originalSize;
                                                
                                                _15(true_name_len == 0 || orig_sz == 0) continue;

                                                int free_slot = -1;
                                                _39(int cs = 0; cs < 28; cs++) {
                                                    if (cfs_dir[cs].type == 0) { free_slot = cs; break; }
                                                }
                                                _15(free_slot == -1) {
                                                    print_win(win, "[ERR] CFS FULL! CANNOT EXTRACT MORE.\n");
                                                    _37; 
                                                }
                                                
                                                uint8_t* src_ptr = comp_buffer + current_entry->offset;
                                                _39(uint32_t m = 0; m < orig_sz + 512; m++) decomp_buffer[m] = 0;
                                                
                                                _15(current_entry->isPureStream == 1) {
                                                    uint32_t framesCount = orig_sz / 26;
                                                    uint32_t dst_p = 0;
                                                    _39(uint32_t fc = 0; fc < framesCount; fc++) {
                                                        decomp_buffer[dst_p] = 0x2A; 
                                                        _39(uint32_t cp = 0; cp < 24; cp++) decomp_buffer[dst_p + 1 + cp] = src_ptr[(fc * 24) + cp];
                                                        decomp_buffer[dst_p + 25] = 0xFF; 
                                                        dst_p += 26;
                                                    }
                                                } _41 _15(current_entry->isCompressed == 1) {
                                                    uint32_t bCnt = *(uint32_t*)src_ptr; src_ptr += 4;
                                                    uint32_t mapBytes = (bCnt + 7) / 8;
                                                    uint8_t* mapData = src_ptr; src_ptr += mapBytes;
                                                    
                                                    uint32_t dstIdx = 0;
                                                    _39(uint32_t b = 0; b < bCnt; b++) {
                                                        uint32_t currentBlockSize = 65536;
                                                        _15(b == bCnt - 1) {
                                                            uint32_t rem = orig_sz % 65536;
                                                            _15(rem != 0) currentBlockSize = rem;
                                                        }
                                                        
                                                        _44 isBlockComp = (mapData[b / 8] & (1 << (b % 8))) != 0;
                                                        _15(isBlockComp) {
                                                            uint32_t cSize = *(uint32_t*)src_ptr; src_ptr += 4;
                                                            decompress_tba((_184*)src_ptr, cSize, (_184*)(decomp_buffer + dstIdx), currentBlockSize);
                                                            src_ptr += cSize;
                                                        } _41 {
                                                            _39(uint32_t m = 0; m < currentBlockSize; m++) decomp_buffer[dstIdx + m] = src_ptr[m];
                                                            src_ptr += currentBlockSize;
                                                        }
                                                        dstIdx += currentBlockSize;
                                                    }
                                                } _41 {
                                                    _39(uint32_t m = 0; m < orig_sz; m++) decomp_buffer[m] = src_ptr[m];
                                                }
                                                
                                                uint32_t write_sectors = (orig_sz + 511) / 512;
                                                _39(uint32_t ws = 0; ws < write_sectors; ws++) {
                                                    ahci_write_sectors(active_port, next_free_lba + ws, 1, (uint64_t)decomp_buffer + (ws * 512));
                                                    _39(_192 _43 w = 0; w < 2000; w++) ;
                                                }
                                                
                                                cfs_dir[free_slot].type = 1; 
                                                cfs_dir[free_slot].start_lba = (uint16_t)next_free_lba;
                                                cfs_dir[free_slot].file_size = (uint16_t)orig_sz;
                                                cfs_dir[free_slot].parent_idx = target_parent; 
                                                cfs_dir[free_slot].reserved = 0;
                                                
                                                _39(int n = 0; n < 11; n++) cfs_dir[free_slot].filename[n] = 0;
                                                
                                                int dot_idx = -1; int end_idx = 0;
                                                _39(int k = 511; k >= 0; k--) {
                                                    char c = current_entry->name[k];
                                                    if (end_idx == 0 && c > 32 && c <= 126) end_idx = k;
                                                    if (c == '.' && dot_idx == -1) dot_idx = k;
                                                }
                                                int start_idx = 0;
                                                if (end_idx > 0) {
                                                    _39(int k = end_idx; k >= 0; k--) {
                                                        char c = current_entry->name[k];
                                                        if (c == '/' || c == '\\') { start_idx = k + 1; break; }
                                                    }
                                                }
                                                if (dot_idx != -1 && dot_idx > start_idx) {
                                                    int base_len = dot_idx - start_idx;
                                                    int ext_len = end_idx - dot_idx + 1; 
                                                    if (ext_len > 4) ext_len = 4; 
                                                    int max_base_len = 11 - ext_len;
                                                    if (base_len > max_base_len) base_len = max_base_len; 
                                                    
                                                    _39(int n = 0; n < base_len; n++) cfs_dir[free_slot].filename[n] = current_entry->name[start_idx + n];
                                                    _39(int n = 0; n < ext_len; n++) cfs_dir[free_slot].filename[base_len + n] = current_entry->name[dot_idx + n];
                                                } else {
                                                    int len = end_idx - start_idx + 1;
                                                    if (len > 11) len = 11;
                                                    _39(int n = 0; n < len; n++) cfs_dir[free_slot].filename[n] = current_entry->name[start_idx + n];
                                                }
                                                
                                                _39(int n = 0; n < 11; n++) {
                                                    char ch = cfs_dir[free_slot].filename[n];
                                                    if (ch >= 'a' && ch <= 'z') cfs_dir[free_slot].filename[n] = ch - 32;
                                                }
                                                
                                                cfs_files[free_slot].exists = 1;
                                                cfs_files[free_slot].is_folder = 0;
                                                cfs_files[free_slot].parent_idx = target_parent;
                                                cfs_files[free_slot].size = orig_sz;
                                                cfs_files[free_slot].start_lba = next_free_lba;
                                                _39(int n = 0; n < 11; n++) cfs_files[free_slot].name[n] = cfs_dir[free_slot].filename[n];
                                                cfs_files[free_slot].name[11] = 0;
                                                
                                                next_free_lba += write_sectors + 2; 
                                            }
                                            
                                            ahci_write_sectors(active_port, 1002, 1, buf_dir);
                                            _39(_192 _43 w = 0; w < 500000; w++) ;
                                            
                                            print_win(win, "[OK] SUCCESS! ALL ROMS UNPACKED!\n");
                                        } _41 {
                                            print_win(win, "[ERR] BAD TBA SIGNATURE!\n");
                                        }
                                        input_cooldown = 25;
                                        need_ui_refresh = _128; 
                                    }
                                    _41 _15(is_exe) {
                                        windows[15].cursor_pos = 0;
                                        windows[15].content[0] = 0;
                                        print_win(&windows[15], "\n[SYS] LOADING WINDOWS EXE...\n");
                                        windows[15].open = _128; windows[15].minimized = _86; focus_window(15);
                                        uint64_t file_ram_addr = get_ram_addr_copy();
#ifdef __linux__
                                        if (selected_drive_idx == -1) {
                                            struct stat st;
                                            if (stat(linux_file_paths[i], &st) == 0) {
                                                cfs_files[i].size = st.st_size;
                                            }
                                        }
#endif
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;
                                        if (cfs_files[i].size > 30 * 1024 * 1024) {
                                            print_win(&windows[15], "[ERR] EXE TOO LARGE (>30MB)! ABORTING.\n");
                                            input_cooldown = 25;
                                            need_ui_refresh = _128;
                                            continue;
                                        }
#ifdef __linux__
                                        if (selected_drive_idx == -1) {
                                            disk_read_auto(1000000 + i, file_ram_addr);
                                        } else {
#endif
                                        _39(uint32_t sec = 0; sec < total_sectors; sec++) {
                                            ahci_read_sectors(active_port, cfs_files[i].start_lba + sec, 1, file_ram_addr + (sec * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
#ifdef __linux__
                                        }
#endif
                                        uint64_t final_entry = load_and_resolve_pe((uint8_t*)file_ram_addr, cfs_files[i].size);
                                        if (final_entry != 0) {
                                            print_win(&windows[15], "[SYS] EXE READY. STARTING NATIVE TASK...\n");
#ifdef __linux__
                                            pthread_t t;
                                            pthread_create(&t, NULL, linux_exe_thread, (void*)final_entry);
                                            pthread_detach(t);
#else
                                            create_task((void (*)()) final_entry);
#endif
                                        } else {
                                            print_win(&windows[15], "[ERR] HYBRID: FAILED TO LOAD PE.\n");
                                        }
                                        input_cooldown = 25;
                                        need_ui_refresh = _128;
                                    }
                                    _41 _15(is_jpg_png) {
                                        print_win(win, "\n[SYS] DECODING JPG/PNG IN MEMORY...\n");
                                        uint64_t raw_ram_addr = get_ram_addr_raw();
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;
#ifdef __linux__
                                        if (selected_drive_idx == -1) {
                                            disk_read_auto(1000000 + i, raw_ram_addr);
                                        } else {
#endif
                                        _39(uint32_t sec = 0; sec < total_sectors; sec++) {
                                            ahci_read_sectors(active_port, cfs_files[i].start_lba + sec, 1, raw_ram_addr + (sec * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
#ifdef __linux__
                                        }
#endif
                                        int w=0, h=0, comp=0;
                                        unsigned char* decoded = decode_image((unsigned char*)raw_ram_addr, cfs_files[i].size, &w, &h, &comp, 3);
                                        _15(decoded != 0) {
                                            uint64_t bmp_ram_addr = get_ram_addr_bmp();
                                            uint8_t* bmp = (uint8_t*)bmp_ram_addr;
                                            _39(int z=0; z<54; z++) bmp[z] = 0;
                                            bmp[0] = 'B'; bmp[1] = 'M';
                                            *(uint32_t*)(bmp_ram_addr + 2) = 54 + (w * h * 3);
                                            *(uint32_t*)(bmp_ram_addr + 10) = 54;
                                            *(uint32_t*)(bmp_ram_addr + 14) = 40;
                                            *(int32_t*)(bmp_ram_addr + 18) = w;
                                            *(int32_t*)(bmp_ram_addr + 22) = h;
                                            *(uint16_t*)(bmp_ram_addr + 26) = 1;
                                            *(uint16_t*)(bmp_ram_addr + 28) = 24;
                                            *(uint32_t*)(bmp_ram_addr + 30) = 0;
                                            *(uint32_t*)(bmp_ram_addr + 34) = w * h * 3;
                                            
                                            uint8_t* pixels = (uint8_t*)(bmp_ram_addr + 54);
                                            _39(int y=0; y<h; y++) {
                                                _39(int x=0; x<w; x++) {
                                                    int src_idx = (y * w + x) * 3;
                                                    int dst_idx = ((h - 1 - y) * w + x) * 3;
                                                    pixels[dst_idx] = decoded[src_idx + 2];
                                                    pixels[dst_idx + 1] = decoded[src_idx + 1];
                                                    pixels[dst_idx + 2] = decoded[src_idx];
                                                }
                                            }
                                            
                                            free_image(decoded);
                                            
                                            print_win(win, "\n[SYS] OPENING IMAGE VIEWER...\n");
                                            windows[9].open = _128; windows[9].minimized = _86; focus_window(9);
                                            str_cpy(windows[9].title, cfs_files[i].name);
                                            windows[9].content[0] = 'B'; windows[9].content[1] = 'M'; windows[9].content[2] = 'P'; windows[9].content[3] = 0;
                                        } _41 {
                                            print_win(win, "\n[ERR] DECODE FAILED!\n");
                                        }
                                    } _41 _15(is_bmp) {
                                        print_win(win, "\n[SYS] OPENING IMAGE VIEWER...\n");
                                        windows[9].open = _128; windows[9].minimized = _86; focus_window(9);
                                        str_cpy(windows[9].title, cfs_files[i].name);
                                        uint64_t bmp_ram_addr = get_ram_addr_bmp();
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;
                                        _39(uint32_t sec = 0; sec < total_sectors; sec++) {
                                            ahci_read_sectors(active_port, cfs_files[i].start_lba + sec, 1, bmp_ram_addr + (sec * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
                                        windows[9].content[0] = 'B'; windows[9].content[1] = 'M'; windows[9].content[2] = 'P'; windows[9].content[3] = 0;
                                    } _41 _15(is_bin) {
                                        print_win(win, "\n[SYS] LOADING BINARY APP...\n");
                                        
                                        char _n0c = cfs_files[i].name[0]; if (_n0c >= 'a' && _n0c <= 'z') _n0c -= 32;
                                        char _n1c = cfs_files[i].name[1]; if (_n1c >= 'a' && _n1c <= 'z') _n1c -= 32;
                                        char _n2c = cfs_files[i].name[2]; if (_n2c >= 'a' && _n2c <= 'z') _n2c -= 32;
                                        _15(_n0c == 'A' && _n1c == 'P' && _n2c == 'P') {
                                            print_win(win, "[SYS] ZUENDE NATIVE 3D SURREAL ENGINE!\n");
                                            
                                            windows[14].id = 14;   
                                            windows[14].w = 800;   
                                            windows[14].h = 600;   
                                            windows[14].x = (screen_w - windows[14].w) / 2;   
                                            windows[14].y = (screen_h - windows[14].h) / 2;   
                                            windows[14].open = _128;
                                            windows[14].minimized = _86;
                                            str_cpy(windows[14].title, "Super Smash Cats 3D");
                                            
                                            windows[14].content[0] = 'A'; 
                                            windows[14].content[1] = 'P'; 
                                            windows[14].content[2] = 'P'; 
                                            windows[14].content[3] = 0;
                                            
                                            os_app_x = windows[14].x;
                                            os_app_y = windows[14].y;
                                            os_app_w = windows[14].w;
                                            os_app_h = windows[14].h;
                                            
                                            app_window_active = _128; 
                                            focus_window(14);
                                            
                                        } _41 {
                                            uint32_t bin_sz = cfs_files[i].size;
                                            _15(bin_sz == 0) bin_sz = 16384; 
                                            uint32_t app_sectors = (bin_sz + 511) / 512;
                                            
                                            if (load_and_run_bin(cfs_files[i].start_lba, app_sectors)) {
                                                print_win(win, "[SYS] TASK SPAWNED!\n"); 
                                            } else { 
                                                print_win(win, "[ERR] LOAD FAILED!\n");
                                            }
                                        }
                                    } _41 _15(is_ch8) {
                                        print_win(win, "\n[SYS] SENDING .BIN TO CHIP-8 HYPERVISOR...\n");
                                        c8_reset();
                                        _43 sectors = (cfs_files[i].size + 511) / 512;
                                        if (sectors > 7) sectors = 7;
                                        
                                        unsigned long long target_ram = (unsigned long long)&c8_ram[0x200];
                                        for (_43 s = 0; s < sectors; s++) {
                                            ahci_read_sectors(active_port, cfs_files[i].start_lba + s, 1, target_ram + (s * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
                                        
                                        c8_pc = 0x200;
                                        c8_state = 1;
                                        extern VirtualMachine emu_chip8;
                                        extern VirtualMachine* active_vm;
                                        active_vm = &emu_chip8;
                                        
                                        windows[13].open = _128; 
                                        windows[13].minimized = _86; 
                                        focus_window(13);
                                        print_win(win, "[SYS] EMULATOR LAUNCHED!\n");
                                    } _41 _15(is_wav) {
                                        print_win(win, "\n[SYS] PLAYING WAV AUDIO...\n");
                                        uint64_t wav_ram_addr = get_ram_addr_wav();
                                        uint32_t total_sectors = (cfs_files[i].size / 512) + 1;
                                        _39(uint32_t sec = 0; sec < total_sectors; sec++) {
                                            ahci_read_sectors(active_port, cfs_files[i].start_lba + sec, 1, wav_ram_addr + (sec * 512));
                                            _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                        }
                                        
                                        uint8_t* wav = (uint8_t*)wav_ram_addr;
                                        if (wav[0] == 'R' && wav[1] == 'I' && wav[2] == 'F' && wav[3] == 'F') {
                                            uint16_t channels = *(uint16_t*)&wav[22];
                                            uint32_t sample_rate = *(uint32_t*)&wav[24];
                                            uint16_t bits = *(uint16_t*)&wav[34];
                                            uint32_t data_size = 0;
                                            uint32_t data_offset = 12;
                                            while (data_offset < 100) {
                                                if (wav[data_offset] == 'd' && wav[data_offset+1] == 'a' && wav[data_offset+2] == 't' && wav[data_offset+3] == 'a') {
                                                    data_size = *(uint32_t*)&wav[data_offset+4];
                                                    data_offset += 8;
                                                    break;
                                                }
                                                data_offset++;
                                            }
                                            if (data_size > 0) {
                                                play_hda_wav(wav_ram_addr + data_offset, data_size, sample_rate, channels, bits);
                                            }
                                        }
                                    } _41 {
                                        print_win(win, "\n[SYS] OPENING IN NOTEPAD...\n");
                                        windows[0].open = _128; windows[0].minimized = _86; focus_window(0);
                                        active_file_lba = cfs_files[i].start_lba; active_file_idx = i;
                                        str_cpy(windows[0].title, cfs_files[i].name);
                                        uint64_t text_ram_addr = get_ram_addr_text(); char* text_buffer = (char*)text_ram_addr;
                                        _39(int j=0; j<2000; j++) text_buffer[j] = 0;
                                        
                                        ahci_read_sectors(active_port, active_file_lba, 1, text_ram_addr);
                                        _39(_192 _43 wait = 0; wait < 1000000; wait++) ;
                                        
                                        int limit = cfs_files[i].size;
                                        if(limit > 2000) limit = 2000; if(limit == 0) limit = 512;
                                        int c_idx = 0;
                                        for(int c=0; c<limit; c++) {
                                            char ch = text_buffer[c]; if(ch == 0) break; 
                                            if((ch >= 32 && ch <= 126) || ch == '\n') { windows[0].content[c_idx++] = ch; }
                                        }
                                        windows[0].content[c_idx] = 0; windows[0].cursor_pos = c_idx;
                                    }
                                }
                                }
                                input_cooldown = 25;
                            }
                            
                            /// COPY BUTTON
                            _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+235, y_off, 40, 16)) {
                                str_cpy(win->title, "READING..."); 
                                print_win(win, "\n[SYS] COPY TO RAM CLIPBOARD...\n");
                                clipboard_is_folder = cfs_files[i].is_folder;
                                
                                _15(!clipboard_is_folder) {
                                    uint64_t copy_ram_buffer = get_ram_addr_copy();
                                    uint32_t c_size = cfs_files[i].size;
                                    if (c_size == 0) c_size = 5120;
                                    clipboard_size = c_size;
                                    
                                    uint32_t copy_sec = (c_size + 511) / 512;
                                    if (copy_sec > 60000) copy_sec = 60000;

                                    _39(uint32_t s=0; s<copy_sec; s++) {
                                        ahci_read_sectors(drives[selected_drive_idx].base_port, cfs_files[i].start_lba + s, 1, copy_ram_buffer + (s * 512));
                                        _39(_192 _43 wait = 0; wait < 2000; wait++) ;
                                    }
                                }
                                
                                str_cpy(clipboard_name, cfs_files[i].name);
                                clipboard_active = _128; 
                                
                                str_cpy(win->title, "IN CLIPBOARD");
                                print_win(win, "[OK] READY TO PASTE.\n");
                                input_cooldown = 25;
                            }
                            
                            visible_count++;
                            y_off += 25;
                        }
                    }
                } _41 {
                    _43 list_y = wy + 60;
                    Text(wx+15, list_y - 15, "AVAILABLE DRIVES:", 0xAAAAAA, _128);
                    
                    DrawRoundedRect(wx+140, list_y - 18, 35, 16, 2, 0x444444);
                    Text(wx+146, list_y - 15, "REF", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+140, list_y - 18, 35, 16)) {
                        drives_scanned = _86; 
                        input_cooldown = 15;
                    }
                    
                    DrawRoundedRect(wx+180, list_y - 18, 65, 16, 2, 0x333333);
                    Text(wx+185, list_y - 15, "NO USB", 0x888888, _128);
                
                    _39(_43 i=0; i < drive_count; i++) {
                        _44 is_sel = (selected_drive_idx == i);
                        DrawRoundedRect(wx+15, list_y, 160, 25, 4, is_sel ? 0x0088FF : 0x333333);
                        Text(wx+25, list_y+5, drives[i].model, 0xFFFFFF, _128);
                        
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+15, list_y, 160, 25)) {
                            selected_drive_idx = i; 
                            is_mounted = false; 
                            input_cooldown = 15;
                        }
                        list_y += 30;
                    }
#ifdef __linux__
                    {
                        _44 is_sel = (selected_drive_idx == -1);
                        DrawRoundedRect(wx+15, list_y, 160, 25, 4, is_sel ? 0x0088FF : 0x333333);
                        Text(wx+25, list_y+5, "SIMULATOR (LINUX)", 0xFFFFFF, _128);

                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+15, list_y, 160, 25)) {
                            selected_drive_idx = -1;
                            is_mounted = false;
                            input_cooldown = 15;
                        }
                        list_y += 30;
                    }
#endif
                
                    _89 btn_col = (selected_drive_idx == -1) ? 0x444444 : 0x00AA00;
                    DrawRoundedRect(wx+150, wy+60, 80, 25, 4, btn_col); 
                    TextC(wx+190, wy+68, "OPEN", 0xFFFFFF, _128);
                    
                    DrawRoundedRect(wx+240, wy+60, 80, 25, 4, (selected_drive_idx == -1) ? 0x444444 : 0xAA0000); 
                    TextC(wx+280, wy+68, "FORMAT", 0xFFFFFF, _128);
                    
                    char diag[64];
                    str_cpy(diag, "USB IO BASE: 0x");
                    hex_to_str(usb_io_base, diag + 15);
                    Text(wx+15, wy+220, diag, 0xAAAAAA, _86);
                    
                    char diag2[64];
                    str_cpy(diag2, "xHCI BASE: 0x");
                    hex_to_str((uint32_t)global_xhci_base_addr, diag2 + 13);
                    Text(wx+15, wy+235, diag2, 0xAAAAAA, _86);
                    Text(wx+15, wy+250, cmd_status, 0xFF8800, _86);
                    
                    /// FORMATIEREN (FIXED: Targeted Write!)
                    _15(selected_drive_idx != -1 AND input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+240, wy+60, 80, 25)) {
                        win->cursor_pos = 0; 
                        char* mbr = (char*)buf_mbr; _39(int i=0; i<512; i++) mbr[i] = 0;
                        mbr[3] = 'C'; mbr[4] = 'F'; mbr[5] = 'S'; mbr[510] = 0x55; mbr[511] = 0xAA;         
                        ahci_write_sectors(drives[selected_drive_idx].base_port, 0, 1, (uint64_t)buf_mbr); 
                        _39(_192 _43 wait = 0; wait < 1000000; wait++) ;
                        
                        char* dir = (char*)buf_dir; _39(int i=0; i<512; i++) dir[i] = 0;
                        ahci_write_sectors(drives[selected_drive_idx].base_port, 1002, 1, (uint64_t)buf_dir);
                        _39(_192 _43 wait2 = 0; wait2 < 1000000; wait2++) ;
                        
                        _39(int i=0; i<28; i++) { cfs_files[i].exists = 0; cfs_files[i].is_folder = 0; cfs_files[i].parent_idx = 255; }
                        is_mounted = false; current_folder_id = 255; 
                        print_win(win, "\n[OK] OS2-CFS V2 FORMATTED.\n");
                        input_cooldown = 15;
                    }
                    
                    /// OPEN DRIVE (FIXED: Targeted Read!)
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+150, wy+60, 80, 25)) {
                        win->cursor_pos = 0;
#ifdef __linux__
                        if (selected_drive_idx == -1) {
                            print_win(win, "\n[OK] SIMULATOR (LINUX EXT4) MOUNTED.\n");
                            is_ntfs_drive = _86; is_fat32_drive = _86;
                            
                            _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                            simulator_page_offset = current_page_offset; disk_read_auto(1002, (uint64_t)buf_dir);
                            CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                            _39(int i=0; i<28; i++) {
                                cfs_files[i].exists = 0;
                                cfs_files[i].is_folder = 0;
                                cfs_files[i].parent_idx = 255;
                                _15(dir[i].type != 0) {
                                    cfs_files[i].exists = 1;
                                    cfs_files[i].is_folder = (dir[i].type == 2) ? 1 : 0;
                                    cfs_files[i].size = dir[i].file_size;
                                    cfs_files[i].start_lba = dir[i].start_lba;
                                    _39(int n=0; n<11; n++) cfs_files[i].name[n] = dir[i].filename[n];
                                    cfs_files[i].name[11] = 0;
                                }
                            }
                            
                            current_folder_id = 255;
                            dsk_mgr_opened = _128;
                            is_mounted = _128;
                            input_cooldown = 15;
                        } else {
#endif
                        _43 drive_type = drives[selected_drive_idx].type;
                        _43 active_port = drives[selected_drive_idx].base_port;
                        
                        _15(drive_type == 2) {
                            active_sata_port = active_port;
                            ahci_identify((uint64_t)buf_mbr);
                            uint32_t lba_low = *(uint32_t*)(buf_mbr + 200);
                            drive_total_gb = lba_low / 2097152; _15(drive_total_gb == 0) drive_total_gb = 1; 
                        } _41 {
                            drive_total_gb = drives[selected_drive_idx].size_mb / 1024;
                            _15(drive_total_gb == 0) drive_total_gb = 1; 
                        }
                        
                        _39(int i=0; i<512; i++) ((char*)buf_mbr)[i] = 0;
                        ahci_read_sectors(active_port, 0, 1, (uint64_t)buf_mbr);
                        _39(_192 _43 wait = 0; wait < 1000000; wait++) ;
                        
                        uint8_t* boot = (uint8_t*)buf_mbr;
                        _39(int i=0; i<28; i++) { cfs_files[i].exists = 0; cfs_files[i].parent_idx = 255; cfs_files[i].is_folder = 0; }
                        is_mounted = true; drive_used_kb = 0;

                        /// FALL 1: NATIVES CFS
                        _15(boot[3] == 'C' && boot[4] == 'F' && boot[5] == 'S') {
                            is_ntfs_drive = _86; 
                            _39(int i=0; i<512; i++) ((char*)buf_dir)[i] = 0;
                            
                            ahci_read_sectors(active_port, 1002, 1, (uint64_t)buf_dir);
                            _39(_192 _43 wait2 = 0; wait2 < 1000000; wait2++) ;
                            
                            CFS_DIR_ENTRY* dir = (CFS_DIR_ENTRY*)buf_dir;
                            _39(int i=0; i<28; i++) {
                                _15(dir[i].type != 0) { 
                                    cfs_files[i].exists = 1; 
                                    cfs_files[i].is_folder = (dir[i].type == 2) ? 1 : 0; 
                                    cfs_files[i].parent_idx = dir[i].parent_idx;
                                    _39(int n=0; n<11; n++) { char c = dir[i].filename[n]; cfs_files[i].name[n] = (c >= 32 && c <= 126) ? c : 0; }
                                    cfs_files[i].name[11] = 0;
                                    cfs_files[i].size = dir[i].file_size; 
                                    cfs_files[i].start_lba = dir[i].start_lba;
                                    drive_used_kb += dir[i].file_size; 
                                }
                            }
                            need_ui_refresh = _128; 
                            print_win(win, "\n[OK] OS2 CFS V2 MOUNTED.\n");
                        }
                        // ==========================================
                        // NEU: exFAT ERKENNUNG
                        // ==========================================
                        _41 _15(boot[3] == 'E' && boot[4] == 'X' && boot[5] == 'F' && boot[6] == 'A' && boot[7] == 'T') {
                            print_win(win, "\n[OK] exFAT VOLUME FOUND!\n");
                            print_win(win, "[SYS] exFAT PARSER NOT IMPLEMENTED YET.\n");
                            // Hier kommt später dein exFAT Code rein!
                        }
                        // ==========================================
                        _41 _15(boot[0] == 0x50 && boot[1] == 0x4B && boot[2] == 0x03 && boot[3] == 0x04) {
                            print_win(win, "\n[OK] ANDROID APK DETECTED.\n");
                        }
                        _41 {
                            uint8_t part_type = boot[446 + 4];
                            _39(int i=0; i<28; i++) cfs_files[i].exists = 0;
                            
                            _43 target_ntfs_lba = 0; 
                            _43 target_fat32_lba = 0;
                            
                            _15(boot[510] == 0x55 && boot[511] == 0xAA) {
                                _15(part_type == 0xEE) {
                                    print_win(win, "\n[OK] GPT DRIVE DETECTED.\n");
                                    ahci_read_sectors(active_port, 1, 1, (uint64_t)buf_dir);
                                    _39(_192 _43 w = 0; w < 500000; w++) ; 
                                    uint64_t table_lba = *(uint64_t*)(buf_dir + 72);
                                    
                                    ahci_read_sectors(active_port, table_lba, 1, (uint64_t)buf_dir);
                                    _39(_192 _43 w2 = 0; w2 < 500000; w2++) ; 
                                    _39(int p=0; p<4; p++) {
                                        uint64_t slba = *(uint64_t*)(buf_dir + (p * 128) + 32);
                                        _15(slba > 0) {
                                            ahci_read_sectors(active_port, slba, 1, (uint64_t)buf_mbr);
                                            _39(_192 _43 w3 = 0; w3 < 200000; w3++) ; 
                                            if (((uint8_t*)buf_mbr)[3]=='N' && ((uint8_t*)buf_mbr)[4]=='T') { target_ntfs_lba = slba; _37; }
                                            if (((uint8_t*)buf_mbr)[82]=='F' && ((uint8_t*)buf_mbr)[83]=='A' && ((uint8_t*)buf_mbr)[84]=='T') { target_fat32_lba = slba; }
                                        }
                                    }
                                } _41 {
                                    _39(int p=0; p<4; p++) {
										uint8_t pt = boot[446 + (p * 16) + 4];
										
										// FIX: Wenn 'NTFS' gefunden wird, setze LBA zwingend auf 0!
										_15(pt == 0x07 || (boot[3]=='N' && boot[4]=='T')) {
											target_ntfs_lba = (boot[3]=='N' && boot[4]=='T') ? 0 : *(_43*)&boot[446 + (p * 16) + 8];
											_37;
										} 
										_41 _15(pt == 0x0B || pt == 0x0C || (boot[82]=='F' && boot[83]=='A' && boot[84]=='T')) {
											target_fat32_lba = (boot[82]=='F') ? 0 : *(_43*)&boot[446 + (p * 16) + 8];
											_37;
										}
									}
                                }
                            }
                            
                            _15(target_ntfs_lba > 0) {
                                print_win(win, "\n[OK] NTFS VOLUME FOUND!\n");
                                is_ntfs_drive = _128;
                                is_fat32_drive = _86;
                                
                                ahci_read_sectors(active_port, target_ntfs_lba, 1, (uint64_t)buf_dir);
                                _39(_192 _43 w = 0; w < 500000; w++) ; 
                                uint8_t* vbr = (uint8_t*)buf_dir;
                                _43 sec_per_cluster = vbr[13];
                                global_ntfs_lba = target_ntfs_lba;
                                global_ntfs_spc = sec_per_cluster;
                                uint64_t mft_cluster = *(uint64_t*)&vbr[48];
                                uint64_t mft_lba = target_ntfs_lba + (mft_cluster * sec_per_cluster);
                                
                                uint8_t* mft_cache = global_mft_cache; /// FIX: Nutzt echten Buffer!
                                print_win(win, "[SYS] CACHING MFT RECORDS (BULK)...\n");
                                
                                _43 total_sectors = 25000 * 2; 
                                _43 bulk_size = 128;            
                                _43 port_no = active_port;
                                
                                _39(_43 offset = 0; offset < total_sectors; offset += bulk_size) {
                                    _43 remaining = total_sectors - offset;
                                    _43 chunk = (remaining < bulk_size) ? remaining : bulk_size;
                                    uint64_t lba = mft_lba + offset;
                                    uint64_t ram = (uint64_t)(mft_cache + (offset * 512));
                                    ahci_read_sectors(port_no, lba, chunk, ram);
                                    _39(_192 _43 w1 = 0; w1 < 5000; w1++) ;
                                }
                                print_win(win, "[OK] CACHE READY! RAM SPEED UNLOCKED.\n");
                                need_ui_refresh = _128; 
                            } _41 _15(target_fat32_lba > 0 || (boot[82]=='F' && boot[83]=='A' && boot[84]=='T')) {
                                print_win(win, "\n[OK] FAT32 VOLUME FOUND!\n");
                                is_fat32_drive = _128;
                                is_ntfs_drive = _86;
                                
                                ahci_read_sectors(active_port, target_fat32_lba, 1, (uint64_t)buf_dir);
                                _39(_192 _43 w = 0; w < 500000; w++) ;
                                
                                if (fat32_init(target_fat32_lba, (uint8_t*)buf_dir)) {
                                    active_fat32_folder_lba = fat32_root_lba;
                                    need_ui_refresh = _128;
                                } else {
                                    print_win(win, "\n[ERR] FAT32 INIT FAILED!\n");
                                    is_fat32_drive = _86;
                                }
                            } _41 { print_win(win, "\n[ERR] NO VALID NTFS/FAT32 FOUND.\n"); }
                        }
                        
                        current_folder_id = 255; 
                        dsk_mgr_opened = _128;   
                        input_cooldown = 15;
#ifdef __linux__ 
                        }
#endif
                    }
			/// ==========================================
                }
            }
            /// NOTEPAD (ID 0) - BARE METAL FIX (SICHTBAR!)
            /// ==========================================
            _15(win->id EQ 0) {
                /// BARE METAL FIX: Fokus-Check für Notepad Buttons!
                _44 is_active = (win_z[19] EQ win->id);
                
                /// BARE METAL FIX: Harte Farbe (Weiß) für Text!
                _89 safe_txt_color = 0xFFFFFF; 
                Text(wx+15, wy+45, win->content, safe_txt_color, _86);
                
                /// BARE METAL FIX: Sichtbarer Block-Cursor!
                _15(win_z[19] EQ win->id AND (frame / 20) % 2 EQ 0) {
                    _43 cursor_off_x = 0; _43 cursor_off_y = 0;
                    _39(_43 c_idx = 0; c_idx < win->cursor_pos; c_idx++) { 
                        _15(win->content[c_idx] EQ '\n') { cursor_off_y += 15; cursor_off_x = 0; } 
                        _41 cursor_off_x += 6; 
                    }
                    DrawRoundedRect(wx + 15 + cursor_off_x, wy + 45 + cursor_off_y, 6, 10, 0, safe_txt_color);
                }
                
                /// SAVE BUTTON (Idiotensicher)
                DrawRoundedRect(wx+ww-80, wy+15, 60, 20, 3, 0x005500); TextC(wx+ww-50, wy+21, "SAVE", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-80, wy+15, 60, 20)) {
                    str_cpy(win->title, "SAVING..."); 
                    
                    uint64_t file_ram_addr = get_ram_addr_copy(); char* file_data = (char*)file_ram_addr;
                    for(int i=0; i<512; i++) file_data[i] = 0; 
                    for(int i=0; i < win->cursor_pos; i++) file_data[i] = win->content[i];
                    
                    _43 test_lba = 500; 
                    if(active_file_lba > 0) test_lba = active_file_lba;
                    
                    if(disk_write_auto(test_lba, file_ram_addr)) {
                        str_cpy(win->title, "NOTEPAD - SAVED!");
                    } else {
                        str_cpy(win->title, "NOTEPAD - WRITE ERROR!");
                    }
                    input_cooldown = 25;
                }
                
                /// SAVE AS BUTTON
                DrawRoundedRect(wx+ww-160, wy+15, 75, 20, 3, 0x444444); 
                TextC(wx+ww-122, wy+21, "SAVE AS", 0xFFFFFF, _128);
                _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-160, wy+15, 75, 20)) {
                    windows[2].open = _128; 
                    windows[2].minimized = _86; 
                    focus_window(2); 
                    save_step = 0; 
                    input_cooldown = 25;
                }
            }
            /// ==========================================
            /// HYBRID KERNEL LOG (ID 15)
            /// ==========================================
            _15(win->id EQ 15) {
                _89 safe_txt_color = 0x00FF00;
                Text(wx+15, wy+45, win->content, safe_txt_color, _86);
            }
			/// =========================================================
            /// BARE METAL FIX: SAVE AS & CREATE FOLDER (ID 2)
            /// =========================================================
            _15(win->id EQ 2) {
                _44 is_active = (win_z[19] EQ win->id);
                /// Feste, sichere DMA RAM-Adressen (64-Bit OS2 Alignments)
                uint64_t buf_dir = (uint64_t)global_buf_dir;
                uint64_t text_ram_addr = get_ram_addr_copy(); /// Gleicher Buffer wie beim normalen Save
                _15(save_step EQ 0) { 
                    Text(wx+20, wy+40, "DESTINATION: ACTIVE MOUNTED DRIVE", 0x000000, _128);
                    _15(!is_mounted) {
                        Text(wx+20, wy+70, "NO CFS DRIVE MOUNTED!", 0xCC4444, _128);
                        Text(wx+20, wy+90, "PLEASE OPEN DISK MGR AND MOUNT FIRST.", 0x555555, _128);
                    } _41 {
                        Text(wx+20, wy+70, "DRIVE IS READY. SELECT ACTION:", 0x00AA00, _128);
                        /// CREATE FOLDER BUTTON
                        DrawRoundedRect(wx+20, wy+wh-40, 110, 25, 5, 0xCCCCCC); 
                        TextC(wx+75, wy+wh-32, "NEW FOLDER", 0x000000, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+20, wy+wh-40, 110, 25)) { 
                            save_step = 2; input_cooldown = 25; 
                        }
                        /// NEXT (FILE SAVE) BUTTON
                        DrawRoundedRect(wx+ww-110, wy+wh-40, 90, 25, 5, 0x555555); 
                        TextC(wx+ww-65, wy+wh-32, "NEXT (FILE)", 0xFFFFFF, _128);
                        _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-110, wy+wh-40, 90, 25)) { 
                            save_step = 1; input_cooldown = 25; 
                        }
                    }
                } _41 _15(save_step EQ 1) { 
                    Text(wx+20, wy+40, "ENTER FILENAME (MAX 11 CHARS):", 0x000000, _128); 
                    DrawRoundedRect(wx+20, wy+60, ww-40, 25, 2, 0xCCCCCC); 
                    Text(wx+25, wy+65, save_filename, 0x000000, _128);
                    _15((frame/20)%2 EQ 0) DrawChar(wx+25+(save_name_idx*7), wy+65, '_', 0x000000, _128);
                    DrawRoundedRect(wx+ww-100, wy+wh-40, 80, 25, 5, 0x00AA00); 
                    TextC(wx+ww-60, wy+wh-32, "SAVE", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-100, wy+wh-40, 80, 25)) { 
                        /// 1. TEXT INS RAM KOPIEREN
                        char* file_data = (char*)text_ram_addr;
                        _39(int i=0; i<5120; i++) file_data[i] = 0; 
                        _39(int i=0; i < windows[0].cursor_pos; i++) file_data[i] = windows[0].content[i];
                        /// 2. INHALTSVERZEICHNIS LADEN (Nutzt deinen Universal-Adapter!)
                        simulator_page_offset = 0; disk_read_auto(1002, buf_dir);
                        _39(_192 _43 wait=0; wait<500000; wait++) ;
                        /// 3. FREIEN SLOT SUCHEN
                        CFS_DIR_ENTRY* entries = (CFS_DIR_ENTRY*)buf_dir;
                        int slot = -1;
                        _39(int i=0; i<28; i++) { _15(entries[i].type EQ 0) { slot = i; _37; } }
                        _15(slot NEQ -1) {
                            if (save_as_mode == 0) {
                                /// ==========================================
                                /// MODUS 0: NOTEPAD TEXT SPEICHERN
                                /// ==========================================
                                char* file_data = (char*)text_ram_addr;
                                _39(int i=0; i<5120; i++) file_data[i] = 0; 
                                _39(int i=0; i < windows[0].cursor_pos; i++) file_data[i] = windows[0].content[i];
                                
                                uint32_t target_sec = 4000 + slot;
                                disk_write_auto(target_sec, text_ram_addr);
                                _39(_192 _43 wait=0; wait<500000; wait++) ;
                                
                                entries[slot].file_size = 5120; 
                                entries[slot].start_lba = target_sec;
                            } else {
                                /// ==========================================
                                /// MODUS 1: BROWSER DOWNLOAD SPEICHERN
                                /// ==========================================
                                extern _184 browser_download_buffer[];
                                extern _43 browser_download_len;
                                
                                uint32_t payload_start = 0;
                                for(uint32_t i=0; i<browser_download_len-4; i++) {
                                    if (browser_download_buffer[i] == '\r' && browser_download_buffer[i+1] == '\n' &&
                                        browser_download_buffer[i+2] == '\r' && browser_download_buffer[i+3] == '\n') {
                                        payload_start = i + 4;
                                        break;
                                    }
                                }
                                
                                uint32_t file_size = browser_download_len - payload_start;
                                
                                /// Finde dynamisch einen freien Platz ganz hinten auf der Festplatte
                                uint32_t target_sec = 11000;
                                for (int i = 0; i < 28; i++) {
                                    if (cfs_files[i].exists && cfs_files[i].start_lba >= target_sec) {
                                        target_sec = cfs_files[i].start_lba + ((cfs_files[i].size + 511) / 512) + 1;
                                    }
                                }
                                
                                /// Die gesamte Datei Sektor für Sektor schreiben!
                                uint32_t sectors = (file_size + 511) / 512;
                                for (uint32_t s=0; s<sectors; s++) {
                                    disk_write_auto(target_sec + s, (uint64_t)&browser_download_buffer[payload_start + s*512]);
                                    for(volatile int w=0; w<2000; w++) ;
                                }
                                
                                entries[slot].file_size = file_size; 
                                entries[slot].start_lba = target_sec;
                            }
                            
                            /// ==========================================
                            /// GEMEINSAMER CODE (FÜR BEIDE MODI)
                            /// ==========================================
                            entries[slot].type = 1;
                            _39(int n=0; n<11; n++) entries[slot].filename[n] = 0;
                            str_cpy(entries[slot].filename, save_filename);
                            
                            /// Ordner-Zuweisung!
                            entries[slot].parent_idx = current_folder_id; 
                            
                            disk_write_auto(1002, buf_dir);
                            _39(_192 _43 wait2=0; wait2<500000; wait2++) ;
                            
                            /// LIVE UI UPDATE
                            cfs_files[slot].exists = 1;
                            cfs_files[slot].is_folder = 0;
                            cfs_files[slot].parent_idx = current_folder_id;
                            str_cpy(cfs_files[slot].name, save_filename);
                            cfs_files[slot].size = entries[slot].file_size;
                            cfs_files[slot].start_lba = entries[slot].start_lba;
                            
                            active_file_lba = entries[slot].start_lba;
                            active_file_idx = slot;
                            
                            str_cpy(windows[0].title, "SAVED SUCCESS!");
                            save_as_mode = 0; /// Nach dem Speichern wieder auf Standard setzen
                        } _41 {
                            str_cpy(windows[0].title, "ERR: ROOT FULL!");
                        }
                        win->open = _86; 
                        input_cooldown = 25; 
                    }
                } _41 _15(save_step EQ 2) { 
                    Text(wx+20, wy+40, "FOLDER NAME (MAX 11 CHARS):", 0x000000, _128); 
                    DrawRoundedRect(wx+20, wy+60, ww-40, 25, 2, 0xCCCCCC); 
                    Text(wx+25, wy+65, new_folder_name, 0x000000, _128);
                    _15((frame/20)%2 EQ 0) DrawChar(wx+25+(folder_name_idx*7), wy+65, '_', 0x000000, _128);
                    DrawRoundedRect(wx+ww-100, wy+wh-40, 80, 25, 5, 0x0055AA); 
                    TextC(wx+ww-60, wy+wh-32, "CREATE", 0xFFFFFF, _128);
                    _15(input_cooldown EQ 0 AND mouse_just_pressed AND is_active AND is_over_rect(mouse_x, mouse_y, wx+ww-100, wy+wh-40, 80, 25)) { 
                        /// 1. INHALTSVERZEICHNIS LADEN
                        simulator_page_offset = 0; disk_read_auto(1002, buf_dir);
                        _39(_192 _43 wait=0; wait<500000; wait++) ;
                        CFS_DIR_ENTRY* entries = (CFS_DIR_ENTRY*)buf_dir;
                        int slot = -1;
                        _39(int i=0; i<28; i++) { _15(entries[i].type EQ 0) { slot = i; _37; } }
                        _15(slot NEQ -1) {
                            /// 2. ORDNER-EINTRAG SCHREIBEN (V2!)
                            entries[slot].type = 2; /// 2 = FOLDER
                            _39(int n=0; n<11; n++) entries[slot].filename[n] = 0;
                            str_cpy(entries[slot].filename, new_folder_name);
                            entries[slot].file_size = 0;
                            entries[slot].start_lba = 0; 
                            
                            /// BARE METAL FIX: Ordner-Hierarchie auf Platte sichern!
                            entries[slot].parent_idx = current_folder_id; 
                            
                            disk_write_auto(1002, buf_dir);
                            _39(_192 _43 wait2=0; wait2<500000; wait2++) ;
                            
                            /// 3. UI UPDATE
                            cfs_files[slot].exists = 1;
                            cfs_files[slot].is_folder = 1;
                            cfs_files[slot].parent_idx = current_folder_id;
                            str_cpy(cfs_files[slot].name, new_folder_name);
                            cfs_files[slot].size = 0;
                            cfs_files[slot].start_lba = 0;
                            
                            str_cpy(windows[0].title, "FOLDER CREATED!");
                        } _41 {
                            str_cpy(windows[0].title, "ERR: ROOT FULL!");
                        }
                        win->open = _86; 
                        input_cooldown = 25; 
                    }
                }
            }
            /// ==========================================
            /// UNIVERSAL APP INSTALLER (ID 7)
            /// ==========================================
            _15(win->id EQ 7) {
                str_cpy(win->title, "APP STORE");
                
                Text(wx+15, wy+40, "SEARCH PACKAGE:", 0x00FFFF, _128);
                
                // Text Input Field
                DrawRoundedRect(wx+15, wy+60, win->w - 30, 25, 4, 0x222222);
                Text(wx+20, wy+65, app_installer_buf, 0xFFFFFF, _128);
                
                // Cursor blink
                if ((system_ticks / 500) % 2 == 0) {
                    int txt_len = 0; while(app_installer_buf[txt_len] != 0) txt_len++;
                    DrawRoundedRect(wx+20 + (txt_len * 8), wy+65, 8, 16, 0, 0xAAAAAA);
                }
                
                Text(wx+15, wy+100, "SEARCH ON PLATFORM:", 0xAAAAAA, _86);
                
                static int store_state = 0; // 0=Idle, 1=Search, 2=Installing
                static int store_platform = 0; 
                static char store_results[8][128] = {0};
                static int num_results = 0;
                static int search_timer = 0;

                // Linux Button
                _44 hov_lin = is_over_rect(mouse_x, mouse_y, wx+15, wy+120, 100, 30);
                DrawRoundedRect(wx+15, wy+120, 100, 30, 4, hov_lin ? 0x008800 : 0x005500);
                TextC(wx+65, wy+128, "LINUX (.deb)", 0xFFFFFF, _128);
                _15(input_cooldown == 0 && mouse_just_pressed && (win_z[19] == 7) && hov_lin && app_installer_buf[0] != 0) { 
                    input_cooldown = 20; play_sound(1000, 10);
                    char sys_cmd[512];
                    snprintf(sys_cmd, sizeof(sys_cmd), "apt-cache search %s | head -n 7 > /tmp/cosmos_store.txt &", app_installer_buf);
                    ::system(sys_cmd);
                    store_state = 1; store_platform = 1; search_timer = 0; num_results = 0;
                    for(int i=0; i<8; i++) store_results[i][0] = 0;
                }

                // Windows Button
                _44 hov_win = is_over_rect(mouse_x, mouse_y, wx+125, wy+120, 110, 30);
                DrawRoundedRect(wx+125, wy+120, 110, 30, 4, hov_win ? 0x0055AA : 0x003388);
                TextC(wx+180, wy+128, "WINDOWS (.exe)", 0xFFFFFF, _128);
                _15(input_cooldown == 0 && mouse_just_pressed && (win_z[19] == 7) && hov_win && app_installer_buf[0] != 0) { 
                    input_cooldown = 20; play_sound(1000, 10); 
                    char sys_cmd[512];
                    snprintf(sys_cmd, sizeof(sys_cmd), "echo '%s - Official Windows Setup' > /tmp/cosmos_store.txt &", app_installer_buf);
                    ::system(sys_cmd);
                    store_state = 1; store_platform = 2; search_timer = 0; num_results = 0;
                    for(int i=0; i<8; i++) store_results[i][0] = 0;
                }

                // Android Button
                _44 hov_and = is_over_rect(mouse_x, mouse_y, wx+245, wy+120, 100, 30);
                DrawRoundedRect(wx+245, wy+120, 100, 30, 4, hov_and ? 0xAA5500 : 0x883300);
                TextC(wx+295, wy+128, "ANDROID (.apk)", 0xFFFFFF, _128);
                _15(input_cooldown == 0 && mouse_just_pressed && (win_z[19] == 7) && hov_and && app_installer_buf[0] != 0) { 
                    input_cooldown = 20; play_sound(1000, 10);
                    char sys_cmd[512];
                    snprintf(sys_cmd, sizeof(sys_cmd), "echo '%s - Verified APK Package' > /tmp/cosmos_store.txt &", app_installer_buf);
                    ::system(sys_cmd);
                    store_state = 1; store_platform = 3; search_timer = 0; num_results = 0;
                    for(int i=0; i<8; i++) store_results[i][0] = 0;
                }

                // Box background
                DrawRoundedRect(wx+15, wy+165, win->w - 30, 110, 4, 0x111111);
                
                if (store_state == 1) { // SEARCH RESULTS
                    Text(wx+20, wy+175, "SEARCH RESULTS (Click to install):", 0x00FF00, _86);
                    
                    if (search_timer++ % 20 == 0) {
                        FILE* f = fopen("/tmp/cosmos_store.txt", "r");
                        if (f) {
                            num_results = 0;
                            char line_buf[256];
                            while(num_results < 7 && fgets(line_buf, sizeof(line_buf), f)) {
                                for(int i=0; i<256; i++) { if(line_buf[i] == '\n' || line_buf[i] == '\r') line_buf[i] = 0; }
                                if (line_buf[0] != 0) {
                                    strncpy(store_results[num_results], line_buf, 127);
                                    num_results++;
                                }
                            }
                            fclose(f);
                        }
                    }
                    
                    if (num_results > 0) {
                        for(int i=0; i<num_results; i++) {
                            int ry = wy + 190 + (i * 13);
                            bool r_hov = is_over_rect(mouse_x, mouse_y, wx+15, ry, win->w - 30, 13);
                            if (r_hov) DrawRoundedRect(wx+15, ry, win->w - 30, 13, 0, 0x333333);
                            
                            char disp[80]; strncpy(disp, store_results[i], 79); disp[79] = 0;
                            Text(wx+20, ry+2, disp, r_hov ? 0xFFFFFF : 0xAAAAAA, _86);
                            
                            if (r_hov && mouse_just_pressed && input_cooldown == 0) {
                                input_cooldown = 20; play_sound(1000, 10);
                                
                                char pkg[128] = {0};
                                int s=0; while(store_results[i][s] != ' ' && store_results[i][s] != 0 && s < 127) { pkg[s] = store_results[i][s]; s++; }
                                
                                char sys_cmd[512];
                                if (store_platform == 1) {
                                    snprintf(sys_cmd, sizeof(sys_cmd), "(echo '[LINUX] Installing %s...' && apt-get install -y %s && echo '[OK] Installed!') > /tmp/cosmos_install.log 2>&1 &", pkg, pkg);
                                } else if (store_platform == 2) {
                                    snprintf(sys_cmd, sizeof(sys_cmd), "(echo '[WINDOWS] Installing %s via Winget...' && cmd.exe /c winget install %s --accept-package-agreements --accept-source-agreements && echo '[OK] Installed!') > /tmp/cosmos_install.log 2>&1 &", pkg, pkg);
                                } else if (store_platform == 3) {
                                    snprintf(sys_cmd, sizeof(sys_cmd), "(echo '[ANDROID] Pushing APK %s...' && adb shell cmd package install %s || echo '[ERROR] ADB failed.') > /tmp/cosmos_install.log 2>&1 &", pkg, pkg);
                                }
                                ::system(sys_cmd);
                                
                                store_state = 2; // Switch to installation log view
                                ::system("echo 'Initializing installation...' > /tmp/cosmos_install.log");
                            }
                        }
                    } else {
                        Text(wx+25, wy+190, "Loading results...", 0x888888, _86);
                    }
                } else if (store_state == 2 || store_state == 0) { // INSTALLATION LOG
                    Text(wx+20, wy+175, "LIVE INSTALLATION LOG:", 0x888888, _86);
                    
                    static char log_lines[6][128] = {0};
                    static int log_timer = 0;
                    static bool has_log = false;
                    
                    if (log_timer++ > 50) {
                        log_timer = 0;
                        FILE* log_f = fopen("/tmp/cosmos_install.log", "r");
                        if (log_f) {
                            has_log = true;
                            int line_idx = 0;
                            char line_buf[256];
                            while(fgets(line_buf, sizeof(line_buf), log_f)) {
                                for(int i=0; i<256; i++) { if(line_buf[i] == '\n' || line_buf[i] == '\r') line_buf[i] = 0; }
                                for(int i=0; i<5; i++) strncpy(log_lines[i], log_lines[i+1], 127);
                                strncpy(log_lines[5], line_buf, 127);
                                line_idx++;
                            }
                            fclose(log_f);
                        } else {
                            has_log = false;
                        }
                    }
                    
                    if (has_log) {
                        for(int i=0; i<6; i++) {
                            if (log_lines[i][0] != 0) {
                                Text(wx+25, wy+190 + (i*13), log_lines[i], 0xAAAAAA, _86);
                            }
                        }
                    } else {
                        Text(wx+25, wy+190, "No installation task running.", 0x555555, _86);
                    }
                }
            }
			
            /// ==========================================
            /// CMD (ID 5) - BARE METAL FIX (SICHTBAR!)
            /// ==========================================
            _15(win->id EQ 5) {
                _89 cmd_color = 0xAAAAAA; /// Hacker-Grün
                Text(wx+15, wy+45, win->content, cmd_color, _86);
                
                _43 lines = 0; _39(_43 i=0; i<win->cursor_pos; i++) { if(win->content[i] == '\n') lines++; }
                _43 prompt_y = wy + 45 + (lines * 15);
                
                Text(wx+15, prompt_y, "C:\\> ", cmd_color, _128);
                Text(wx+55, prompt_y, cmd_input_buf, cmd_color, _128);
                
            }
        }        
        /// BARE METAL FIX: Tastatur-Ereignisse nach einem Frame verwerfen!
        key_new = _86;
        last_app_key = 0;
        mouse_just_pressed = _86;
		DrawAeroCursor(mouse_x, mouse_y);
        
        Swap(); 
#ifdef __linux__
        // usleep(1000) drosselt die CPU-Last von 100% auf ca. 2-5%
        // Verhindert, dass CosmOS den gesamten Computer blockiert!
        // Der FPS-Wert bleibt so bei den gewuenschten 100-150 FPS.
        usleep(1000);
#endif
        frame++;
    }
}
