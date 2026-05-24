; Stage 2 Bootloader.
; Starts in 16-bit real mode at 0x8000.
; Checks CPU support for 64-bit long mode, sets up 4-level paging,
; and switches from 16-bit -> 32-bit protected mode -> 64-bit long mode.
; Loads the kernel from disk sector 10 to physical address 0x100000 (1MB).
; 80 sectors = 40 KB reserved for the kernel binary.

[bits 16]
[org 0x8000]

entry_stage2:
    mov [boot_drive], dl        ; save boot drive number passed from stage 1

    mov si, msg_stage2_start
    call print_string_16

    ; load kernel sectors from disk before turning off BIOS interrupts
    call load_kernel

    ; verify CPU is modern enough to run 64-bit code
    call check_cpuid
    jc .err_no_cpuid
    call check_long_mode
    jc .err_no_long_mode

    mov si, msg_entering_pm
    call print_string_16

    cli                         ; disable interrupts, no turning back
    lgdt [gdt_descriptor_32]    ; load temporary 32-bit GDT

    ; set protection enable (PE) bit in cr0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; far jump to clear prefetch cache and enter 32-bit protected mode
    jmp CODE_SEG_32:entry_protected_mode

.err_no_cpuid:
    mov si, msg_err_cpuid
    call print_string_16
    cli
    hlt

.err_no_long_mode:
    mov si, msg_err_long_mode
    call print_string_16
    cli
    hlt

print_string_16:
    push ax
    push bx
    mov ah, 0x0e
    xor bh, bh
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

check_cpuid:
    ; check if CPUID is supported by trying to toggle the ID flag (bit 21) in FLAGS
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_support
    clc
    ret
.no_support:
    stc
    ret

check_long_mode:
    ; check if extended functions are supported by CPUID
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; check if long mode (LM-bit, bit 29) is supported
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    clc
    ret
.no_long_mode:
    stc
    ret

load_kernel:
    mov si, msg_loading_kernel
    call print_string_16
    
    mov byte [kernel_retry], 3
.try_read:
    mov ax, 0x1000              ; load to segment 0x1000 (physical address 0x10000)
    mov es, ax
    xor bx, bx                  ; offset 0

    mov ah, 0x02                ; BIOS read sectors function
    mov al, 120                 ; read 120 sectors (60KB kernel)
    mov ch, 0                   ; cylinder 0
    mov dh, 0                   ; head 0
    mov cl, 10                  ; kernel starts at sector 10
    mov dl, [boot_drive]
    int 0x13
    jnc .read_success

    ; reset disk drive and retry
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13

    dec byte [kernel_retry]
    jnz .try_read

    mov si, msg_err_kernel_read
    call print_string_16
    cli
    hlt

.read_success:
    mov si, msg_kernel_loaded_16
    call print_string_16
    ret


[bits 32]
entry_protected_mode:
    ; load 32-bit data selector into segment registers
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; temporary 32-bit stack (0x90000 is safe conventional memory)
    mov esp, 0x90000

    ; zero out and build page tables at fixed addresses: PML4=0x1000, PDPT=0x2000, PD=0x3000
    call setup_paging

    ; load PML4 address into cr3 register
    mov eax, 0x1000
    mov cr3, eax

    ; enable Physical Address Extension (PAE) in cr4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; set Long Mode Enable (LME) bit in EFER MSR (0xC0000080)
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable Paging (PG) and Write Protect (WP) in cr0
    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)
    mov cr0, eax

    ; load 64-bit GDT and make the final jump to 64-bit long mode
    lgdt [gdt_descriptor_64]
    jmp CODE_SEG_64:entry_long_mode

setup_paging:
    ; clear page table space (0x1000 to 0x3FFF, 3 pages = 12KB)
    mov edi, 0x1000
    mov ecx, 3072
    xor eax, eax
    rep stosd

    ; link PML4[0] -> PDPT (0x2000 | present | read-write)
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0x0000

    ; link PDPT[0] -> PD (0x3000 | present | read-write)
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0x0000

    ; identity map first 1GB of physical RAM using 2MB huge pages (no PT needed)
    ; PD entry = (i * 2MB) | present | read-write | huge-page (0x83)
    mov edi, 0x3000
    mov eax, 0x00000083
    mov ecx, 512

