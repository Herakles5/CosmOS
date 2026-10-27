/*
 * ============================================================================
 *  COSMOS VMX ASSEMBLY - Low-Level VMX Instruction Wrappers
 * ============================================================================
 *
 *  Diese Datei enthält die Assembly-Routinen für VMX-Befehle, die nicht
 *  direkt aus C/C++ aufgerufen werden können. Alle Funktionen folgen der
 *  System V AMD64 ABI (Linux/CosmOS Calling Convention).
 *
 *  Calling Convention:
 *    RDI = 1. Argument
 *    RSI = 2. Argument
 *    RDX = 3. Argument
 *    RCX = 4. Argument
 *    RAX = Return Value
 *
 *  VMX Instruction Status:
 *    CF=1: VMfailInvalid (kein aktiver VMCS)
 *    ZF=1: VMfailValid   (Fehlercode in VM-instruction error field)
 *    Beide 0: Erfolg
 * ============================================================================
 */

.intel_syntax noprefix
.text

/* ============================================================================
 *  cosmos_vmxon - Aktiviere VMX Root Operation
 *
 *  uint8_t cosmos_vmxon(uint64_t* vmxon_region_phys_ptr)
 *
 *  RDI = Pointer auf 64-bit physische Adresse der VMXON-Region
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmxon
cosmos_vmxon:
    vmxon [rdi]
    jbe .vmxon_fail          # Jump if CF=1 OR ZF=1
    xor eax, eax             # Return 0 (Erfolg)
    ret
.vmxon_fail:
    mov eax, 1               # Return 1 (Fehler)
    ret

/* ============================================================================
 *  cosmos_vmxoff - Deaktiviere VMX Root Operation
 *
 *  uint8_t cosmos_vmxoff(void)
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmxoff
cosmos_vmxoff:
    vmxoff
    jbe .vmxoff_fail
    xor eax, eax
    ret
.vmxoff_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmptrld - Lade VMCS als aktiv
 *
 *  uint8_t cosmos_vmptrld(uint64_t* vmcs_phys_ptr)
 *  RDI = Pointer auf 64-bit physische Adresse des VMCS
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmptrld
cosmos_vmptrld:
    vmptrld [rdi]
    jbe .vmptrld_fail
    xor eax, eax
    ret
.vmptrld_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmclear - VMCS zurücksetzen und als inaktiv markieren
 *
 *  uint8_t cosmos_vmclear(uint64_t* vmcs_phys_ptr)
 *  RDI = Pointer auf 64-bit physische Adresse des VMCS
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmclear
cosmos_vmclear:
    vmclear [rdi]
    jbe .vmclear_fail
    xor eax, eax
    ret
.vmclear_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmread - Lese ein Feld aus dem aktiven VMCS
 *
 *  uint8_t cosmos_vmread(uint64_t field, uint64_t* value)
 *  RDI = VMCS Field Encoding
 *  RSI = Pointer auf 64-bit Ziel für den gelesenen Wert
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmread
cosmos_vmread:
    vmread [rsi], rdi
    jbe .vmread_fail
    xor eax, eax
    ret
.vmread_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmwrite - Schreibe einen Wert in ein VMCS-Feld
 *
 *  uint8_t cosmos_vmwrite(uint64_t field, uint64_t value)
 *  RDI = VMCS Field Encoding
 *  RSI = Wert zum Schreiben
 *  Returns: 0 = Erfolg, 1 = Fehler
 * ============================================================================ */
.global cosmos_vmwrite
cosmos_vmwrite:
    vmwrite rdi, rsi
    jbe .vmwrite_fail
    xor eax, eax
    ret
.vmwrite_fail:
    mov eax, 1
    ret

/* ============================================================================
 *  cosmos_vmlaunch_trampoline - Starte den Gast zum ersten Mal
 *
 *  void cosmos_vmlaunch_trampoline(vmx_guest_regs* guest_regs)
 *
 *  RDI = Pointer auf vmx_guest_regs Struktur
 *
 *  Diese Funktion:
 *  1. Sichert alle Host-Register auf dem Stack
 *  2. Lädt alle Gast-Register aus der Struktur
 *  3. Führt VMLAUNCH aus
 *  4. Bei VM-Exit landen wir automatisch bei cosmos_vmexit_stub
 *
 *  WICHTIG: Der Host RSP wird im VMCS gespeichert und zeigt auf
 *  cosmos_vmexit_stub. Bei jedem VM-Exit springt die CPU dorthin.
 * ============================================================================ */
.global cosmos_vmlaunch_trampoline
cosmos_vmlaunch_trampoline:
    # Host-State sichern (callee-saved nach System V ABI)
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    # RDI (guest_regs Pointer) in R15 sichern (callee-saved)
    mov r15, rdi

    # Alle Gast-GPRs aus der Struktur laden
    # Reihenfolge: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8-r15
    mov rax, [r15 + 0x00]    # guest_regs->rax
    mov rbx, [r15 + 0x08]    # guest_regs->rbx
    mov rcx, [r15 + 0x10]    # guest_regs->rcx
    mov rdx, [r15 + 0x18]    # guest_regs->rdx
    mov rsi, [r15 + 0x20]    # guest_regs->rsi
    mov rdi, [r15 + 0x28]    # guest_regs->rdi
    mov rbp, [r15 + 0x30]    # guest_regs->rbp
    mov r8,  [r15 + 0x38]    # guest_regs->r8
    mov r9,  [r15 + 0x40]    # guest_regs->r9
    mov r10, [r15 + 0x48]    # guest_regs->r10
    mov r11, [r15 + 0x50]    # guest_regs->r11
    mov r12, [r15 + 0x58]    # guest_regs->r12
    mov r13, [r15 + 0x60]    # guest_regs->r13
    mov r14, [r15 + 0x68]    # guest_regs->r14
    # R15 selbst laden wir ZULETZT, weil wir es als Pointer benutzen
    mov r15, [r15 + 0x70]    # guest_regs->r15

    # VMLAUNCH! Ab hier läuft der Gast.
    # Bei Erfolg kehrt diese Instruktion NICHT zurück.
    # Bei Fehler fällt sie durch zur nächsten Instruktion.
    vmlaunch

    # Wenn wir hier landen, ist VMLAUNCH fehlgeschlagen!
    # Fehlerbehandlung: Host-State wiederherstellen und zurückkehren
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

