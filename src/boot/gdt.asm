; Global Descriptor Table (GDT) for mode transitions.
; We need a temporary 32-bit GDT to switch from real mode to protected mode,
; and then a 64-bit GDT to finally leap into 64-bit long mode.

align 4
gdt_start_32:
    ; null descriptor is mandatory
    dd 0
    dd 0

gdt_code_32:
    ; 32-bit code descriptor for kernel. base=0, limit=4GB, granularity=page
    dw 0xffff
    dw 0x0000
    db 0x00
    db 0x9a
    db 0xcf
    db 0x00

gdt_data_32:
    ; 32-bit data descriptor. base=0, limit=4GB, granularity=page
    dw 0xffff
    dw 0x0000
    db 0x00
    db 0x92
    db 0xcf
    db 0x00

gdt_end_32:

gdt_descriptor_32:
    dw gdt_end_32 - gdt_start_32 - 1
    dd gdt_start_32

; Selectors for 32-bit segments
CODE_SEG_32 equ gdt_code_32 - gdt_start_32
DATA_SEG_32 equ gdt_data_32 - gdt_start_32


align 8
gdt_start_64:
    ; another null descriptor for 64-bit GDT
    dq 0

gdt_code_64:
    ; 64-bit code descriptor. base and limit are ignored in long mode, but L-bit (long mode) must be 1.
    dw 0x0000
    dw 0x0000
    db 0x00
    db 0x9a
    db 0x20
    db 0x00

gdt_data_64:
    ; 64-bit data descriptor. required for configuring data segments in long mode.
    dw 0x0000
    dw 0x0000
    db 0x00
    db 0x92
    db 0x00
    db 0x00

gdt_end_64:

gdt_descriptor_64:
    dw gdt_end_64 - gdt_start_64 - 1
    dq gdt_start_64

; Selectors for 64-bit segments
CODE_SEG_64 equ gdt_code_64 - gdt_start_64
DATA_SEG_64 equ gdt_data_64 - gdt_start_64
