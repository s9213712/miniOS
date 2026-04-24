.global _start
.section .text
_start:
    mov $1, %rax
    mov $1, %rdi
    lea msg(%rip), %rsi
    mov $msg_len, %rdx
    syscall

    mov $257, %rax
    mov $-100, %rdi
    lea readme_path(%rip), %rsi
    xor %rdx, %rdx
    xor %r10, %r10
    syscall
    test %rax, %rax
    js .exit

    mov %rax, %r12
    xor %rax, %rax
    mov %r12, %rdi
    lea read_buf(%rip), %rsi
    mov $read_buf_len, %rdx
    syscall
    test %rax, %rax
    jle .close

    mov %rax, %r13
    mov $1, %rax
    mov $1, %rdi
    lea read_prefix(%rip), %rsi
    mov $read_prefix_len, %rdx
    syscall

    mov $1, %rax
    mov $1, %rdi
    lea read_buf(%rip), %rsi
    mov %r13, %rdx
    syscall

.close:
    mov $3, %rax
    mov %r12, %rdi
    syscall

.exit:
    mov $60, %rax
    xor %rdi, %rdi
    syscall

.section .rodata
msg:
    .ascii "hello from linux user elf\n"
msg_len = . - msg
readme_path:
    .asciz "/boot/init/readme.txt"
read_prefix:
    .ascii "read from initfs: "
read_prefix_len = . - read_prefix

.section .bss
.balign 16
read_buf:
    .skip 80
read_buf_len = . - read_buf
