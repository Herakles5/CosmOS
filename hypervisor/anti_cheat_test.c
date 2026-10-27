#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

// Helper to execute CPUID
static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf), "c" (0)
    );
}

int main() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("==========================================\n");
    printf("   CosmOS Anti-Cheat Emulator & Tester    \n");
    printf("==========================================\n\n");
    
    // --- 1. Hypervisor Present Bit Check ---
    printf("[*] Test 1: CPUID.01H (Hypervisor Present Bit)\n");
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (ecx & (1 << 31)) {
        printf("    [!] DANGER: Hypervisor Present Bit is SET! (Virtualization detected)\n");
    } else {
        printf("    [+] OK: Hypervisor Present Bit is CLEAR.\n");
    }
    printf("\n");
    
    // --- 2. Hypervisor Signature Check ---
    printf("[*] Test 2: CPUID.40000000H (Hypervisor Signature)\n");
    cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
    
    if (eax != 0 || ebx != 0 || ecx != 0 || edx != 0) {
        char sig[13];
        *(uint32_t*)&sig[0] = ebx;
        *(uint32_t*)&sig[4] = ecx;
        *(uint32_t*)&sig[8] = edx;
        sig[12] = '\0';
        printf("    [!] DANGER: Hypervisor Signature found: '%s' (eax=0x%x)\n", sig, eax);
    } else {
        printf("    [+] OK: No Hypervisor Signature detected.\n");
    }
    printf("\n");
    
    // --- 3. RDTSC Timing Check ---
    // Anti-cheats measure how long a CPUID instruction takes.
    // Normally CPUID takes ~100-250 cycles. 
    // If a hypervisor intercepts it, a VM-Exit takes ~1500-3000 cycles.
    printf("[*] Test 3: RDTSC Timing Analysis (CPUID VM-Exit Cost)\n");
    
    // Warmup
    cpuid(0, &eax, &ebx, &ecx, &edx);
    cpuid(0, &eax, &ebx, &ecx, &edx);
    
    uint64_t tsc1 = __rdtsc();
    cpuid(0, &eax, &ebx, &ecx, &edx);
    uint64_t tsc2 = __rdtsc();
    
    uint64_t diff = tsc2 - tsc1;
    printf("    CPUID instruction took: %lu cycles\n", diff);
    
    if (diff > 1000) {
        printf("    [!] DANGER: CPUID took too long! High probability of a Hypervisor (VM-Exit detected).\n");
    } else {
        printf("    [+] OK: CPUID executed natively fast. No Hypervisor timing anomaly detected.\n");
    }
    printf("\n");
    
    printf("==========================================\n");
    printf("Tests complete.\n");
    
    return 0;
}