.loop_map:
    mov [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    add eax, 0x200000           ; add 2MB to base address
    loop .loop_map

    ret


[bits 64]
entry_long_mode:
    ; clear data segments in 64-bit mode
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; setup stable 64-bit stack pointer at 0x90000
    mov rsp, 0x90000

    ; copy kernel from temp buffer 0x10000 to final physical address 0x100000 (1MB)
    ; kernel.bin is ~33 KB (80 sectors reserved): copy 8192 QWORDs = 64 KB
    ; to cover .text (0x100000) + .rodata (0x108000) + .data (0x10C000) with margin.
    mov rsi, 0x10000
    mov rdi, 0x100000
    mov rcx, 8192       ; 8192 * 8 = 65536 bytes (64 KB)
    rep movsq

    ; draw bootloader dashboard directly to VGA memory (0xB8000)
    call clear_screen_64
    call display_system_info_64

    ; hand off to the kernel — jump to physical address 0x100000 (_start in kernel_entry.asm)
    mov     rax, 0x100000
    jmp     rax

clear_screen_64:
    mov rdi, 0xb8000
    mov rcx, 80 * 25
    mov ax, 0x1f20              ; space character with white text on deep blue background
    rep stosw
    ret

print_string_64:
.loop:
    lodsb
    test al, al
    jz .done
    mov [rdi], ax
    add rdi, 2
    jmp .loop
.done:
    ret

display_system_info_64:
    ; header row
    mov rdi, 0xb8000
    mov rsi, msg_ui_title
    mov ah, 0x70
    call print_string_64

    ; CPU status
    mov rdi, 0xb8000 + 2 * (80 * 2 + 2)
    mov rsi, msg_ui_cpu_ok
    mov ah, 0x1a
    call print_string_64

    ; Paging status
    mov rdi, 0xb8000 + 2 * (80 * 3 + 2)
    mov rsi, msg_ui_paging_ok
    mov ah, 0x1a
    call print_string_64

    ; GDT status
    mov rdi, 0xb8000 + 2 * (80 * 4 + 2)
    mov rsi, msg_ui_gdt_ok
    mov ah, 0x1a
    call print_string_64

    ; final success status
    mov rdi, 0xb8000 + 2 * (80 * 6 + 2)
    mov rsi, msg_ui_success
    mov ah, 0x2f
    call print_string_64

    ; kernel state status
    mov rdi, 0xb8000 + 2 * (80 * 8 + 2)
    mov rsi, msg_ui_halted
    mov ah, 0x1e
    call print_string_64

    ret


boot_drive:         db 0
kernel_retry:       db 0

msg_stage2_start:      db "ReeOS Stage 2: Initializing hardware...", 0x0d, 0x0a, 0
msg_loading_kernel:    db "Reading 64-bit Kernel sectors from disk...", 0x0d, 0x0a, 0
msg_kernel_loaded_16:  db "Kernel sectors loaded to RAM buffer at 0x10000.", 0x0d, 0x0a, 0
msg_entering_pm:       db "Entering 32-bit Protected Mode...", 0x0d, 0x0a, 0

msg_err_cpuid:         db "FATAL: CPUID not supported. System halted.", 0x0d, 0x0a, 0
msg_err_long_mode:     db "FATAL: x86_64 Long Mode not supported by this CPU. System halted.", 0x0d, 0x0a, 0
msg_err_kernel_read:   db "FATAL: Kernel disk read error. System halted.", 0x0d, 0x0a, 0

msg_ui_title:      db "  === ReeOS Kernel Loader v1.0.0 (x86_64 Long Mode Bootloader) ===            ", 0
msg_ui_cpu_ok:     db "[ OK ] CPU Capability verified: AMD64/Intel64 Long Mode instruction set active.", 0
msg_ui_paging_ok:  db "[ OK ] 4-Level Paging: 1 GB of Physical RAM identity-mapped (PML4 -> PDPT -> PD).", 0
msg_ui_gdt_ok:     db "[ OK ] 64-bit Global Descriptor Table loaded (CS Selector 0x08, SS/DS 0x00).", 0
msg_ui_success:    db " SUCCESS: Kernel loaded at physical address: 0x0000000000100000! ", 0
msg_ui_halted:     db "[ INFO ] Jumping to kernel entry point at 0x100000...", 0

%include "gdt.asm"
