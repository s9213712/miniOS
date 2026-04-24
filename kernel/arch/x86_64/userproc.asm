BITS 64
default rel

global userproc_enter_asm
global isr_user_syscall
global minios_userapp_hello
global minios_userapp_ticks
global minios_userapp_scheduler
global minios_userapp_linux_abi

extern userproc_dispatch
extern g_userproc_return_rip
extern g_userproc_return_stack

section .text

userproc_enter_asm:
    ; Arguments:
    ; rdi = entry function pointer (RIP in user mode)
    ; rsi = user stack top
    push qword 0x23          ; user data segment selector
    push rsi                  ; user stack pointer
    pushfq                    ; copy current RFLAGS
    pop rax
    or eax, 0x200             ; keep IF set for user-mode responsiveness
    push rax                  ; restore modified RFLAGS
    push qword 0x1B           ; user code selector (RPL=3)
    push rdi                  ; user RIP
    iretq

userapp_hello_syscall_loop:
    mov eax, 1
    mov edi, 1
    int 0x80
    mov eax, 60
    xor edi, edi
    int 0x80

minios_userapp_hello:
    nop
    jmp userapp_hello_syscall_loop

minios_userapp_ticks:
    mov eax, 1
    mov edi, 2
    int 0x80
    mov eax, 60
    xor edi, edi
    int 0x80

minios_userapp_scheduler:
    mov eax, 1
    mov edi, 3
    int 0x80
    mov eax, 60
    xor edi, edi
    int 0x80

minios_userapp_linux_abi:
    mov eax, 1
    mov edi, 1
    lea rsi, [rel linux_abi_msg]
    mov edx, linux_abi_msg_len
    int 0x80
    mov eax, 39
    int 0x80
    mov eax, 60
    xor edi, edi
    int 0x80

isr_user_syscall:
    ; Save all general-purpose registers.
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; rax     = syscall number
    ; rdi     = arg1
    ; rsi     = arg2
    ; rdx     = arg3
    ; user_dispatch returns 1 on exit request.
    mov r8, rax
    mov r9, rdi
    mov r10, rsi
    mov r11, rdx
    mov rdi, r8
    mov rsi, r9
    mov rdx, r10
    mov rcx, r11
    call userproc_dispatch
    mov rbx, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx

    cmp rbx, 1
    jne .user_syscall_continue

    ; User exit requested: rebuild kernel return frame.
    mov rax, [rel g_userproc_return_rip]
    mov [rsp], rax
    mov qword [rsp + 8], 0x08
    mov rbx, [rel g_userproc_return_stack]
    mov qword [rsp + 24], rbx
    mov qword [rsp + 32], 0x10
    mov qword [rsp + 16], 0x202
    pop rax
    iretq

.user_syscall_continue:
    add rsp, 8
    mov rax, rbx
    iretq

section .rodata
linux_abi_msg: db "linux abi preview: write/getpid/exit ok", 10
linux_abi_msg_len equ $ - linux_abi_msg
