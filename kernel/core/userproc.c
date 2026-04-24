#include <mvos/userproc.h>
#include <mvos/console.h>
#include <mvos/interrupt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
    MINIOS_LINUX_SYSCALL_WRITE = 1,
    MINIOS_LINUX_SYSCALL_BRK = 12,
    MINIOS_LINUX_SYSCALL_WRITEV = 20,
    MINIOS_LINUX_SYSCALL_UNAME = 63,
    MINIOS_LINUX_SYSCALL_GETPID = 39,
    MINIOS_LINUX_SYSCALL_EXIT = 60,
    MINIOS_LINUX_SYSCALL_ARCH_PRCTL = 158,
    MINIOS_LINUX_SYSCALL_GETTID = 186,
    MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS = 218,
    MINIOS_LINUX_SYSCALL_EXIT_GROUP = 231,
    MINIOS_SYSCALL_USER_PRINT = 1
};

enum {
    MINIOS_ARCH_SET_GS = 0x1001,
    MINIOS_ARCH_SET_FS = 0x1002,
    MINIOS_ARCH_GET_FS = 0x1003,
    MINIOS_ARCH_GET_GS = 0x1004
};

typedef struct {
    uint64_t iov_base;
    uint64_t iov_len;
} minios_iovec_t;

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} minios_utsname_t;

uint64_t g_userproc_return_rip;
uint64_t g_userproc_return_stack;
uint64_t g_userproc_current_app_id;
uint64_t g_userproc_running;

static uint64_t g_userproc_brk = 0x0000000000400000ULL;
static uint64_t g_userproc_fs_base;
static uint64_t g_userproc_gs_base;
static uint64_t g_userproc_tid_addr;

static uint64_t userproc_errno(int64_t code) {
    return (uint64_t)code;
}

static int userproc_is_errno(uint64_t value) {
    return (int64_t)value < 0;
}

static void userproc_copy_cstr(char *dst, size_t cap, const char *src) {
    if (cap == 0) {
        return;
    }
    size_t i = 0;
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    ++i;
    while (i < cap) {
        dst[i++] = '\0';
    }
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
        return userproc_errno(-9); /* EBADF */
    }
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
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

static uint64_t userproc_linux_writev(uint64_t fd, uint64_t user_iov, uint64_t iovcnt) {
    if (iovcnt == 0) {
        return 0;
    }
    if (user_iov == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (iovcnt > 16) {
        return userproc_errno(-22); /* EINVAL */
    }

    const volatile minios_iovec_t *iov = (const volatile minios_iovec_t *)(uintptr_t)user_iov;
    uint64_t total = 0;
    for (uint64_t i = 0; i < iovcnt; ++i) {
        uint64_t n = userproc_linux_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (userproc_is_errno(n)) {
            return (total > 0) ? total : n;
        }
        total += n;
    }
    return total;
}

static uint64_t userproc_linux_brk(uint64_t new_brk) {
    if (new_brk == 0) {
        return g_userproc_brk;
    }
    if (new_brk < 0x0000000000400000ULL) {
        return g_userproc_brk;
    }
    g_userproc_brk = new_brk;
    return g_userproc_brk;
}

static uint64_t userproc_linux_uname(uint64_t user_buf) {
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    volatile minios_utsname_t *u = (volatile minios_utsname_t *)(uintptr_t)user_buf;
    userproc_copy_cstr((char *)u->sysname, sizeof(u->sysname), "miniOS");
    userproc_copy_cstr((char *)u->nodename, sizeof(u->nodename), "miniOS-node");
    userproc_copy_cstr((char *)u->release, sizeof(u->release), "0.31");
    userproc_copy_cstr((char *)u->version, sizeof(u->version), "Phase31 linux abi preview+");
    userproc_copy_cstr((char *)u->machine, sizeof(u->machine), "x86_64");
    userproc_copy_cstr((char *)u->domainname, sizeof(u->domainname), "miniOS.local");
    return 0;
}

static uint64_t userproc_linux_arch_prctl(uint64_t code, uint64_t arg) {
    switch (code) {
        case MINIOS_ARCH_SET_FS:
            g_userproc_fs_base = arg;
            return 0;
        case MINIOS_ARCH_SET_GS:
            g_userproc_gs_base = arg;
            return 0;
        case MINIOS_ARCH_GET_FS:
            if (arg == 0) {
                return userproc_errno(-14); /* EFAULT */
            }
            *(volatile uint64_t *)(uintptr_t)arg = g_userproc_fs_base;
            return 0;
        case MINIOS_ARCH_GET_GS:
            if (arg == 0) {
                return userproc_errno(-14); /* EFAULT */
            }
            *(volatile uint64_t *)(uintptr_t)arg = g_userproc_gs_base;
            return 0;
        default:
            return userproc_errno(-22); /* EINVAL */
    }
}

void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack) {
    g_userproc_return_rip = return_rip;
    g_userproc_return_stack = return_stack;
}

