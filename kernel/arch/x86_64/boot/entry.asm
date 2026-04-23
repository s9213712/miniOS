SECTION .text
global _start
extern kmain

; Reserve a small bootstrap stack (16 KiB)
section .bss
align 16
kernel_stack:
    resb 16384
kernel_stack_top:

section .text
_start:
    cli
    mov rsp, kernel_stack_top
    call kmain

.halt:
    cli
    hlt
    jmp .halt