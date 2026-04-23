SECTION .text
global _start
global kernel_stack_top
extern kmain

%define COM1_PORT 0x3f8
%define SERIAL_LSR 0x3fd
%define SERIAL_DLL 0x3f8
%define SERIAL_DLM 0x3f9
%define SERIAL_IER 0x3f9
%define SERIAL_IIR 0x3fa
%define SERIAL_LCR 0x3fb
%define SERIAL_MCR 0x3fc

section .rodata
entry_boot_msg: db "miniOS: kernel entry", 10, 0
early_hello_msg: db "hello from kernel", 10, 0

; Reserve a small bootstrap stack (16 KiB)
section .bss
align 16
kernel_stack:
    resb 16384
global kernel_stack_top
kernel_stack_top:

section .text
_start:
    cli
    mov rsp, kernel_stack_top
    call serial_init_early
    lea rdi, [rel entry_boot_msg]
    call serial_write_string_early
    lea rdi, [rel early_hello_msg]
    call serial_write_string_early
    call kmain

.halt:
    cli
    hlt
    jmp .halt

serial_init_early:
    mov al, 0x00
    mov dx, SERIAL_IER
    out dx, al

    mov al, 0x80
    mov dx, SERIAL_LCR
    out dx, al
    mov al, 0x03
    mov dx, SERIAL_DLL
    out dx, al
    mov al, 0x00
    mov dx, SERIAL_DLM
    out dx, al

    mov al, 0x03
    mov dx, SERIAL_LCR
    out dx, al
    mov al, 0xc7
    mov dx, SERIAL_IIR
    out dx, al
    mov al, 0x0b
    mov dx, SERIAL_MCR
    out dx, al
    ret

serial_write_string_early:
    mov rsi, rdi
.serial_loop:
    lodsb
    test al, al
    jz .serial_done
    call serial_write_char_early
    jmp .serial_loop

.serial_done:
    ret

serial_write_char_early:
    mov ah, al
    mov al, 0x0d
    cmp ah, 10
    jne .send_char
    mov dx, COM1_PORT
    call serial_wait_ready
    out dx, al
    mov al, ah
    call serial_wait_ready
    out dx, al
    ret

.send_char:
    mov al, ah
    mov dx, COM1_PORT
    call serial_wait_ready
    out dx, al
    ret

serial_wait_ready:
    mov dx, SERIAL_LSR
    mov ecx, 0x00020000
.wait:
    in al, dx
    and al, 0x20
    jnz .ready
    dec rcx
    jnz .wait
    ret
.ready:
    ret