/* ============================================================================
 *  cosmos_vmresume_trampoline - Setze den Gast fort (nach VM-Exit)
 *
 *  void cosmos_vmresume_trampoline(vmx_guest_regs* guest_regs)
 *  Identisch zu vmlaunch_trampoline, aber benutzt VMRESUME statt VMLAUNCH.
 * ============================================================================ */
.global cosmos_vmresume_trampoline
cosmos_vmresume_trampoline:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r15, rdi

    mov rax, [r15 + 0x00]
    mov rbx, [r15 + 0x08]
    mov rcx, [r15 + 0x10]
    mov rdx, [r15 + 0x18]
    mov rsi, [r15 + 0x20]
    mov rdi, [r15 + 0x28]
    mov rbp, [r15 + 0x30]
    mov r8,  [r15 + 0x38]
    mov r9,  [r15 + 0x40]
    mov r10, [r15 + 0x48]
    mov r11, [r15 + 0x50]
    mov r12, [r15 + 0x58]
    mov r13, [r15 + 0x60]
    mov r14, [r15 + 0x68]
    mov r15, [r15 + 0x70]

    vmresume

    # VMRESUME fehlgeschlagen
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

/* ============================================================================
 *  cosmos_vmexit_stub - VM-Exit Entry Point
 *
 *  HIER landet die CPU bei JEDEM VM-Exit!
 *
 *  Der VMCS Host-RIP zeigt auf diese Adresse.
 *  Der VMCS Host-RSP zeigt auf einen vorbereiteten Stack.
 *
 *  Ablauf:
 *  1. Alle Gast-Register auf den Stack sichern (die CPU stellt nur RSP/RIP her)
 *  2. C++ Handler aufrufen: cosmos_vmexit_handler(vmx_guest_regs*)
 *  3. Gast-Register wiederherstellen
 *  4. VMRESUME ausführen
 * ============================================================================ */
.global cosmos_vmexit_stub
cosmos_vmexit_stub:
    # Gast-Register auf dem Stack sichern (in umgekehrter Reihenfolge!)
    # Wir bauen eine vmx_guest_regs Struktur auf dem Stack auf.
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    # RSP zeigt jetzt auf die vmx_guest_regs Struktur
    mov rdi, rsp              # 1. Argument für cosmos_vmexit_handler

    # Stack auf 16 Bytes alignen (System V ABI)
    # RSP hat 15 * 8 = 120 Bytes gepusht → RSP ist 8-byte aligned
    # Wir brauchen 16-byte alignment für den CALL
    sub rsp, 8

    # C++ VM-Exit Handler aufrufen
    call cosmos_vmexit_handler

    # Stack Alignment rückgängig machen
    add rsp, 8

    # RAX enthält jetzt den Return-Wert von cosmos_vmexit_handler (0 = resume, 1 = abort)
    test eax, eax
    jz .do_resume
    
    # Abort requested! Wir lösen einen Kernel Panic aus (Invalid Opcode).
    # Das stoppt den stillen Freeze und druckt einen Trace ins dmesg!
    ud2

.do_resume:
    # Gast-Register wiederherstellen
    # Hinweis: guest_regs auf dem Stack wurden evtl. von C++ überschrieben
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    # Zurück in den Gast! VMRESUME setzt die Ausführung fort.
    vmresume

    # Wenn VMRESUME fehlschlägt, landen wir hier.
    # In diesem Fall: HLT loop (Notfall-Stop)
    cli
.vmexit_halt_loop:
    hlt
    jmp .vmexit_halt_loop

/* ============================================================================
 *  cosmos_invept - Invalidate EPT Translations
 *
 *  void cosmos_invept(uint64_t type, uint64_t eptp)
 *  RDI = Invalidation Type (1=single-context, 2=global)
 *  RSI = EPTP (Extended Page Table Pointer)
 * ============================================================================ */
.global cosmos_invept
cosmos_invept:
    # INVEPT erwartet: Typ in Register, 128-bit Deskriptor in Memory
    # Deskriptor: [EPTP (64-bit)] [Reserved (64-bit)]
    sub rsp, 16
    mov [rsp], rsi            # EPTP
    mov qword ptr [rsp + 8], 0  # Reserved
    invept rdi, [rsp]
    add rsp, 16
    ret

/* ============================================================================
 *  cosmos_invvpid - Invalidate VPID Translations
 *
 *  void cosmos_invvpid(uint64_t type, uint64_t vpid, uint64_t linear_addr)
 *  RDI = Invalidation Type
 *  RSI = VPID
 *  RDX = Linear Address
 * ============================================================================ */
.global cosmos_invvpid
cosmos_invvpid:
    sub rsp, 16
    mov [rsp], rsi            # VPID
    mov [rsp + 8], rdx        # Linear Address
    invvpid rdi, [rsp]
    add rsp, 16
    ret
