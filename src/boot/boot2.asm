; =============================================================================
;  ReeOS - Stage 2 Bootloader (Unified Version with Integrated GDT)
;  Start in 16-bit Real Mode at physical address 0x8000
; =============================================================================

[bits 16]
[org 0x8000]

entry_stage2:
    cli                         ; Disable interrupts
    xor ax, ax                  ; Clean segment registers
    mov ds, ax
    mov es, ax
    
    ; Setup a secure temporary stack (0x7C00)
    mov ss, ax
    mov sp, 0x7c00
    sti                         ; Re-enable interrupts for disk functions

    mov [boot_drive], dl        ; Save the boot drive

    mov si, msg_stage2_start
    call print_string_16

    ; Load kernel sectors into RAM
    call load_kernel

    ; Processor hardware verification
    call check_cpuid
    jc .err_no_cpuid
    call check_long_mode
    jc .err_no_long_mode

    mov si, msg_entering_pm
    call print_string_16

    cli                         ; Permanent shutdown of BIOS interrupts
    lgdt [gdt_descriptor_32]    ; Load the internal 32-bit GDT

    ; Activate protected mode (PE bit of CR0 = 1)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to clear CPU instruction pipeline and switch to 32-bit
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

; --- 16-bit Utility Functions ---
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
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

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
    mov ah, 0x42                ; BIOS Extension INT 0x13
    mov dl, [boot_drive]
    mov si, DAP_struct          ; DS:SI points to our DAP structure
    int 0x13
    jnc .read_success

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

; =============================================================================
;  32-BIT PROTECTED MODE
; =============================================================================
[bits 32]
entry_protected_mode:
    ; Initialize 32-bit data segment registers
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000            ; Temporary 32-bit stack

    call setup_paging

    ; Load PML4 (0x1000) into CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Enable PAE in CR4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode (LME) in EFER MSR
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging (PG) and write protection (WP)
    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)
    mov cr0, eax

    lgdt [gdt_descriptor_64]    ; Load internal 64-bit GDT
    
    ; Jump to 64-bit code
    jmp CODE_SEG_64:entry_long_mode

setup_paging:
    mov edi, 0x1000
    mov ecx, 3072
    xor eax, eax
    rep stosd                   ; Clear page table memory (12 KB)

    ; Page tables setup (1 GB Identity mapping)
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0x0000

    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0x0000

    mov edi, 0x3000
    mov eax, 0x00000083
    mov ecx, 512
