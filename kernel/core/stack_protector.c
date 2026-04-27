#include <mvos/panic.h>
#include <stdint.h>

uintptr_t __stack_chk_guard = 0x6d766f735f737370ULL;

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail(void) {
    panic("stack protector check failed");
    __builtin_unreachable();
}

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail_local(void) {
    __stack_chk_fail();
}