void userproc_set_current_app_id(uint64_t app_id) {
    g_userproc_current_app_id = app_id;
}

uint64_t userproc_dispatch(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg3;
    if (!g_userproc_running) {
        return userproc_errno(-1);
    }

    /* Preserve old phase-20 teaching probes:
     * syscall(1, 1/2/3, 0, 0) prints known demo channels.
     */
    if (syscall == MINIOS_LINUX_SYSCALL_WRITE && arg2 == 0 && arg3 == 0 && arg1 > 0 && arg1 < 16) {
        userproc_legacy_print(arg1);
        return 0;
    }

    switch (syscall) {
        case MINIOS_LINUX_SYSCALL_WRITE:
            return userproc_linux_write(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_WRITEV:
            return userproc_linux_writev(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_BRK:
            return userproc_linux_brk(arg1);
        case MINIOS_LINUX_SYSCALL_UNAME:
            return userproc_linux_uname(arg1);
        case MINIOS_LINUX_SYSCALL_GETPID:
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_GETTID:
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS:
            g_userproc_tid_addr = arg1;
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_ARCH_PRCTL:
            return userproc_linux_arch_prctl(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_EXIT:
        case MINIOS_LINUX_SYSCALL_EXIT_GROUP:
            g_userproc_running = false;
            console_write_string("userapp exit requested code=");
            console_write_u64(arg1);
            console_write_string("\n");
            return 1;
        default:
            console_write_string("userapp syscall not implemented\n");
            return userproc_errno(-38); /* ENOSYS */
    }
}

static void userproc_probe_print_ret(const char *label, uint64_t value) {
    console_write_string("[linux-abi] ");
    console_write_string(label);
    console_write_string("=");
    console_write_u64(value);
    console_write_string("\n");
}

void userproc_linux_abi_probe(void) {
    uint64_t prev_running = g_userproc_running;
    uint64_t prev_app_id = g_userproc_current_app_id;

    g_userproc_running = true;
    g_userproc_current_app_id = 31;

    static const char msg1[] = "[linux-abi] probe write ok\n";
    uint64_t ret = userproc_dispatch(
        MINIOS_LINUX_SYSCALL_WRITE,
        1,
        (uint64_t)(uintptr_t)msg1,
        sizeof(msg1) - 1);
    userproc_probe_print_ret("write.ret", ret);

    static const char msg2[] = "[linux-abi] writev[0] ";
    static const char msg3[] = "writev[1]\n";
    minios_iovec_t iov[2];
    iov[0].iov_base = (uint64_t)(uintptr_t)msg2;
    iov[0].iov_len = sizeof(msg2) - 1;
    iov[1].iov_base = (uint64_t)(uintptr_t)msg3;
    iov[1].iov_len = sizeof(msg3) - 1;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_WRITEV, 1, (uint64_t)(uintptr_t)iov, 2);
    userproc_probe_print_ret("writev.ret", ret);

    userproc_probe_print_ret("getpid.ret", userproc_dispatch(MINIOS_LINUX_SYSCALL_GETPID, 0, 0, 0));
    userproc_probe_print_ret("gettid.ret", userproc_dispatch(MINIOS_LINUX_SYSCALL_GETTID, 0, 0, 0));

    uint64_t brk0 = userproc_dispatch(MINIOS_LINUX_SYSCALL_BRK, 0, 0, 0);
    userproc_probe_print_ret("brk.current", brk0);
    uint64_t brk1 = userproc_dispatch(MINIOS_LINUX_SYSCALL_BRK, brk0 + 0x1000, 0, 0);
    userproc_probe_print_ret("brk.bumped", brk1);

    minios_utsname_t uts;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_UNAME, (uint64_t)(uintptr_t)&uts, 0, 0);
    userproc_probe_print_ret("uname.ret", ret);
    if (!userproc_is_errno(ret)) {
        console_write_string("[linux-abi] uname.sysname=");
        console_write_string(uts.sysname);
        console_write_string(" release=");
        console_write_string(uts.release);
        console_write_string("\n");
    }

    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_ARCH_PRCTL, MINIOS_ARCH_SET_FS, 0x700000001000ULL, 0);
    userproc_probe_print_ret("arch_prctl.set_fs", ret);
    uint64_t fs_base = 0;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_ARCH_PRCTL, MINIOS_ARCH_GET_FS, (uint64_t)(uintptr_t)&fs_base, 0);
    userproc_probe_print_ret("arch_prctl.get_fs", ret);
    userproc_probe_print_ret("arch_prctl.fs_base", fs_base);

    uint64_t tid_storage = 0;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS, (uint64_t)(uintptr_t)&tid_storage, 0, 0);
    userproc_probe_print_ret("set_tid_address.ret", ret);
    userproc_probe_print_ret("set_tid_address.ptr", g_userproc_tid_addr);

    g_userproc_running = prev_running;
    g_userproc_current_app_id = prev_app_id;
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
