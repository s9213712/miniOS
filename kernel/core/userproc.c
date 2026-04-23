#include <mvos/userproc.h>
#include <mvos/log.h>
#include <mvos/console.h>
#include <mvos/interrupt.h>
#include <mvos/pmm.h>
#include <stdbool.h>
#include <stdint.h>

enum {
    MINIOS_SYSCALL_USER_PRINT = 1,
    MINIOS_SYSCALL_USER_EXIT = 60
};

uint64_t g_userproc_return_rip;
uint64_t g_userproc_return_stack;
uint64_t g_userproc_current_app_id;
uint64_t g_userproc_running;

void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack) {
    g_userproc_return_rip = return_rip;
    g_userproc_return_stack = return_stack;
}

void userproc_set_current_app_id(uint64_t app_id) {
    g_userproc_current_app_id = app_id;
}

uint64_t userproc_dispatch(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2;
    (void)arg3;
    if (!g_userproc_running) {
        return 0;
    }
    switch (syscall) {
        case MINIOS_SYSCALL_USER_PRINT: {
            const char *msg = "userapp";
            console_write_string("userapp #");
            console_write_u64(g_userproc_current_app_id);
            console_write_string(" syscall ");
            console_write_u64(arg1);
            console_write_string(": ");
            switch ((uint8_t)arg1) {
                case 1:
                    console_write_string("hello from user app\n");
                    break;
                case 2:
                    console_write_string("ticks=");
                    console_write_u64(timer_ticks());
                    console_write_string("\n");
                    break;
                case 3:
                    console_write_string("scheduler not exposed to user yet\n");
                    break;
                default:
                    console_write_string(msg);
                    console_write_string(" print request\n");
                    break;
            }
            break;
        }
        case MINIOS_SYSCALL_USER_EXIT:
            g_userproc_running = false;
            console_write_string("userapp exit requested\n");
            return 1;
        default:
            console_write_string("userapp syscall not implemented\n");
            break;
    }
    return 0;
}

/* Entry point implemented in assembly:
 * pushes a user-mode IRET frame and jumps via iretq.
 */
extern void userproc_enter_asm(uint64_t entry, uint64_t user_stack);

void userproc_enter(uint64_t entry, uint64_t user_stack_top, uint64_t return_rip, uint64_t return_stack) {
    g_userproc_running = true;
    userproc_set_return_context(return_rip, return_stack);
    g_userproc_current_app_id = 0;
    userproc_enter_asm(entry, user_stack_top);
}
