.global _start
.section .text
_start:
    mov $1, %rax
    mov $1, %rdi
    lea msg(%rip), %rsi
    mov $msg_len, %rdx
    int $0x80

    mov $60, %rax
    xor %rdi, %rdi
    int $0x80

.section .rodata
msg:
    .ascii "hello from linux user elf\n"
msg_len = . - msg