.loop_map:
    mov [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    add eax, 0x200000
    loop .loop_map
    ret

; =============================================================================
;  64-BIT LONG MODE
; =============================================================================
[bits 64]
entry_long_mode:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ax, DATA_SEG_64

    mov rsp, 0x90000            ; Set up 64-bit stack pointer

    ; Copy the kernel to its final physical destination (1 MB)
    cld
    mov rsi, 0x10000            ; Source (Temporary buffer)
    mov rdi, 0x100000           ; Destination (1 MB)
    mov rcx, 8192               ; 64 KB
    rep movsq

    call clear_screen_64
    call display_system_info_64

    ; Jump to the Kernel entry point!
    mov rax, 0x100000
    jmp rax

clear_screen_64:
    mov rdi, 0xb8000
    mov rcx, 80 * 25
    mov ax, 0x1f20
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
    mov rdi, 0xb8000
    mov rsi, msg_ui_title
    mov ah, 0x70
    call print_string_64

    mov rdi, 0xb8000 + 2 * (80 * 2 + 2)
    mov rsi, msg_ui_cpu_ok
    mov ah, 0x1a
    call print_string_64

    mov rdi, 0xb8000 + 2 * (80 * 3 + 2)
    mov rsi, msg_ui_paging_ok
    mov ah, 0x1a
    call print_string_64

    mov rdi, 0xb8000 + 2 * (80 * 4 + 2)
    mov rsi, msg_ui_gdt_ok
    mov ah, 0x1a
    call print_string_64

    mov rdi, 0xb8000 + 2 * (80 * 6 + 2)
    mov rsi, msg_ui_success
    mov ah, 0x2f
    call print_string_64

    mov rdi, 0xb8000 + 2 * (80 * 8 + 2)
    mov rsi, msg_ui_halted
    mov ah, 0x1e
    call print_string_64
    ret

; =============================================================================
;  INTERNAL ALIGNED GDT TABLES
; =============================================================================

align 4
gdt_start_32:
    dd 0, 0                     ; Mandatory null descriptor
gdt_code_32:
    dw 0xffff, 0x0000
    db 0x00, 0x9a, 0xcf, 0x00   ; 32-bit Code Descriptor, Base=0, Limit=4GB
gdt_data_32:
    dw 0xffff, 0x0000
    db 0x00, 0x92, 0xcf, 0x00   ; 32-bit Data Descriptor, Base=0, Limit=4GB
gdt_end_32:

gdt_descriptor_32:
    dw gdt_end_32 - gdt_start_32 - 1
    dd gdt_start_32

CODE_SEG_32 equ gdt_code_32 - gdt_start_32
DATA_SEG_32 equ gdt_data_32 - gdt_start_32

align 8
gdt_start_64:
    dq 0                        ; Mandatory null descriptor
gdt_code_64:
    dw 0x0000, 0x0000
    db 0x00, 0x9a, 0x20, 0x00   ; 64-bit Code Descriptor (L-bit set to 1)
gdt_data_64:
    dw 0x0000, 0x0000
    db 0x00, 0x92, 0x00, 0x00   ; 64-bit Data Descriptor
gdt_end_64:

gdt_descriptor_64:
    dw gdt_end_64 - gdt_start_64 - 1
    dd gdt_start_64

CODE_SEG_64 equ gdt_code_64 - gdt_start_64
DATA_SEG_64 equ gdt_data_64 - gdt_start_64

; =============================================================================
;  DATA STRUCTURES & DISK VARIABLES
; =============================================================================
boot_drive:         db 0
kernel_retry:       db 0

align 4
DAP_struct:
    db 0x10                     ; Structure size (16 bytes)
    db 0x00                     ; Reserved
    dw 120                      ; Number of sectors to read (60 KB)
    dw 0x0000                   ; Destination offset
    dw 0x1000                   ; Destination segment (0x1000 -> 0x10000 physical)
    dq 9                        ; Kernel positioned at LBA index 9

; --- String Constants ---
msg_stage2_start:      db "ReeOS Stage 2: Initializing hardware...", 0x0d, 0x0a, 0
msg_loading_kernel:    db "Reading 64-bit Kernel sectors from disk...", 0x0d, 0x0a, 0
msg_kernel_loaded_16:  db "Kernel sectors loaded to RAM buffer at 0x10000.", 0x0d, 0x0a, 0
msg_entering_pm:       db "Entering 32-bit Protected Mode...", 0x0d, 0x0a, 0

msg_err_cpuid:         db "FATAL: CPUID not supported. System halted.", 0x0d, 0x0a, 0
msg_err_long_mode:     db "FATAL: x86_64 Long Mode not supported. System halted.", 0x0d, 0x0a, 0
msg_err_kernel_read:   db "FATAL: Kernel disk read error. System halted.", 0x0d, 0x0a, 0

msg_ui_title:      db "   === ReeOS Kernel Loader v1.0.0 (x86_64 Long Mode Bootloader) ===             ", 0
msg_ui_cpu_ok:     db "[ OK ] CPU Capability verified: AMD64/Intel64 Long Mode active.", 0
msg_ui_paging_ok:  db "[ OK ] 4-Level Paging: 1 GB of Physical RAM identity-mapped.", 0
msg_ui_gdt_ok:     db "[ OK ] 64-bit Global Descriptor Table loaded.", 0
msg_ui_success:    db " SUCCESS: Kernel loaded at physical address: 0x0000000000100000! ", 0
msg_ui_halted:     db "[ INFO ] Jumping to kernel entry point at 0x100000...", 0