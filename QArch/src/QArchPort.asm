; QArch Port - Assembly port I/O and CPU operations
; This file contains low-level assembly routines

section .text

global asm_outb
global asm_inb
global asm_outw
global asm_inw
global asm_outl
global asm_inl
global asm_io_wait
global asm_cli
global asm_sti
global asm_hlt
global cpu_relax

; void asm_outb(uint16_t port, uint8_t value)
asm_outb:
    mov dx, di      ; port
    mov al, sil     ; value
    out dx, al
    ret

; uint8_t asm_inb(uint16_t port)
asm_inb:
    mov dx, di      ; port
    xor eax, eax
    in al, dx
    ret

; void asm_outw(uint16_t port, uint16_t value)
asm_outw:
    mov dx, di      ; port
    mov ax, si      ; value
    out dx, ax
    ret

; uint16_t asm_inw(uint16_t port)
asm_inw:
    mov dx, di      ; port
    xor eax, eax
    in ax, dx
    ret

; void asm_outl(uint16_t port, uint32_t value)
asm_outl:
    mov dx, di      ; port
    mov eax, esi    ; value
    out dx, eax
    ret

; uint32_t asm_inl(uint16_t port)
asm_inl:
    mov dx, di      ; port
    in eax, dx
    ret

; void asm_io_wait(void)
asm_io_wait:
    mov al, 0
    out 0x80, al
    ret

; void asm_cli(void)
asm_cli:
    cli
    ret

; void asm_sti(void)
asm_sti:
    sti
    ret

; void asm_hlt(void)
asm_hlt:
    hlt
    ret

; void cpu_relax(void)
cpu_relax:
    pause
    ret

; Mark stack as non-executable (silences ld executable-stack warning)
section .note.GNU-stack noalloc noexec nowrite
