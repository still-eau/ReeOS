; =============================================================================
;  ReeOS - Stage 1 Boot Sector
;  Start in 16-bit Real Mode at physical address 0x7C00
; =============================================================================

[bits 16]
[org 0x7c00]

; --- Configuration Constants ---
STAGE2_LOAD_SEG  equ 0x0000
STAGE2_LOAD_OFS  equ 0x8000
STAGE2_SECTOR    equ 2          ; CHS sector #2 (sector 1 is the MBR itself)
STAGE2_SEC_COUNT equ 8          ; Load 8 sectors (4 KB), easily enough for Stage 2

start:
    cli                         ; Disable interrupts during register configuration

    ; Proper initialization of segment registers (0x0000)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Stack configuration
    ; Grows downward from 0x7C00, a safe and standard region in conventional memory
    mov ss, ax
    mov sp, 0x7c00

    cld                         ; DF = 0 (read strings forward with lodsb)
    sti                         ; Stack is stable, re-enable interrupts

    mov [boot_drive], dl        ; BIOS passes the boot drive number in DL, save it

    ; Welcome message
    mov si, msg_booting
    call print_string

    ; Load Stage 2 from disk
    call load_stage2

    ; Transition message
    mov si, msg_jump_stage2
    call print_string

    ; Final jump to Stage 2 (0x0000:0x8000)
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFS

; --- Function: Load Stage 2 via BIOS INT 0x13 AH=02h ---
load_stage2:
    mov byte [retry_count], 3   ; Max 3 retries on disk read error

.try_read:
    mov ax, STAGE2_LOAD_SEG
    mov es, ax
    mov bx, STAGE2_LOAD_OFS     ; ES:BX points to 0x0000:0x8000

    mov ah, 0x02                ; BIOS Function: Read sectors
    mov al, STAGE2_SEC_COUNT    ; Number of sectors to read
    mov ch, 0                   ; Cylinder 0
    mov dh, 0                   ; Head 0
    mov cl, STAGE2_SECTOR       ; Sector 2 (1-indexed for CHS)
    mov dl, [boot_drive]        ; Source disk
    int 0x13
    jnc .read_success           ; If Carry Flag (CF) is 0 -> Success!

    ; On failure: reset disk controller and retry
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13

    dec byte [retry_count]
    jnz .try_read

    ; If we get here, the error is fatal
    mov si, msg_err_read
    call print_string
    cli
    hlt

.read_success:
    ret

; --- Function: Print a string (TTY) ---
print_string:
    push ax
    push bx
    mov ah, 0x0e                ; BIOS Function: TTY Character Output
    xor bh, bh                  ; Video page 0
.loop:
    lodsb                       ; Load byte from DS:SI into AL and increment SI
    test al, al                 ; Is it the end of the string (\0)?
    jz .done
    int 0x10                    ; BIOS video interrupt
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; --- Data and Variables ---
boot_drive:    db 0
retry_count:   db 0

msg_booting:      db "ReeOS Stage 1 Loading...", 0x0d, 0x0a, 0
msg_jump_stage2:  db "Stage 2 Loaded, Jumping...", 0x0d, 0x0a, 0
msg_err_read:     db "CRITICAL: Disk read error! System halted.", 0x0d, 0x0a, 0

times 510 - ($ - $$) db 0
dw 0xaa55