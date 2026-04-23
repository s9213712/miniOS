.text
.globl _start
_start:
    cli
    leaq kernel_stack_top(%rip), %rsp
    call kmain

.section .bss
    .align 16
kernel_stack:
    .skip 16384
kernel_stack_top:

.section .text
.halt:
    cli
    hlt
    jmp .halt
