#include <mvos/userproc.h>
#include <mvos/log.h>
#include <mvos/console.h>
#include <mvos/interrupt.h>
#include <mvos/pmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
    MINIOS_LINUX_SYSCALL_WRITE = 1,
    MINIOS_LINUX_SYSCALL_GETPID = 39,
    MINIOS_LINUX_SYSCALL_EXIT = 60,
    MINIOS_SYSCALL_USER_PRINT = 1
};

uint64_t g_userproc_return_rip;
uint64_t g_userproc_return_stack;
uint64_t g_userproc_current_app_id;
uint64_t g_userproc_running;

static uint64_t userproc_errno(int64_t code) {
    return (uint64_t)code;
}

static void userproc_legacy_print(uint64_t channel) {
    const char *msg = "userapp";
    console_write_string("userapp #");
    console_write_u64(g_userproc_current_app_id);
    console_write_string(" syscall ");
    console_write_u64(channel);
    console_write_string(": ");
    switch ((uint8_t)channel) {
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
}

static uint64_t userproc_linux_write(uint64_t fd, uint64_t user_buf, uint64_t count) {
    if (fd != 1 && fd != 2) {
        return userproc_errno(-9);
    }
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14);
    }

    uint64_t to_write = count;
    if (to_write > 512) {
        to_write = 512;
    }

    const volatile char *buf = (const volatile char *)(uintptr_t)user_buf;
    for (uint64_t i = 0; i < to_write; ++i) {
        console_write_char(buf[i]);
    }
    return to_write;
}

void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack) {
    g_userproc_return_rip = return_rip;
    g_userproc_return_stack = return_stack;
}

void userproc_set_current_app_id(uint64_t app_id) {
    g_userproc_current_app_id = app_id;
}

uint64_t userproc_dispatch(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (!g_userproc_running) {
        return userproc_errno(-1);
    }

    /* Linux ABI preview path for userspace compatibility experiments. */
    if (syscall == MINIOS_LINUX_SYSCALL_WRITE) {
        if (arg2 == 0 && arg3 == 0 && arg1 > 0 && arg1 < 16) {
            userproc_legacy_print(arg1);
            return 0;
        }
        return userproc_linux_write(arg1, arg2, arg3);
    }
    if (syscall == MINIOS_LINUX_SYSCALL_GETPID) {
        return 1000 + g_userproc_current_app_id;
    }
    if (syscall == MINIOS_LINUX_SYSCALL_EXIT) {
        g_userproc_running = false;
        console_write_string("userapp exit requested\n");
        return 1;
    }

    switch (syscall) {
        case MINIOS_SYSCALL_USER_PRINT:
            userproc_legacy_print(arg1);
            return 0;
        default:
            console_write_string("userapp syscall not implemented\n");
            return userproc_errno(-38);
    }
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
