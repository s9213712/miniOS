#pragma once

#include <stdint.h>

typedef struct {
    uint64_t initial_rsp;
    uint64_t argc;
    uint64_t argv_user;
    uint64_t envp_user;
    uint64_t auxv_user;
    uint64_t strings_bytes;
    uint64_t used_bytes;
} mvos_user_stack_layout_t;

void userproc_enter(uint64_t entry, uint64_t user_stack_top, uint64_t return_rip, uint64_t return_stack);
void userproc_enter_execve(uint64_t entry,
                           uint64_t user_stack_top,
                           uint64_t argc,
                           uint64_t argv_user,
                           uint64_t envp_user,
                           uint64_t return_rip,
                           uint64_t return_stack);
uint64_t userproc_enter_execve_and_wait(uint64_t entry,
                                        uint64_t user_stack_top,
                                        uint64_t argc,
                                        uint64_t argv_user,
                                        uint64_t envp_user);
void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack);
void userproc_set_current_app_id(uint64_t app_id);
void userproc_set_exe_path(const char *path);
void userproc_syscall_init(void);
uint64_t userproc_dispatch(uint64_t syscall,
                           uint64_t arg1,
                           uint64_t arg2,
                           uint64_t arg3,
                           uint64_t arg4,
                           uint64_t arg5,
                           uint64_t arg6);
void userproc_linux_abi_probe(void);
int userproc_handoff_dry_run(uint64_t entry, uint64_t user_stack_top);
const char *userproc_handoff_result_name(int rc);
int userproc_prepare_exec_stack(uint8_t *stack_mem,
                                uint64_t stack_size,
                                uint64_t stack_base,
                                uint64_t stack_top,
                                const char *const *argv,
                                uint64_t argc,
                                const char *const *envp,
                                uint64_t envc,
                                mvos_user_stack_layout_t *out);
const char *userproc_stack_result_name(int rc);
