; Stage 1 Boot Sector.
; Loaded by BIOS at 0x7C00 in 16-bit real mode.
; Sets up a safe stack, loads Stage 2 from disk to 0x8000, and jumps there.

[bits 16]
[org 0x7c00]

STAGE2_LOAD_SEG  equ 0x0000
STAGE2_LOAD_OFS  equ 0x8000
STAGE2_SECTOR    equ 2          ; Stage 2 starts at sector 2 on the drive
STAGE2_SEC_COUNT equ 8          ; Load 8 sectors (4KB), plenty of room for stage 2

start:
    cli                         ; disable interrupts while configuring registers

    ; zero out segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; stack grows down from 0x7C00, safe conventional memory
    mov ss, ax
    mov sp, 0x7c00

    cld                         ; clear direction flag to read strings forward
    sti                         ; stack is safe, re-enable interrupts

    mov [boot_drive], dl        ; BIOS passes boot drive in DL, save it

    mov si, msg_booting
    call print_string

    call load_stage2

    mov si, msg_jump_stage2
    call print_string

    ; jump to stage 2 at 0x0000:0x8000
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFS

load_stage2:
    mov byte [retry_count], 3   ; retry up to 3 times in case of cheap floppy errors

.try_read:
    mov ax, STAGE2_LOAD_SEG
    mov es, ax
    mov bx, STAGE2_LOAD_OFS

    mov ah, 0x02                ; BIOS read sectors function
    mov al, STAGE2_SEC_COUNT
    mov ch, 0                   ; cylinder 0
    mov dh, 0                   ; head 0
    mov cl, STAGE2_SECTOR       ; sector 2
    mov dl, [boot_drive]
    int 0x13
    jnc .read_success           ; carry flag clear means success

    ; reset disk drive and try again
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13

    dec byte [retry_count]
    jnz .try_read

    ; if we got here, we're screwed
    mov si, msg_err_read
    call print_string
    cli
    hlt

.read_success:
    ret

print_string:
    push ax
    push bx
    mov ah, 0x0e                ; BIOS TTY character output
    xor bh, bh                  ; video page 0
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

boot_drive:    db 0
retry_count:   db 0

msg_booting:     db "ReeOS Stage 1 Loading...", 0x0d, 0x0a, 0
msg_jump_stage2: db "Stage 2 Loaded, Jumping...", 0x0d, 0x0a, 0
msg_err_read:    db "CRITICAL: Disk read error! System halted.", 0x0d, 0x0a, 0

; Pad to 510 bytes and append BIOS boot signature
times 510 - ($ - $$) db 0
dw 0xaa55

; Include Stage 2 directly in the final output image
incbin "boot2.bin"

; Pad Stage 2 to 8 sectors (4096 bytes)
times 4608 - ($ - $$) db 0

; Reserve space for kernel: 120 sectors at BIOS sector 10 (1-indexed) = LBA 9 = byte 4608.
; Total image = 4608 (boot+stage2) + 120*512 (kernel) = 66048 bytes = 129 sectors.
times 66048 - ($ - $$) db 0