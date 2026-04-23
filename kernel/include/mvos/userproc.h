#pragma once

#include <stdint.h>

void userproc_enter(uint64_t entry, uint64_t user_stack_top, uint64_t return_rip, uint64_t return_stack);
void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack);
void userproc_set_current_app_id(uint64_t app_id);
uint64_t userproc_dispatch(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3);
