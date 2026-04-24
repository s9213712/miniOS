BITS 64
default rel

global userproc_enter_asm
global userproc_enter_execve_asm
global userproc_execve_trampoline_asm
global isr_user_syscall
global syscall_entry
global minios_userapp_hello
global minios_userapp_ticks
global minios_userapp_scheduler
global minios_userapp_linux_abi

extern userproc_dispatch
extern g_userproc_return_rip
extern g_userproc_return_stack
extern g_userproc_syscall_stack_top
extern g_userproc_syscall_user_rsp

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

userproc_enter_execve_asm:
    ; Arguments:
    ; rdi = entry function pointer (RIP in user mode)
    ; rsi = initial userspace RSP
    ; rdx = argc
    ; rcx = argv pointer
    ; r8  = envp pointer
    mov r9, rdi
    mov r10, rsi
    push qword 0x23          ; user data segment selector
    push r10                 ; user stack pointer
    pushfq
    pop rax
    or eax, 0x200
    push rax
    push qword 0x1B          ; user code selector (RPL=3)
    push r9                  ; user RIP
    mov rdi, rdx             ; argc
    mov rsi, rcx             ; argv
    mov rdx, r8              ; envp
    iretq

userproc_execve_trampoline_asm:
    ; Arguments:
    ; rdi = entry function pointer (RIP in user mode)
    ; rsi = initial userspace RSP
    ; rdx = argc
    ; rcx = argv pointer
    ; r8  = envp pointer
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    lea rax, [rel .return]
    mov [rel g_userproc_return_rip], rax
    mov [rel g_userproc_return_stack], rsp
    mov r9, rdi
    mov r10, rsi
    push qword 0x23          ; user data segment selector
    push r10                 ; user stack pointer
    pushfq
    pop rax
    or eax, 0x200
    push rax
    push qword 0x1B          ; user code selector (RPL=3)
    push r9                  ; user RIP
    mov rdi, rdx             ; argc
    mov rsi, rcx             ; argv
    mov rdx, r8              ; envp
    iretq
.return:
    mov eax, 1
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

userapp_hello_syscall_loop:
    mov eax, 1
    mov edi, 1
    xor esi, esi
    xor edx, edx
    syscall
    mov eax, 60
    xor edi, edi
    syscall

minios_userapp_hello:
    nop
    jmp userapp_hello_syscall_loop

minios_userapp_ticks:
    mov eax, 1
    mov edi, 2
    xor esi, esi
    xor edx, edx
    syscall
    mov eax, 60
    xor edi, edi
    syscall

minios_userapp_scheduler:
    mov eax, 1
    mov edi, 3
    xor esi, esi
    xor edx, edx
    syscall
    mov eax, 60
    xor edi, edi
    syscall

minios_userapp_linux_abi:
    mov eax, 1
    mov edi, 1
    lea rsi, [rel linux_abi_msg]
    mov edx, linux_abi_msg_len
    syscall
    mov eax, 20
    mov edi, 1
    lea rsi, [rel linux_abi_iov]
    mov edx, 2
    syscall
    mov eax, 39
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall
    mov eax, 186
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall
    mov eax, 231
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall

syscall_entry:
    ; Long-mode SYSCALL does not switch stacks. Capture the user RSP,
    ; then move to the kernel-provided syscall stack before touching C.
    mov [rel g_userproc_syscall_user_rsp], rsp
    mov rsp, [rel g_userproc_syscall_stack_top]

    ; Build an IRET-compatible user return frame.
    push qword 0x23          ; user data segment selector
    push qword [rel g_userproc_syscall_user_rsp]
    push r11                 ; user RFLAGS saved by SYSCALL
    push qword 0x1B          ; user code selector
    push rcx                 ; user RIP saved by SYSCALL

    ; Save all general-purpose registers. RCX/R11 intentionally restore to
    ; SYSCALL-clobbered values (return RIP / flags), matching x86-64 ABI.
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

    mov rdi, [rsp + 112]    ; syscall number
    mov rsi, [rsp + 72]     ; arg1: rdi
    mov rdx, [rsp + 80]     ; arg2: rsi
    mov rcx, [rsp + 88]     ; arg3: rdx
    mov r8, [rsp + 40]      ; arg4: r10 in the Linux syscall ABI
    mov r9, [rsp + 56]      ; arg5: r8
    mov rax, [rsp + 48]     ; arg6: r9
    sub rsp, 16
    mov [rsp], rax
    call userproc_dispatch
    add rsp, 16
    cmp rax, 1
    je .syscall_user_exit
    mov [rsp + 112], rax

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
    pop rax
    iretq

.syscall_user_exit:
    add rsp, 120
    mov rsp, [rel g_userproc_return_stack]
    mov rax, [rel g_userproc_return_rip]
    sti
    jmp rax

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
    ; r10     = arg4
    ; r8      = arg5
    ; r9      = arg6
    ; user_dispatch returns 1 on exit request.
    mov rdi, [rsp + 112]
    mov rsi, [rsp + 72]
    mov rdx, [rsp + 80]
    mov rcx, [rsp + 88]
    mov r8, [rsp + 40]
    mov r9, [rsp + 56]
    mov rax, [rsp + 48]
    sub rsp, 16
    mov [rsp], rax
    call userproc_dispatch
    add rsp, 16
    cmp rax, 1
    je .user_syscall_exit
    mov [rsp + 112], rax

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
    pop rax
    iretq

.user_syscall_exit:
    ; User exit requested: resume the saved supervisor continuation.
    ; Returning to CPL0 with iretq would not pop RSP/SS, so switch stacks directly.
    add rsp, 120
    mov rsp, [rel g_userproc_return_stack]
    mov rax, [rel g_userproc_return_rip]
    sti
    jmp rax

section .rodata
linux_abi_msg: db "linux abi preview: write/getpid/gettid/exit_group", 10
linux_abi_msg_len equ $ - linux_abi_msg
linux_abi_msg2a: db "[linux-abi] writev part A "
linux_abi_msg2a_len equ $ - linux_abi_msg2a
linux_abi_msg2b: db "part B", 10
linux_abi_msg2b_len equ $ - linux_abi_msg2b
linux_abi_iov:
    dq linux_abi_msg2a
    dq linux_abi_msg2a_len
    dq linux_abi_msg2b
    dq linux_abi_msg2b_len
