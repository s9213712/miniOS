#include <mvos/userproc.h>
#include <mvos/console.h>
#include <mvos/gdt.h>
#include <mvos/interrupt.h>
#include <mvos/userimg.h>
#include <mvos/vfs.h>
#include <mvos/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum {
    MINIOS_LINUX_SYSCALL_READ = 0,
    MINIOS_LINUX_SYSCALL_WRITE = 1,
    MINIOS_LINUX_SYSCALL_CLOSE = 3,
    MINIOS_LINUX_SYSCALL_FSTAT = 5,
    MINIOS_LINUX_SYSCALL_LSEEK = 8,
    MINIOS_LINUX_SYSCALL_MMAP = 9,
    MINIOS_LINUX_SYSCALL_MPROTECT = 10,
    MINIOS_LINUX_SYSCALL_MUNMAP = 11,
    MINIOS_LINUX_SYSCALL_BRK = 12,
    MINIOS_LINUX_SYSCALL_RT_SIGACTION = 13,
    MINIOS_LINUX_SYSCALL_RT_SIGPROCMASK = 14,
    MINIOS_LINUX_SYSCALL_IOCTL = 16,
    MINIOS_LINUX_SYSCALL_PREAD64 = 17,
    MINIOS_LINUX_SYSCALL_ACCESS = 21,
    MINIOS_LINUX_SYSCALL_WRITEV = 20,
    MINIOS_LINUX_SYSCALL_MADVISE = 28,
    MINIOS_LINUX_SYSCALL_DUP = 32,
    MINIOS_LINUX_SYSCALL_DUP2 = 33,
    MINIOS_LINUX_SYSCALL_UNAME = 63,
    MINIOS_LINUX_SYSCALL_FCNTL = 72,
    MINIOS_LINUX_SYSCALL_GETCWD = 79,
    MINIOS_LINUX_SYSCALL_GETPID = 39,
    MINIOS_LINUX_SYSCALL_EXECVE = 59,
    MINIOS_LINUX_SYSCALL_EXIT = 60,
    MINIOS_LINUX_SYSCALL_READLINK = 89,
    MINIOS_LINUX_SYSCALL_GETTIMEOFDAY = 96,
    MINIOS_LINUX_SYSCALL_SYSINFO = 99,
    MINIOS_LINUX_SYSCALL_GETUID = 102,
    MINIOS_LINUX_SYSCALL_GETGID = 104,
    MINIOS_LINUX_SYSCALL_GETEUID = 107,
    MINIOS_LINUX_SYSCALL_GETEGID = 108,
    MINIOS_LINUX_SYSCALL_GETPPID = 110,
    MINIOS_LINUX_SYSCALL_GETRLIMIT = 97,
    MINIOS_LINUX_SYSCALL_ARCH_PRCTL = 158,
    MINIOS_LINUX_SYSCALL_GETTID = 186,
    MINIOS_LINUX_SYSCALL_FUTEX = 202,
    MINIOS_LINUX_SYSCALL_SCHED_GETAFFINITY = 204,
    MINIOS_LINUX_SYSCALL_GETDENTS64 = 217,
    MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS = 218,
    MINIOS_LINUX_SYSCALL_CLOCK_GETTIME = 228,
    MINIOS_LINUX_SYSCALL_EXIT_GROUP = 231,
    MINIOS_LINUX_SYSCALL_OPENAT = 257,
    MINIOS_LINUX_SYSCALL_NEWFSTATAT = 262,
    MINIOS_LINUX_SYSCALL_READLINKAT = 267,
    MINIOS_LINUX_SYSCALL_FACCESSAT = 269,
    MINIOS_LINUX_SYSCALL_SET_ROBUST_LIST = 273,
    MINIOS_LINUX_SYSCALL_GET_ROBUST_LIST = 274,
    MINIOS_LINUX_SYSCALL_DUP3 = 292,
    MINIOS_LINUX_SYSCALL_PRLIMIT64 = 302,
    MINIOS_LINUX_SYSCALL_GETRANDOM = 318,
    MINIOS_LINUX_SYSCALL_STATX = 332,
    MINIOS_SYSCALL_USER_PRINT = 1
};

enum {
    MINIOS_ARCH_SET_GS = 0x1001,
    MINIOS_ARCH_SET_FS = 0x1002,
    MINIOS_ARCH_GET_FS = 0x1003,
    MINIOS_ARCH_GET_GS = 0x1004
};

enum {
    MINIOS_EXEC_STACK_MAX_ARGC = 32,
    MINIOS_EXEC_STACK_MAX_ENVC = 32,
    MINIOS_EXECVE_MAX_PATH = 256,
    MINIOS_EXECVE_MAX_ARG_STR = 128,
    MINIOS_EXECVE_STACK_SCRATCH_SIZE = 65536,
    MINIOS_EXECVE_KERNEL_STACK_SIZE = 16384,
    MINIOS_USERPROC_MAX_FDS = 8,
    MINIOS_USERPROC_FD_BASE = 3,
    MINIOS_USERPROC_ROOT_ID = 0,
    MINIOS_AT_FDCWD = -100,
    MINIOS_O_ACCMODE = 0x3,
    MINIOS_O_WRONLY = 0x1,
    MINIOS_O_DIRECTORY = 00200000,
    MINIOS_PROT_READ = 0x1,
    MINIOS_PROT_WRITE = 0x2,
    MINIOS_PROT_EXEC = 0x4,
    MINIOS_MAP_PRIVATE = 0x02,
    MINIOS_MAP_FIXED = 0x10,
    MINIOS_MAP_ANONYMOUS = 0x20,
    MINIOS_SEEK_SET = 0,
    MINIOS_SEEK_CUR = 1,
    MINIOS_SEEK_END = 2,
    MINIOS_CLOCK_REALTIME = 0,
    MINIOS_CLOCK_MONOTONIC = 1,
    MINIOS_F_GETFD = 1,
    MINIOS_F_SETFD = 2,
    MINIOS_F_GETFL = 3,
    MINIOS_F_SETFL = 4,
    MINIOS_FD_CLOEXEC = 1,
    MINIOS_RT_SIGSET_SIZE = 8,
    MINIOS_RT_SIGACTION_SIZE = 32,
    MINIOS_RLIMIT_DATA = 2,
    MINIOS_RLIMIT_STACK = 3,
    MINIOS_RLIMIT_NOFILE = 7,
    MINIOS_RLIMIT_AS = 9,
    MINIOS_FUTEX_WAIT = 0,
    MINIOS_FUTEX_WAKE = 1,
    MINIOS_FUTEX_PRIVATE_FLAG = 128,
    MINIOS_FUTEX_CLOCK_REALTIME = 256,
    MINIOS_FUTEX_CMD_MASK = ~(MINIOS_FUTEX_PRIVATE_FLAG | MINIOS_FUTEX_CLOCK_REALTIME),
    MINIOS_S_IFREG = 0100000,
    MINIOS_S_IFDIR = 0040000,
    MINIOS_S_IFCHR = 0020000,
    MINIOS_STATX_TYPE = 0x0001,
    MINIOS_STATX_MODE = 0x0002,
    MINIOS_STATX_NLINK = 0x0004,
    MINIOS_STATX_UID = 0x0008,
    MINIOS_STATX_GID = 0x0010,
    MINIOS_STATX_ATIME = 0x0020,
    MINIOS_STATX_MTIME = 0x0040,
    MINIOS_STATX_CTIME = 0x0080,
    MINIOS_STATX_INO = 0x0100,
    MINIOS_STATX_SIZE = 0x0200,
    MINIOS_STATX_BLOCKS = 0x0400,
    MINIOS_STATX_BASIC_STATS = MINIOS_STATX_TYPE | MINIOS_STATX_MODE | MINIOS_STATX_NLINK |
                                MINIOS_STATX_UID | MINIOS_STATX_GID | MINIOS_STATX_ATIME |
                                MINIOS_STATX_MTIME | MINIOS_STATX_CTIME | MINIOS_STATX_INO |
                                MINIOS_STATX_SIZE | MINIOS_STATX_BLOCKS,
    MINIOS_DT_DIR = 4,
    MINIOS_DT_REG = 8,
    MINIOS_USERPROC_MMAP_BASE = 0x0000400001000000ULL,
    MINIOS_USERPROC_MMAP_LIMIT = 0x0000400010000000ULL,
};

typedef struct {
    uint64_t iov_base;
    uint64_t iov_len;
} minios_iovec_t;

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} minios_rlimit_t;

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t __unused[3];
} minios_linux_stat_t;

typedef struct {
    int64_t tv_sec;
    uint32_t tv_nsec;
    int32_t __reserved;
} minios_statx_timestamp_t;

typedef struct {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    minios_statx_timestamp_t stx_atime;
    minios_statx_timestamp_t stx_btime;
    minios_statx_timestamp_t stx_ctime;
    minios_statx_timestamp_t stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t stx_mnt_id;
    uint32_t stx_dio_mem_align;
    uint32_t stx_dio_offset_align;
    uint64_t __spare3[12];
} minios_statx_t;

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} minios_utsname_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} minios_timespec_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_usec;
} minios_timeval_t;

typedef struct {
    int32_t tz_minuteswest;
    int32_t tz_dsttime;
} minios_timezone_t;

typedef struct {
    int64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t __pad;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
} minios_sysinfo_t;

typedef enum {
    MINIOS_USERPROC_FD_NONE = 0,
    MINIOS_USERPROC_FD_FILE = 1,
    MINIOS_USERPROC_FD_DIR = 2,
    MINIOS_USERPROC_FD_STDIO = 3,
} minios_userproc_fd_kind_t;

uint64_t g_userproc_return_rip;
uint64_t g_userproc_return_stack;
uint64_t g_userproc_current_app_id;
uint64_t g_userproc_running;
uint64_t g_userproc_syscall_stack_top;
uint64_t g_userproc_syscall_user_rsp;

static uint64_t g_userproc_fs_base;
static uint64_t g_userproc_gs_base;
static uint64_t g_userproc_tid_addr;
static uint64_t g_userproc_robust_list_head;
static uint64_t g_userproc_robust_list_len;
static uint64_t g_userproc_mmap_next = MINIOS_USERPROC_MMAP_BASE;
static bool g_userproc_strict_user_memory;
static char g_userproc_exe_path[MINIOS_EXECVE_MAX_PATH];
static minios_userproc_fd_kind_t g_userproc_fd_kinds[MINIOS_USERPROC_MAX_FDS];
static char g_userproc_fd_paths[MINIOS_USERPROC_MAX_FDS][MINIOS_EXECVE_MAX_PATH];
static uint8_t g_userproc_fd_owns_vfs[MINIOS_USERPROC_MAX_FDS];
static uint8_t g_userproc_fd_cloexec[MINIOS_USERPROC_MAX_FDS];
static uint64_t g_userproc_fd_stdio_targets[MINIOS_USERPROC_MAX_FDS];
static mvos_vfs_file_t g_userproc_files[MINIOS_USERPROC_MAX_FDS];
static uint8_t g_userproc_execve_stack_scratch[MINIOS_EXECVE_STACK_SCRATCH_SIZE];
static uint8_t g_userproc_execve_kernel_stack[MINIOS_EXECVE_KERNEL_STACK_SIZE] __attribute__((aligned(16)));

enum {
    MINIOS_MSR_EFER = 0xC0000080,
    MINIOS_MSR_STAR = 0xC0000081,
    MINIOS_MSR_LSTAR = 0xC0000082,
    MINIOS_MSR_SFMASK = 0xC0000084,
    MINIOS_EFER_SCE = 1u << 0,
    MINIOS_RFLAGS_IF = 1u << 9,
};

extern void syscall_entry(void);

static uint64_t userproc_rdmsr(uint32_t msr) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void userproc_wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static int userproc_streq(const char *a, const char *b) {
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int userproc_region_contains(const mvos_vmm_region_info_t *region, uint64_t addr) {
    if (region->size == 0 || addr < region->base) {
        return 0;
    }
    uint64_t end = region->base + region->size;
    if (end < region->base) {
        return 0;
    }
    return addr < end;
}

static int userproc_find_region_for_addr(uint64_t addr,
                                         const char *tag,
                                         uint64_t required_flags,
                                         mvos_vmm_region_info_t *out) {
    uint32_t count = vmm_region_count();
    for (uint32_t i = 0; i < count; ++i) {
        mvos_vmm_region_info_t info;
        if (vmm_region_at(i, &info) != 0) {
            continue;
        }
        if (tag != NULL && !userproc_streq(info.tag, tag)) {
            continue;
        }
        if ((info.flags & required_flags) != required_flags) {
            continue;
        }
        if (!userproc_region_contains(&info, addr)) {
            continue;
        }
        if (out != NULL) {
            *out = info;
        }
        return 0;
    }
    return -1;
}

static uint64_t userproc_strlen(const char *s) {
    if (s == NULL) {
        return 0;
    }
    uint64_t len = 0;
    while (s[len] != '\0') {
        ++len;
    }
    return len;
}

static int userproc_stack_write_u64(uint8_t *stack_mem,
                                    uint64_t stack_base,
                                    uint64_t stack_top,
                                    uint64_t addr,
                                    uint64_t value) {
    if (addr < stack_base || addr + sizeof(uint64_t) > stack_top) {
        return -1;
    }
    uint64_t offset = addr - stack_base;
    memcpy(stack_mem + offset, &value, sizeof(value));
    return 0;
}

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
    if (src == NULL) {
        src = "";
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

static int userproc_checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int userproc_align_up_u64(uint64_t value, uint64_t align, uint64_t *out) {
    if (align == 0) {
        *out = value;
        return 0;
    }
    uint64_t mask = align - 1ULL;
    if (value > UINT64_MAX - mask) {
        return -1;
    }
    *out = (value + mask) & ~mask;
    return 0;
}

static int userproc_user_range_ok(uint64_t user_ptr, uint64_t len, uint64_t required_flags) {
    if (!g_userproc_strict_user_memory) {
        return 1;
    }
    return vmm_user_range_check(user_ptr, len, required_flags) == 0;
}

static int64_t userproc_copy_from_user(void *dst, uint64_t user_src, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    if (dst == NULL || user_src == 0 ||
        !userproc_user_range_ok(user_src, len, MVOS_VMM_FLAG_READ)) {
        return -14; /* EFAULT */
    }
    memcpy(dst, (const void *)(uintptr_t)user_src, (size_t)len);
    return 0;
}

static int64_t userproc_copy_to_user(uint64_t user_dst, const void *src, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    if (src == NULL || user_dst == 0 ||
        !userproc_user_range_ok(user_dst, len, MVOS_VMM_FLAG_WRITE)) {
        return -14; /* EFAULT */
    }
    memcpy((void *)(uintptr_t)user_dst, src, (size_t)len);
    return 0;
}

static void userproc_store_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8u) & 0xffu);
}

static void userproc_store_u64_le(uint8_t *dst, uint64_t value) {
    for (uint64_t i = 0; i < 8; ++i) {
        dst[i] = (uint8_t)((value >> (i * 8ULL)) & 0xffu);
    }
}

static int64_t userproc_copy_user_cstr(uint64_t user_ptr, char *dst, uint64_t cap) {
    if (user_ptr == 0 || dst == NULL || cap < 2) {
        return -14; /* EFAULT */
    }

    const volatile char *src = (const volatile char *)(uintptr_t)user_ptr;
    for (uint64_t i = 0; i + 1 < cap; ++i) {
        uint64_t addr = 0;
        if (userproc_checked_add_u64(user_ptr, i, &addr) != 0 ||
            !userproc_user_range_ok(addr, 1, MVOS_VMM_FLAG_READ)) {
            return -14; /* EFAULT */
        }
        char ch = src[i];
        dst[i] = ch;
        if (ch == '\0') {
            return 0;
        }
    }

    dst[cap - 1] = '\0';
    return -7; /* E2BIG */
}

static int64_t userproc_collect_user_strv(uint64_t user_vec,
                                          const char **out_vec,
                                          char storage[][MINIOS_EXECVE_MAX_ARG_STR],
                                          uint64_t max_count,
                                          uint64_t *out_count) {
    if (out_vec == NULL || storage == NULL || out_count == NULL) {
        return -14; /* EFAULT */
    }

    *out_count = 0;
    if (user_vec == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < max_count; ++i) {
        uint64_t ptr = 0;
        int64_t read_rc = userproc_copy_from_user(&ptr, user_vec + i * sizeof(uint64_t), sizeof(ptr));
        if (read_rc != 0) {
            return read_rc;
        }
        if (ptr == 0) {
            *out_count = i;
            return 0;
        }

        int64_t rc = userproc_copy_user_cstr(ptr, storage[i], MINIOS_EXECVE_MAX_ARG_STR);
        if (rc != 0) {
            return rc;
        }
        out_vec[i] = storage[i];
    }

    return -7; /* E2BIG: too many argv/envp entries */
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

static int userproc_fd_to_index(uint64_t fd, uint64_t *out_index) {
    if (fd < MINIOS_USERPROC_FD_BASE) {
        return -1;
    }
    uint64_t index = fd - MINIOS_USERPROC_FD_BASE;
    if (index >= MINIOS_USERPROC_MAX_FDS ||
        g_userproc_fd_kinds[index] != MINIOS_USERPROC_FD_FILE ||
        !g_userproc_files[index].in_use) {
        return -1;
    }
    if (out_index != NULL) {
        *out_index = index;
    }
    return 0;
}

static int userproc_fd_to_any_index(uint64_t fd, uint64_t *out_index) {
    if (fd < MINIOS_USERPROC_FD_BASE) {
        return -1;
    }
    uint64_t index = fd - MINIOS_USERPROC_FD_BASE;
    if (index >= MINIOS_USERPROC_MAX_FDS ||
        g_userproc_fd_kinds[index] == MINIOS_USERPROC_FD_NONE) {
        return -1;
    }
    if (out_index != NULL) {
        *out_index = index;
    }
    return 0;
}

static int userproc_fd_is_valid(uint64_t fd) {
    if (fd <= 2) {
        return 1;
    }
    return userproc_fd_to_any_index(fd, NULL) == 0;
}

static void userproc_clear_fd_index(uint64_t index) {
    if (index >= MINIOS_USERPROC_MAX_FDS) {
        return;
    }
    if (g_userproc_fd_kinds[index] == MINIOS_USERPROC_FD_FILE &&
        g_userproc_fd_owns_vfs[index]) {
        vfs_close(&g_userproc_files[index]);
    }
    memset(&g_userproc_files[index], 0, sizeof(g_userproc_files[index]));
    g_userproc_fd_kinds[index] = MINIOS_USERPROC_FD_NONE;
    g_userproc_fd_paths[index][0] = '\0';
    g_userproc_fd_owns_vfs[index] = 0;
    g_userproc_fd_cloexec[index] = 0;
    g_userproc_fd_stdio_targets[index] = 0;
}

static int userproc_fd_alloc_index(uint64_t *out_index) {
    for (uint64_t i = 0; i < MINIOS_USERPROC_MAX_FDS; ++i) {
        if (g_userproc_fd_kinds[i] == MINIOS_USERPROC_FD_NONE) {
            if (out_index != NULL) {
                *out_index = i;
            }
            return 0;
        }
    }
    return -1;
}

static uint64_t userproc_alloc_fd(mvos_vfs_file_t *file) {
    if (file == NULL || !file->in_use) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t i = 0;
    if (userproc_fd_alloc_index(&i) == 0) {
        g_userproc_files[i] = *file;
        g_userproc_fd_kinds[i] = MINIOS_USERPROC_FD_FILE;
        g_userproc_fd_owns_vfs[i] = 1;
        userproc_copy_cstr(g_userproc_fd_paths[i], sizeof(g_userproc_fd_paths[i]), file->path);
        return MINIOS_USERPROC_FD_BASE + i;
    }
    vfs_close(file);
    return userproc_errno(-24); /* EMFILE */
}

static uint64_t userproc_alloc_dir_fd(const char *path) {
    if (path == NULL) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t i = 0;
    if (userproc_fd_alloc_index(&i) == 0) {
        memset(&g_userproc_files[i], 0, sizeof(g_userproc_files[i]));
        g_userproc_fd_kinds[i] = MINIOS_USERPROC_FD_DIR;
        userproc_copy_cstr(g_userproc_fd_paths[i], sizeof(g_userproc_fd_paths[i]), path);
        return MINIOS_USERPROC_FD_BASE + i;
    }
    return userproc_errno(-24); /* EMFILE */
}

static int userproc_clone_fd_to_index(uint64_t oldfd, uint64_t new_index) {
    if (new_index >= MINIOS_USERPROC_MAX_FDS) {
        return -9; /* EBADF */
    }
    if (oldfd <= 2) {
        memset(&g_userproc_files[new_index], 0, sizeof(g_userproc_files[new_index]));
        g_userproc_fd_kinds[new_index] = MINIOS_USERPROC_FD_STDIO;
        g_userproc_fd_cloexec[new_index] = 0;
        g_userproc_fd_stdio_targets[new_index] = oldfd;
        userproc_copy_cstr(g_userproc_fd_paths[new_index], sizeof(g_userproc_fd_paths[new_index]), "(stdio)");
        return 0;
    }

    uint64_t old_index = 0;
    if (userproc_fd_to_any_index(oldfd, &old_index) != 0) {
        return -9; /* EBADF */
    }
    g_userproc_files[new_index] = g_userproc_files[old_index];
    g_userproc_fd_kinds[new_index] = g_userproc_fd_kinds[old_index];
    g_userproc_fd_owns_vfs[new_index] = 0;
    g_userproc_fd_cloexec[new_index] = 0;
    g_userproc_fd_stdio_targets[new_index] = g_userproc_fd_stdio_targets[old_index];
    userproc_copy_cstr(g_userproc_fd_paths[new_index],
                       sizeof(g_userproc_fd_paths[new_index]),
                       g_userproc_fd_paths[old_index]);
    return 0;
}

static uint64_t userproc_linux_dup(uint64_t oldfd) {
    if (!userproc_fd_is_valid(oldfd)) {
        return userproc_errno(-9); /* EBADF */
    }
    uint64_t new_index = 0;
    if (userproc_fd_alloc_index(&new_index) != 0) {
        return userproc_errno(-24); /* EMFILE */
    }
    int rc = userproc_clone_fd_to_index(oldfd, new_index);
    if (rc != 0) {
        userproc_clear_fd_index(new_index);
        return userproc_errno(rc);
    }
    return MINIOS_USERPROC_FD_BASE + new_index;
}

static uint64_t userproc_linux_dup2(uint64_t oldfd, uint64_t newfd) {
    if (!userproc_fd_is_valid(oldfd)) {
        return userproc_errno(-9); /* EBADF */
    }
    if (newfd < MINIOS_USERPROC_FD_BASE ||
        newfd >= MINIOS_USERPROC_FD_BASE + MINIOS_USERPROC_MAX_FDS) {
        return userproc_errno(-9); /* EBADF */
    }
    if (oldfd == newfd) {
        return newfd;
    }

    uint64_t new_index = newfd - MINIOS_USERPROC_FD_BASE;
    userproc_clear_fd_index(new_index);
    int rc = userproc_clone_fd_to_index(oldfd, new_index);
    if (rc != 0) {
        return userproc_errno(rc);
    }
    return newfd;
}

static uint64_t userproc_linux_dup3(uint64_t oldfd, uint64_t newfd, uint64_t flags) {
    if (oldfd == newfd) {
        return userproc_errno(-22); /* EINVAL */
    }
    if (flags != 0) {
        return userproc_errno(-22); /* EINVAL: close-on-exec is not modeled yet. */
    }
    return userproc_linux_dup2(oldfd, newfd);
}

static void userproc_close_cloexec_fds(void) {
    for (uint64_t i = 0; i < MINIOS_USERPROC_MAX_FDS; ++i) {
        if (g_userproc_fd_cloexec[i]) {
            userproc_clear_fd_index(i);
        }
    }
}

static void userproc_fill_stat(minios_linux_stat_t *st, const mvos_vfs_file_t *file, uint32_t mode) {
    memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = (uint64_t)(uintptr_t)file->path;
    st->st_nlink = 1;
    st->st_mode = mode;
    st->st_size = (int64_t)file->size;
    st->st_blksize = 512;
    st->st_blocks = (int64_t)((file->size + 511ULL) / 512ULL);
}

static void userproc_fill_statx(minios_statx_t *stx, const mvos_vfs_file_t *file, uint32_t mode) {
    memset(stx, 0, sizeof(*stx));
    stx->stx_mask = MINIOS_STATX_BASIC_STATS;
    stx->stx_blksize = 512;
    stx->stx_nlink = 1;
    stx->stx_mode = (uint16_t)mode;
    stx->stx_ino = (uint64_t)(uintptr_t)file->path;
    stx->stx_size = file->size;
    stx->stx_blocks = (file->size + 511ULL) / 512ULL;
    stx->stx_dev_major = 0;
    stx->stx_dev_minor = 1;
}

static int userproc_path_child_name(const char *dir, const char *path, char *name, uint64_t name_cap, int *is_dir) {
    if (dir == NULL || path == NULL || name == NULL || name_cap == 0 || path[0] != '/') {
        return 0;
    }

    const char *rest = NULL;
    if (userproc_streq(dir, "/")) {
        rest = path + 1;
    } else {
        uint64_t dir_len = userproc_strlen(dir);
        for (uint64_t i = 0; i < dir_len; ++i) {
            if (path[i] == '\0' || path[i] != dir[i]) {
                return 0;
            }
        }
        if (path[dir_len] != '/') {
            return 0;
        }
        rest = path + dir_len + 1;
    }
    if (rest == NULL || rest[0] == '\0') {
        return 0;
    }

    uint64_t len = 0;
    while (rest[len] != '\0' && rest[len] != '/') {
        if (len + 1 >= name_cap) {
            return 0;
        }
        name[len] = rest[len];
        ++len;
    }
    if (len == 0) {
        return 0;
    }
    name[len] = '\0';
    if (is_dir != NULL) {
        *is_dir = (rest[len] == '/') ? 1 : 0;
    }
    return 1;
}

typedef struct {
    const char *dir;
    uint64_t target;
    uint64_t count;
    int found;
    char name[64];
    int is_dir;
    char seen[32][64];
    uint64_t seen_count;
} userproc_dir_find_ctx_t;

static void userproc_dir_find_visitor(const char *path, uint64_t size, uint32_t checksum, void *user_data) {
    (void)size;
    (void)checksum;
    userproc_dir_find_ctx_t *ctx = (userproc_dir_find_ctx_t *)user_data;
    if (ctx == NULL || ctx->found) {
        return;
    }

    char name[64];
    int is_dir = 0;
    if (!userproc_path_child_name(ctx->dir, path, name, sizeof(name), &is_dir)) {
        return;
    }
    for (uint64_t i = 0; i < ctx->seen_count; ++i) {
        if (userproc_streq(ctx->seen[i], name)) {
            return;
        }
    }
    if (ctx->seen_count < 32) {
        userproc_copy_cstr(ctx->seen[ctx->seen_count], sizeof(ctx->seen[ctx->seen_count]), name);
        ++ctx->seen_count;
    }
    if (ctx->count == ctx->target) {
        userproc_copy_cstr(ctx->name, sizeof(ctx->name), name);
        ctx->is_dir = is_dir;
        ctx->found = 1;
        return;
    }
    ++ctx->count;
}

static int userproc_dir_child_at(const char *dir, uint64_t index, char *name, uint64_t name_cap, int *is_dir) {
    if (dir == NULL || name == NULL || name_cap == 0) {
        return -1;
    }
    if (index == 0) {
        userproc_copy_cstr(name, name_cap, ".");
        if (is_dir != NULL) {
            *is_dir = 1;
        }
        return 0;
    }
    if (index == 1) {
        userproc_copy_cstr(name, name_cap, "..");
        if (is_dir != NULL) {
            *is_dir = 1;
        }
        return 0;
    }

    userproc_dir_find_ctx_t ctx = {
        .dir = dir,
        .target = index - 2,
    };
    (void)vfs_list(userproc_dir_find_visitor, dir, &ctx);
    if (!ctx.found) {
        return -1;
    }
    userproc_copy_cstr(name, name_cap, ctx.name);
    if (is_dir != NULL) {
        *is_dir = ctx.is_dir;
    }
    return 0;
}

typedef struct {
    const char *dir;
    uint64_t count;
} userproc_dir_count_ctx_t;

static void userproc_dir_count_visitor(const char *path, uint64_t size, uint32_t checksum, void *user_data) {
    (void)size;
    (void)checksum;
    userproc_dir_count_ctx_t *ctx = (userproc_dir_count_ctx_t *)user_data;
    char name[64];
    if (ctx != NULL && userproc_path_child_name(ctx->dir, path, name, sizeof(name), NULL)) {
        ++ctx->count;
    }
}

static int userproc_path_is_directory(const char *path) {
    if (path == NULL) {
        return 0;
    }
    if (userproc_streq(path, "/") || userproc_streq(path, "/tmp")) {
        return 1;
    }
    mvos_vfs_file_t file;
    if (vfs_open(path, &file) == 0) {
        vfs_close(&file);
        return 0;
    }
    userproc_dir_count_ctx_t ctx = { .dir = path };
    (void)vfs_list(userproc_dir_count_visitor, path, &ctx);
    return ctx.count > 0;
}

static int64_t userproc_resolve_path(uint64_t dirfd, uint64_t user_path, char *out, uint64_t out_cap) {
    if (user_path == 0 || out == NULL || out_cap < 2) {
        return -14; /* EFAULT */
    }
    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_copy_user_cstr(user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return rc;
    }
    if (path_buf[0] == '/') {
        userproc_copy_cstr(out, out_cap, path_buf);
        return 0;
    }

    const char *base = "/";
    if (dirfd != (uint64_t)(int64_t)MINIOS_AT_FDCWD) {
        uint64_t index = 0;
        if (userproc_fd_to_any_index(dirfd, &index) != 0 ||
            g_userproc_fd_kinds[index] != MINIOS_USERPROC_FD_DIR) {
            return -2; /* ENOENT */
        }
        base = g_userproc_fd_paths[index];
    }

    uint64_t base_len = userproc_strlen(base);
    uint64_t path_len = userproc_strlen(path_buf);
    uint64_t need_slash = (base_len > 1 && base[base_len - 1] != '/') ? 1 : 0;
    if (base_len + need_slash + path_len + 1 > out_cap) {
        return -36; /* ENAMETOOLONG */
    }
    userproc_copy_cstr(out, out_cap, base);
    uint64_t pos = base_len;
    if (need_slash) {
        out[pos++] = '/';
    }
    for (uint64_t i = 0; i < path_len; ++i) {
        out[pos++] = path_buf[i];
    }
    out[pos] = '\0';
    return 0;
}

static uint64_t userproc_linux_read(uint64_t fd, uint64_t user_buf, uint64_t count) {
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (!userproc_user_range_ok(user_buf, count, MVOS_VMM_FLAG_WRITE)) {
        return userproc_errno(-14); /* EFAULT */
    }

    uint64_t index = 0;
    if (userproc_fd_to_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }

    uint64_t bytes = 0;
    int rc = vfs_read(&g_userproc_files[index], (void *)(uintptr_t)user_buf, count, &bytes);
    if (rc != 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    return bytes;
}

static uint64_t userproc_linux_pread64(uint64_t fd, uint64_t user_buf, uint64_t count, uint64_t offset) {
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (!userproc_user_range_ok(user_buf, count, MVOS_VMM_FLAG_WRITE)) {
        return userproc_errno(-14); /* EFAULT */
    }

    uint64_t index = 0;
    if (userproc_fd_to_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }

    mvos_vfs_file_t cursor = g_userproc_files[index];
    if (offset >= cursor.size) {
        return 0;
    }
    if (offset > (uint64_t)INT64_MAX) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t new_offset = 0;
    if (vfs_seek(&cursor, (int64_t)offset, MINIOS_SEEK_SET, &new_offset) != 0 ||
        new_offset != offset) {
        return userproc_errno(-22); /* EINVAL */
    }

    uint64_t bytes = 0;
    int rc = vfs_read(&cursor, (void *)(uintptr_t)user_buf, count, &bytes);
    if (rc != 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    return bytes;
}

static uint64_t userproc_linux_ioctl(uint64_t fd, uint64_t request, uint64_t argp) {
    (void)request;
    (void)argp;
    if (!userproc_fd_is_valid(fd)) {
        return userproc_errno(-9); /* EBADF */
    }
    return userproc_errno(-25); /* ENOTTY */
}

static uint64_t userproc_linux_rt_sigaction(uint64_t sig,
                                            uint64_t user_act,
                                            uint64_t user_oldact,
                                            uint64_t sigsetsize) {
    if (sig == 0 || sig >= 65 || sigsetsize != MINIOS_RT_SIGSET_SIZE) {
        return userproc_errno(-22); /* EINVAL */
    }
    if (user_act != 0 &&
        !userproc_user_range_ok(user_act, MINIOS_RT_SIGACTION_SIZE, MVOS_VMM_FLAG_READ)) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (user_oldact != 0) {
        uint8_t empty_action[MINIOS_RT_SIGACTION_SIZE];
        memset(empty_action, 0, sizeof(empty_action));
        return userproc_errno(userproc_copy_to_user(user_oldact, empty_action, sizeof(empty_action)));
    }
    return 0;
}

static uint64_t userproc_linux_rt_sigprocmask(uint64_t how,
                                              uint64_t user_set,
                                              uint64_t user_oldset,
                                              uint64_t sigsetsize) {
    (void)how;
    if (sigsetsize != MINIOS_RT_SIGSET_SIZE) {
        return userproc_errno(-22); /* EINVAL */
    }
    if (user_set != 0 && !userproc_user_range_ok(user_set, sigsetsize, MVOS_VMM_FLAG_READ)) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (user_oldset != 0) {
        uint64_t empty_mask = 0;
        return userproc_errno(userproc_copy_to_user(user_oldset, &empty_mask, sizeof(empty_mask)));
    }
    return 0;
}

static uint64_t userproc_linux_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg) {
    if (!userproc_fd_is_valid(fd)) {
        return userproc_errno(-9); /* EBADF */
    }

    switch (cmd) {
        case MINIOS_F_GETFD: {
            if (fd < MINIOS_USERPROC_FD_BASE) {
                return 0;
            }
            uint64_t index = 0;
            if (userproc_fd_to_any_index(fd, &index) != 0) {
                return userproc_errno(-9); /* EBADF */
            }
            return g_userproc_fd_cloexec[index] ? MINIOS_FD_CLOEXEC : 0;
        }
        case MINIOS_F_SETFD: {
            if (fd < MINIOS_USERPROC_FD_BASE) {
                return 0;
            }
            uint64_t index = 0;
            if (userproc_fd_to_any_index(fd, &index) != 0) {
                return userproc_errno(-9); /* EBADF */
            }
            g_userproc_fd_cloexec[index] = ((arg & MINIOS_FD_CLOEXEC) != 0) ? 1 : 0;
            return 0;
        }
        case MINIOS_F_GETFL:
            if (fd == 1 || fd == 2) {
                return MINIOS_O_WRONLY;
            }
            if (fd >= MINIOS_USERPROC_FD_BASE) {
                uint64_t index = 0;
                if (userproc_fd_to_any_index(fd, &index) == 0 &&
                    g_userproc_fd_kinds[index] == MINIOS_USERPROC_FD_STDIO &&
                    (g_userproc_fd_stdio_targets[index] == 1 || g_userproc_fd_stdio_targets[index] == 2)) {
                    return MINIOS_O_WRONLY;
                }
            }
            return 0;
        case MINIOS_F_SETFL:
            return 0;
        default:
            return userproc_errno(-22); /* EINVAL */
    }
}

static uint64_t userproc_linux_madvise(uint64_t addr, uint64_t length, uint64_t advice) {
    (void)advice;
    if (length == 0) {
        return 0;
    }
    if (addr == 0 || (addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t map_len = 0;
    if (userproc_align_up_u64(length, MVOS_VMM_PAGE_SIZE, &map_len) != 0 ||
        !userproc_user_range_ok(addr, map_len, 0)) {
        return userproc_errno(-22); /* EINVAL */
    }
    return 0;
}

static uint64_t userproc_linux_write(uint64_t fd, uint64_t user_buf, uint64_t count) {
    if (fd != 1 && fd != 2) {
        uint64_t index = 0;
        if (userproc_fd_to_any_index(fd, &index) != 0 ||
            g_userproc_fd_kinds[index] != MINIOS_USERPROC_FD_STDIO ||
            (g_userproc_fd_stdio_targets[index] != 1 && g_userproc_fd_stdio_targets[index] != 2)) {
            return userproc_errno(-9); /* EBADF */
        }
    }
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (!userproc_user_range_ok(user_buf, count, MVOS_VMM_FLAG_READ)) {
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

    uint64_t total = 0;
    for (uint64_t i = 0; i < iovcnt; ++i) {
        minios_iovec_t iov;
        int64_t read_rc = userproc_copy_from_user(&iov,
                                                  user_iov + i * sizeof(minios_iovec_t),
                                                  sizeof(iov));
        if (read_rc != 0) {
            return (total > 0) ? total : userproc_errno(read_rc);
        }
        uint64_t n = userproc_linux_write(fd, iov.iov_base, iov.iov_len);
        if (userproc_is_errno(n)) {
            return (total > 0) ? total : n;
        }
        total += n;
    }
    return total;
}

static uint64_t userproc_mmap_flags_from_prot(uint64_t prot) {
    uint64_t flags = MVOS_VMM_FLAG_USER;
    if ((prot & MINIOS_PROT_READ) != 0) {
        flags |= MVOS_VMM_FLAG_READ;
    }
    if ((prot & MINIOS_PROT_WRITE) != 0) {
        flags |= MVOS_VMM_FLAG_WRITE;
    }
    if ((prot & MINIOS_PROT_EXEC) != 0) {
        flags |= MVOS_VMM_FLAG_EXEC;
    }
    return flags;
}

static int userproc_back_user_pages(uint64_t base, uint64_t size, uint64_t flags) {
    uint64_t end = 0;
    if (userproc_checked_add_u64(base, size, &end) != 0) {
        return -1;
    }
    for (uint64_t page = base; page < end; page += MVOS_VMM_PAGE_SIZE) {
        void *backing = NULL;
        if (vmm_map_user_backed_page(page, flags, &backing) != 0) {
            (void)vmm_unmap_range(base, page - base);
            return -1;
        }
    }
    return 0;
}

static int64_t userproc_copy_file_to_mapping(mvos_vfs_file_t *file,
                                             uint64_t file_offset,
                                             uint64_t user_addr,
                                             uint64_t length) {
    if (length == 0) {
        return 0;
    }
    if (file == NULL || !file->in_use) {
        return -9; /* EBADF */
    }
    if (!g_userproc_strict_user_memory) {
        return 0;
    }
    if (file_offset > file->size) {
        return -22; /* EINVAL */
    }

    mvos_vfs_file_t cursor = *file;
    uint64_t new_offset = 0;
    if (file_offset > (uint64_t)INT64_MAX) {
        return -22; /* EINVAL */
    }
    if (vfs_seek(&cursor, (int64_t)file_offset, MINIOS_SEEK_SET, &new_offset) != 0 ||
        new_offset != file_offset) {
        return -22; /* EINVAL */
    }

    uint64_t remaining = file->size - file_offset;
    uint64_t to_copy = (length < remaining) ? length : remaining;
    uint8_t scratch[256];
    uint64_t copied = 0;
    while (copied < to_copy) {
        uint64_t chunk = to_copy - copied;
        if (chunk > sizeof(scratch)) {
            chunk = sizeof(scratch);
        }
        uint64_t bytes = 0;
        if (vfs_read(&cursor, scratch, chunk, &bytes) != 0) {
            return -14; /* EFAULT */
        }
        if (bytes == 0) {
            break;
        }
        int64_t copy_rc = userproc_copy_to_user(user_addr + copied, scratch, bytes);
        if (copy_rc != 0) {
            return copy_rc;
        }
        copied += bytes;
    }
    return 0;
}

static uint64_t userproc_linux_mmap(uint64_t addr,
                                    uint64_t length,
                                    uint64_t prot,
                                    uint64_t flags,
                                    uint64_t fd,
                                    uint64_t offset) {
    if (length == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    if ((flags & MINIOS_MAP_PRIVATE) == 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    int is_anonymous = (flags & MINIOS_MAP_ANONYMOUS) != 0;
    uint64_t fd_index = 0;
    if (is_anonymous) {
        if (fd != (uint64_t)-1 && fd != UINT64_MAX) {
            return userproc_errno(-9); /* EBADF */
        }
    } else if (userproc_fd_to_index(fd, &fd_index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }
    if ((offset & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    uint64_t map_len = 0;
    if (userproc_align_up_u64(length, MVOS_VMM_PAGE_SIZE, &map_len) != 0 || map_len == 0) {
        return userproc_errno(-12); /* ENOMEM */
    }

    uint64_t map_addr = 0;
    if ((flags & MINIOS_MAP_FIXED) != 0) {
        if (addr == 0 || (addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
            return userproc_errno(-22); /* EINVAL */
        }
        map_addr = addr;
    } else {
        map_addr = g_userproc_mmap_next;
        if (userproc_align_up_u64(map_addr, MVOS_VMM_PAGE_SIZE, &map_addr) != 0) {
            return userproc_errno(-12); /* ENOMEM */
        }
    }

    uint64_t map_end = 0;
    if (userproc_checked_add_u64(map_addr, map_len, &map_end) != 0 ||
        map_end > MINIOS_USERPROC_MMAP_LIMIT) {
        return userproc_errno(-12); /* ENOMEM */
    }

    uint64_t vmm_flags = userproc_mmap_flags_from_prot(prot);
    uint64_t initial_vmm_flags = vmm_flags;
    if (!is_anonymous) {
        initial_vmm_flags |= MVOS_VMM_FLAG_WRITE;
    }
    if (vmm_map_range(map_addr, map_len, initial_vmm_flags, "user-mmap") != 0) {
        return userproc_errno(-12); /* ENOMEM */
    }
    if (userproc_back_user_pages(map_addr, map_len, initial_vmm_flags) != 0) {
        (void)vmm_unmap_range(map_addr, map_len);
        return userproc_errno(-12); /* ENOMEM */
    }
    if (!is_anonymous) {
        int64_t copy_rc = userproc_copy_file_to_mapping(&g_userproc_files[fd_index], offset, map_addr, length);
        if (copy_rc != 0) {
            (void)vmm_unmap_range(map_addr, map_len);
            return userproc_errno(copy_rc);
        }
        if (vmm_protect_range(map_addr, map_len, vmm_flags) != 0) {
            (void)vmm_unmap_range(map_addr, map_len);
            return userproc_errno(-12); /* ENOMEM */
        }
    }

    if ((flags & MINIOS_MAP_FIXED) == 0) {
        g_userproc_mmap_next = map_end;
    }
    return map_addr;
}

static uint64_t userproc_linux_munmap(uint64_t addr, uint64_t length) {
    if (addr == 0 || (addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0 || length == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t map_len = 0;
    if (userproc_align_up_u64(length, MVOS_VMM_PAGE_SIZE, &map_len) != 0 || map_len == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    if (vmm_unmap_range(addr, map_len) != 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    return 0;
}

static uint64_t userproc_linux_mprotect(uint64_t addr, uint64_t length, uint64_t prot) {
    if (addr == 0 || (addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0 || length == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t map_len = 0;
    if (userproc_align_up_u64(length, MVOS_VMM_PAGE_SIZE, &map_len) != 0 || map_len == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t vmm_flags = userproc_mmap_flags_from_prot(prot);
    if (vmm_protect_range(addr, map_len, vmm_flags) != 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    return 0;
}

static uint64_t userproc_linux_getcwd(uint64_t user_buf, uint64_t size) {
    static const char cwd[] = "/";
    if (user_buf == 0 || size == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (size < sizeof(cwd)) {
        return userproc_errno(-34); /* ERANGE */
    }
    int64_t rc = userproc_copy_to_user(user_buf, cwd, sizeof(cwd));
    if (rc != 0) {
        return userproc_errno(rc);
    }
    return user_buf;
}

static int userproc_fill_rlimit(uint64_t resource, minios_rlimit_t *limit) {
    if (limit == NULL) {
        return -14; /* EFAULT */
    }
    switch (resource) {
        case MINIOS_RLIMIT_DATA:
        case MINIOS_RLIMIT_AS:
            limit->rlim_cur = UINT64_MAX;
            limit->rlim_max = UINT64_MAX;
            return 0;
        case MINIOS_RLIMIT_STACK:
            limit->rlim_cur = MINIOS_EXECVE_STACK_SCRATCH_SIZE;
            limit->rlim_max = MINIOS_EXECVE_STACK_SCRATCH_SIZE;
            return 0;
        case MINIOS_RLIMIT_NOFILE:
            limit->rlim_cur = MINIOS_USERPROC_FD_BASE + MINIOS_USERPROC_MAX_FDS;
            limit->rlim_max = MINIOS_USERPROC_FD_BASE + MINIOS_USERPROC_MAX_FDS;
            return 0;
        default:
            return -22; /* EINVAL */
    }
}

static uint64_t userproc_linux_getrlimit(uint64_t resource, uint64_t user_limit) {
    if (user_limit == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    minios_rlimit_t limit;
    int rc = userproc_fill_rlimit(resource, &limit);
    if (rc != 0) {
        return userproc_errno(rc);
    }
    return userproc_errno(userproc_copy_to_user(user_limit, &limit, sizeof(limit)));
}

static uint64_t userproc_linux_prlimit64(uint64_t pid,
                                         uint64_t resource,
                                         uint64_t user_new_limit,
                                         uint64_t user_old_limit) {
    if (pid != 0 && pid != 1000 + g_userproc_current_app_id) {
        return userproc_errno(-3); /* ESRCH */
    }
    if (user_new_limit != 0) {
        return userproc_errno(-1); /* EPERM: limits are fixed in the single-process sandbox. */
    }
    if (user_old_limit == 0) {
        return 0;
    }
    return userproc_linux_getrlimit(resource, user_old_limit);
}

static uint64_t userproc_linux_faccessat(uint64_t dirfd, uint64_t user_path, uint64_t mode, uint64_t flags) {
    (void)mode;
    (void)flags;
    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_resolve_path(dirfd, user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return userproc_errno(rc);
    }

    if (userproc_path_is_directory(path_buf)) {
        return 0;
    }

    mvos_vfs_file_t file;
    int open_rc = vfs_open(path_buf, &file);
    if (open_rc != 0) {
        return userproc_errno(-2); /* ENOENT */
    }
    vfs_close(&file);
    return 0;
}

static uint64_t userproc_linux_clock_gettime(uint64_t clock_id, uint64_t user_tp) {
    if (user_tp == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (clock_id != MINIOS_CLOCK_REALTIME && clock_id != MINIOS_CLOCK_MONOTONIC) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t ticks = timer_ticks();
    minios_timespec_t ts = {
        .tv_sec = (int64_t)(ticks / 100ULL),
        .tv_nsec = (int64_t)((ticks % 100ULL) * 10000000ULL),
    };
    return userproc_errno(userproc_copy_to_user(user_tp, &ts, sizeof(ts)));
}

static uint64_t userproc_linux_gettimeofday(uint64_t user_tv, uint64_t user_tz) {
    uint64_t ticks = timer_ticks();
    if (user_tv != 0) {
        minios_timeval_t tv = {
            .tv_sec = (int64_t)(ticks / 100ULL),
            .tv_usec = (int64_t)((ticks % 100ULL) * 10000ULL),
        };
        int64_t rc = userproc_copy_to_user(user_tv, &tv, sizeof(tv));
        if (rc != 0) {
            return userproc_errno(rc);
        }
    }
    if (user_tz != 0) {
        minios_timezone_t tz = {
            .tz_minuteswest = 0,
            .tz_dsttime = 0,
        };
        int64_t rc = userproc_copy_to_user(user_tz, &tz, sizeof(tz));
        if (rc != 0) {
            return userproc_errno(rc);
        }
    }
    return 0;
}

static uint64_t userproc_linux_sysinfo(uint64_t user_info) {
    if (user_info == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    minios_sysinfo_t info;
    memset(&info, 0, sizeof(info));
    info.uptime = (int64_t)(timer_ticks() / 100ULL);
    info.totalram = 64ULL * 1024ULL * 1024ULL;
    info.freeram = 32ULL * 1024ULL * 1024ULL;
    info.procs = 1;
    info.mem_unit = 1;
    return userproc_errno(userproc_copy_to_user(user_info, &info, sizeof(info)));
}

static uint64_t userproc_linux_sched_getaffinity(uint64_t pid, uint64_t cpusetsize, uint64_t user_mask) {
    if (pid != 0 && pid != 1000 + g_userproc_current_app_id) {
        return userproc_errno(-3); /* ESRCH */
    }
    if (user_mask == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (cpusetsize < sizeof(uint64_t)) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t mask = 1;
    int64_t rc = userproc_copy_to_user(user_mask, &mask, sizeof(mask));
    if (rc != 0) {
        return userproc_errno(rc);
    }
    return sizeof(mask);
}

static uint64_t userproc_linux_set_robust_list(uint64_t head, uint64_t len) {
    if (head == 0 || len == 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    g_userproc_robust_list_head = head;
    g_userproc_robust_list_len = len;
    return 0;
}

static uint64_t userproc_linux_get_robust_list(uint64_t pid, uint64_t user_head, uint64_t user_len) {
    if (pid != 0 && pid != 1000 + g_userproc_current_app_id) {
        return userproc_errno(-3); /* ESRCH */
    }
    if (user_head == 0 || user_len == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    int64_t rc = userproc_copy_to_user(user_head, &g_userproc_robust_list_head, sizeof(g_userproc_robust_list_head));
    if (rc != 0) {
        return userproc_errno(rc);
    }
    rc = userproc_copy_to_user(user_len, &g_userproc_robust_list_len, sizeof(g_userproc_robust_list_len));
    if (rc != 0) {
        return userproc_errno(rc);
    }
    return 0;
}

static uint64_t userproc_linux_futex(uint64_t user_uaddr,
                                     uint64_t op,
                                     uint64_t val,
                                     uint64_t user_timeout,
                                     uint64_t user_uaddr2,
                                     uint64_t val3) {
    (void)user_timeout;
    (void)user_uaddr2;
    (void)val3;
    if (user_uaddr == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    uint64_t cmd = op & MINIOS_FUTEX_CMD_MASK;
    switch (cmd) {
        case MINIOS_FUTEX_WAKE:
            return 0;
        case MINIOS_FUTEX_WAIT: {
            uint32_t current = 0;
            int64_t rc = userproc_copy_from_user(&current, user_uaddr, sizeof(current));
            if (rc != 0) {
                return userproc_errno(rc);
            }
            if (current != (uint32_t)val) {
                return userproc_errno(-11); /* EAGAIN */
            }
            return userproc_errno(-11); /* EAGAIN: miniOS cannot block a single userspace thread yet. */
        }
        default:
            return userproc_errno(-38); /* ENOSYS */
    }
}

static uint64_t userproc_linux_getrandom(uint64_t user_buf, uint64_t count, uint64_t flags) {
    (void)flags;
    if (count == 0) {
        return 0;
    }
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    uint64_t to_fill = (count > 256ULL) ? 256ULL : count;
    uint64_t seed = 0x9e3779b97f4a7c15ULL ^ timer_ticks() ^ g_userproc_current_app_id;
    uint8_t chunk[64];
    uint64_t written = 0;
    while (written < to_fill) {
        uint64_t n = to_fill - written;
        if (n > sizeof(chunk)) {
            n = sizeof(chunk);
        }
        for (uint64_t i = 0; i < n; ++i) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            chunk[i] = (uint8_t)(seed >> 8);
        }
        int64_t rc = userproc_copy_to_user(user_buf + written, chunk, n);
        if (rc != 0) {
            return (written > 0) ? written : userproc_errno(rc);
        }
        written += n;
    }
    return written;
}

static uint64_t userproc_linux_close(uint64_t fd) {
    if (fd < MINIOS_USERPROC_FD_BASE) {
        return 0;
    }

    uint64_t index = 0;
    if (userproc_fd_to_any_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }
    userproc_clear_fd_index(index);
    return 0;
}

static uint64_t userproc_linux_fstat(uint64_t fd, uint64_t user_stat) {
    if (user_stat == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    minios_linux_stat_t st;
    if (fd < MINIOS_USERPROC_FD_BASE) {
        mvos_vfs_file_t stream = {
            .size = 0,
            .path = "(stdio)",
            .in_use = 1,
        };
        userproc_fill_stat(&st, &stream, MINIOS_S_IFCHR | 0600);
        return userproc_errno(userproc_copy_to_user(user_stat, &st, sizeof(st)));
    }

    uint64_t index = 0;
    if (userproc_fd_to_any_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }
    if (g_userproc_fd_kinds[index] == MINIOS_USERPROC_FD_STDIO) {
        mvos_vfs_file_t stream = {
            .size = 0,
            .path = "(stdio)",
            .in_use = 1,
        };
        userproc_fill_stat(&st, &stream, MINIOS_S_IFCHR | 0600);
    } else if (g_userproc_fd_kinds[index] == MINIOS_USERPROC_FD_DIR) {
        mvos_vfs_file_t dir = {
            .size = 0,
            .path = g_userproc_fd_paths[index],
            .in_use = 1,
        };
        userproc_fill_stat(&st, &dir, MINIOS_S_IFDIR | 0555);
    } else {
        userproc_fill_stat(&st, &g_userproc_files[index], MINIOS_S_IFREG | 0444);
    }
    return userproc_errno(userproc_copy_to_user(user_stat, &st, sizeof(st)));
}

static uint64_t userproc_linux_lseek(uint64_t fd, uint64_t offset, uint64_t whence) {
    uint64_t index = 0;
    if (userproc_fd_to_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }
    if (whence > MINIOS_SEEK_END) {
        return userproc_errno(-22); /* EINVAL */
    }
    uint64_t new_offset = 0;
    int rc = vfs_seek(&g_userproc_files[index], (int64_t)offset, (int)whence, &new_offset);
    if (rc != 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    return new_offset;
}

static uint64_t userproc_linux_openat(uint64_t dirfd, uint64_t user_path, uint64_t flags, uint64_t mode) {
    (void)mode;
    if ((flags & MINIOS_O_ACCMODE) != 0) {
        return userproc_errno(-13); /* EACCES: VFS syscall bridge is read-only for now. */
    }

    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_resolve_path(dirfd, user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return userproc_errno(rc);
    }

    if (userproc_path_is_directory(path_buf)) {
        return userproc_alloc_dir_fd(path_buf);
    }

    mvos_vfs_file_t file;
    int open_rc = vfs_open(path_buf, &file);
    if (open_rc == -2) {
        return userproc_errno(-2); /* ENOENT */
    }
    if ((flags & MINIOS_O_DIRECTORY) != 0) {
        if (open_rc == 0) {
            vfs_close(&file);
        }
        return userproc_errno(-20); /* ENOTDIR */
    }
    if (open_rc == -3) {
        return userproc_errno(-24); /* EMFILE */
    }
    if (open_rc != 0) {
        return userproc_errno(-22); /* EINVAL */
    }
    return userproc_alloc_fd(&file);
}

static uint64_t userproc_linux_newfstatat(uint64_t dirfd, uint64_t user_path, uint64_t user_stat, uint64_t flags) {
    (void)flags;
    if (user_path == 0 || user_stat == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_resolve_path(dirfd, user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return userproc_errno(rc);
    }

    minios_linux_stat_t st;
    if (userproc_path_is_directory(path_buf)) {
        mvos_vfs_file_t dir = {
            .size = 0,
            .path = path_buf,
            .in_use = 1,
        };
        userproc_fill_stat(&st, &dir, MINIOS_S_IFDIR | 0555);
        return userproc_errno(userproc_copy_to_user(user_stat, &st, sizeof(st)));
    }

    mvos_vfs_file_t file;
    int open_rc = vfs_open(path_buf, &file);
    if (open_rc == -2) {
        return userproc_errno(-2); /* ENOENT */
    }
    if (open_rc != 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    userproc_fill_stat(&st, &file, MINIOS_S_IFREG | 0444);
    vfs_close(&file);
    return userproc_errno(userproc_copy_to_user(user_stat, &st, sizeof(st)));
}

static uint64_t userproc_linux_statx(uint64_t dirfd,
                                     uint64_t user_path,
                                     uint64_t flags,
                                     uint64_t mask,
                                     uint64_t user_statx) {
    (void)flags;
    (void)mask;
    if (user_path == 0 || user_statx == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_resolve_path(dirfd, user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return userproc_errno(rc);
    }

    minios_statx_t stx;
    if (userproc_path_is_directory(path_buf)) {
        mvos_vfs_file_t dir = {
            .size = 0,
            .path = path_buf,
            .in_use = 1,
        };
        userproc_fill_statx(&stx, &dir, MINIOS_S_IFDIR | 0555);
        return userproc_errno(userproc_copy_to_user(user_statx, &stx, sizeof(stx)));
    }

    mvos_vfs_file_t file;
    int open_rc = vfs_open(path_buf, &file);
    if (open_rc == -2) {
        return userproc_errno(-2); /* ENOENT */
    }
    if (open_rc != 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    userproc_fill_statx(&stx, &file, MINIOS_S_IFREG | 0444);
    vfs_close(&file);
    return userproc_errno(userproc_copy_to_user(user_statx, &stx, sizeof(stx)));
}

static uint64_t userproc_linux_getdents64(uint64_t fd, uint64_t user_dirp, uint64_t count) {
    if (user_dirp == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (count == 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    uint64_t index = 0;
    if (userproc_fd_to_any_index(fd, &index) != 0) {
        return userproc_errno(-9); /* EBADF */
    }
    if (g_userproc_fd_kinds[index] != MINIOS_USERPROC_FD_DIR) {
        return userproc_errno(-20); /* ENOTDIR */
    }

    uint64_t written = 0;
    for (;;) {
        char name[64];
        int is_dir = 0;
        uint64_t entry_index = g_userproc_files[index].cursor;
        if (userproc_dir_child_at(g_userproc_fd_paths[index], entry_index, name, sizeof(name), &is_dir) != 0) {
            break;
        }

        uint64_t name_len = userproc_strlen(name);
        uint64_t reclen = 19ULL + name_len + 1ULL;
        if (userproc_align_up_u64(reclen, 8, &reclen) != 0 || reclen > sizeof(uint64_t) * 16ULL) {
            return (written > 0) ? written : userproc_errno(-22); /* EINVAL */
        }
        if (reclen > count - written) {
            return (written > 0) ? written : userproc_errno(-22); /* EINVAL */
        }

        uint8_t record[sizeof(uint64_t) * 16ULL];
        memset(record, 0, sizeof(record));
        uint64_t ino = entry_index + 1ULL;
        uint64_t next_off = entry_index + 1ULL;
        uint16_t reclen16 = (uint16_t)reclen;
        uint8_t dtype = is_dir ? MINIOS_DT_DIR : MINIOS_DT_REG;
        userproc_store_u64_le(record + 0, ino);
        userproc_store_u64_le(record + 8, next_off);
        userproc_store_u16_le(record + 16, reclen16);
        record[18] = dtype;
        for (uint64_t i = 0; i <= name_len; ++i) {
            record[19 + i] = (uint8_t)name[i];
        }

        int64_t copy_rc = userproc_copy_to_user(user_dirp + written, record, reclen);
        if (copy_rc != 0) {
            return (written > 0) ? written : userproc_errno(copy_rc);
        }
        written += reclen;
        g_userproc_files[index].cursor = entry_index + 1ULL;
    }
    return written;
}

static uint64_t userproc_linux_readlinkat(uint64_t dirfd, uint64_t user_path, uint64_t user_buf, uint64_t bufsiz) {
    if (user_path == 0 || user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }
    if (bufsiz == 0) {
        return userproc_errno(-22); /* EINVAL */
    }

    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_copy_user_cstr(user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return userproc_errno(rc);
    }
    if (path_buf[0] != '/' && dirfd != (uint64_t)(int64_t)MINIOS_AT_FDCWD) {
        return userproc_errno(-2); /* ENOENT */
    }
    if (!userproc_streq(path_buf, "/proc/self/exe") &&
        !userproc_streq(path_buf, "/proc/curproc/file")) {
        return userproc_errno(-2); /* ENOENT */
    }
    if (g_userproc_exe_path[0] == '\0') {
        return userproc_errno(-2); /* ENOENT */
    }

    uint64_t len = strlen(g_userproc_exe_path);
    uint64_t to_copy = (len < bufsiz) ? len : bufsiz;
    int64_t copy_rc = userproc_copy_to_user(user_buf, g_userproc_exe_path, to_copy);
    if (copy_rc != 0) {
        return userproc_errno(copy_rc);
    }
    return to_copy;
}

static uint64_t userproc_linux_brk(uint64_t new_brk) {
    return vmm_user_brk_set(new_brk);
}

static uint64_t userproc_linux_uname(uint64_t user_buf) {
    if (user_buf == 0) {
        return userproc_errno(-14); /* EFAULT */
    }

    minios_utsname_t u;
    userproc_copy_cstr(u.sysname, sizeof(u.sysname), "miniOS");
    userproc_copy_cstr(u.nodename, sizeof(u.nodename), "miniOS-node");
    userproc_copy_cstr(u.release, sizeof(u.release), "0.43");
    userproc_copy_cstr(u.version, sizeof(u.version), "Stage3 phase43 x86-64 syscall entry");
    userproc_copy_cstr(u.machine, sizeof(u.machine), "x86_64");
    userproc_copy_cstr(u.domainname, sizeof(u.domainname), "miniOS.local");
    return userproc_errno(userproc_copy_to_user(user_buf, &u, sizeof(u)));
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
            return userproc_errno(userproc_copy_to_user(arg, &g_userproc_fs_base, sizeof(g_userproc_fs_base)));
        case MINIOS_ARCH_GET_GS:
            if (arg == 0) {
                return userproc_errno(-14); /* EFAULT */
            }
            return userproc_errno(userproc_copy_to_user(arg, &g_userproc_gs_base, sizeof(g_userproc_gs_base)));
        default:
            return userproc_errno(-22); /* EINVAL */
    }
}

static int64_t userproc_linux_execve(uint64_t user_path,
                                     uint64_t user_argv,
                                     uint64_t user_envp,
                                     mvos_userimg_report_t *out_report,
                                     mvos_user_stack_layout_t *out_layout) {
    if (out_report == NULL || out_layout == NULL) {
        return -14; /* EFAULT */
    }

    char path_buf[MINIOS_EXECVE_MAX_PATH];
    int64_t rc = userproc_copy_user_cstr(user_path, path_buf, sizeof(path_buf));
    if (rc != 0) {
        return rc;
    }

    const char *exec_path = path_buf;
    if (userproc_streq(path_buf, "hello_linux_tiny") ||
        userproc_streq(path_buf, "/bin/hello_linux_tiny")) {
        exec_path = "/boot/init/hello_linux_tiny";
    }

    const char *argv_vec[MINIOS_EXEC_STACK_MAX_ARGC];
    const char *envp_vec[MINIOS_EXEC_STACK_MAX_ENVC];
    char argv_storage[MINIOS_EXEC_STACK_MAX_ARGC][MINIOS_EXECVE_MAX_ARG_STR];
    char envp_storage[MINIOS_EXEC_STACK_MAX_ENVC][MINIOS_EXECVE_MAX_ARG_STR];
    uint64_t argc = 0;
    uint64_t envc = 0;

    rc = userproc_collect_user_strv(user_argv,
                                    argv_vec,
                                    argv_storage,
                                    MINIOS_EXEC_STACK_MAX_ARGC,
                                    &argc);
    if (rc != 0) {
        return rc;
    }
    rc = userproc_collect_user_strv(user_envp,
                                    envp_vec,
                                    envp_storage,
                                    MINIOS_EXEC_STACK_MAX_ENVC,
                                    &envc);
    if (rc != 0) {
        return rc;
    }

    const char *default_argv0[1];
    int use_default_argv = 0;
    if (argc == 0) {
        default_argv0[0] = path_buf;
        argc = 1;
        use_default_argv = 1;
    }
    const char *const *argv_ptr = use_default_argv ? default_argv0 : argv_vec;
    const char *const *envp_ptr = (envc == 0) ? NULL : envp_vec;

    mvos_vfs_file_t exec_file;
    int open_rc = vfs_open(exec_path, &exec_file);
    if (open_rc != 0) {
        return -2; /* ENOENT */
    }

    mvos_userimg_report_t report;
    mvos_userimg_result_t img_rc = userimg_prepare_image((const uint8_t *)exec_file.data, exec_file.size, &report);
    vfs_close(&exec_file);
    if (img_rc != MVOS_USERIMG_OK) {
        return -8; /* ENOEXEC */
    }

    if (userproc_handoff_dry_run(report.mapped_entry, report.stack_top) != 0) {
        return -8; /* ENOEXEC */
    }

    if (report.stack_size > MINIOS_EXECVE_STACK_SCRATCH_SIZE) {
        return -12; /* ENOMEM */
    }

    mvos_user_stack_layout_t layout;
    int stack_rc = userproc_prepare_exec_stack(
        g_userproc_execve_stack_scratch,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        argv_ptr,
        argc,
        envp_ptr,
        envc,
        &layout);
    if (stack_rc != 0) {
        switch (stack_rc) {
            case -1:
            case -4:
                return -14; /* EFAULT */
            case -3:
            case -5:
                return -7; /* E2BIG */
            default:
                return -8; /* ENOEXEC */
        }
    }

    if (vmm_copy_to_user(report.stack_base, g_userproc_execve_stack_scratch, report.stack_size) != 0) {
        return -8; /* ENOEXEC */
    }

    userproc_close_cloexec_fds();
    userproc_copy_cstr(g_userproc_exe_path, sizeof(g_userproc_exe_path), exec_path);
    *out_report = report;
    *out_layout = layout;
    return 0;
}

void userproc_set_return_context(uint64_t return_rip, uint64_t return_stack) {
    g_userproc_return_rip = return_rip;
    g_userproc_return_stack = return_stack;
}

void userproc_set_current_app_id(uint64_t app_id) {
    g_userproc_current_app_id = app_id;
}

void userproc_syscall_init(void) {
    uint64_t efer = userproc_rdmsr(MINIOS_MSR_EFER);
    userproc_wrmsr(MINIOS_MSR_EFER, efer | MINIOS_EFER_SCE);

    uint64_t star = ((uint64_t)MINIOS_GDT_SELECTOR_USER_CODE << 48) |
                    ((uint64_t)MINIOS_GDT_SELECTOR_KERNEL_CODE << 32);
    userproc_wrmsr(MINIOS_MSR_STAR, star);
    userproc_wrmsr(MINIOS_MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    userproc_wrmsr(MINIOS_MSR_SFMASK, MINIOS_RFLAGS_IF);

    g_userproc_syscall_stack_top = (uint64_t)(uintptr_t)(g_userproc_execve_kernel_stack +
                                                        MINIOS_EXECVE_KERNEL_STACK_SIZE);
}

const char *userproc_handoff_result_name(int rc) {
    switch (rc) {
        case 0:
            return "ok";
        case -1:
            return "null-entry-or-stack";
        case -2:
            return "stack-not-page-aligned";
        case -3:
            return "entry-region-missing-or-not-exec";
        case -4:
            return "stack-region-missing-or-not-writable";
        default:
            return "unknown";
    }
}

int userproc_handoff_dry_run(uint64_t entry, uint64_t user_stack_top) {
    if (entry == 0 || user_stack_top == 0) {
        return -1;
    }
    if ((user_stack_top & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        return -2;
    }

    if (userproc_find_region_for_addr(entry,
                                      "userimg-load",
                                      MVOS_VMM_FLAG_USER | MVOS_VMM_FLAG_EXEC,
                                      NULL) != 0) {
        return -3;
    }

    mvos_vmm_region_info_t stack_region;
    if (userproc_find_region_for_addr(user_stack_top - 1ULL,
                                      "userimg-stack",
                                      MVOS_VMM_FLAG_USER | MVOS_VMM_FLAG_WRITE,
                                      &stack_region) != 0) {
        return -4;
    }
    if (user_stack_top <= stack_region.base) {
        return -4;
    }
    return 0;
}

const char *userproc_stack_result_name(int rc) {
    switch (rc) {
        case 0:
            return "ok";
        case -1:
            return "null-arg";
        case -2:
            return "invalid-stack-range";
        case -3:
            return "count-limit-exceeded";
        case -4:
            return "missing-argv-envp-entry";
        case -5:
            return "stack-overflow";
        case -6:
            return "unaligned-stack-top";
        case -7:
            return "stack-write-failed";
        default:
            return "unknown";
    }
}

int userproc_prepare_exec_stack(uint8_t *stack_mem,
                                uint64_t stack_size,
                                uint64_t stack_base,
                                uint64_t stack_top,
                                const char *const *argv,
                                uint64_t argc,
                                const char *const *envp,
                                uint64_t envc,
                                mvos_user_stack_layout_t *out) {
    if (stack_mem == NULL || out == NULL) {
        return -1;
    }
    if (stack_top <= stack_base || stack_size == 0 || stack_top - stack_base != stack_size) {
        return -2;
    }
    if ((stack_top & 0xFULL) != 0) {
        return -6;
    }
    if (argc > MINIOS_EXEC_STACK_MAX_ARGC || envc > MINIOS_EXEC_STACK_MAX_ENVC) {
        return -3;
    }
    if ((argc > 0 && argv == NULL) || (envc > 0 && envp == NULL)) {
        return -4;
    }

    uint64_t argv_ptrs[MINIOS_EXEC_STACK_MAX_ARGC];
    uint64_t env_ptrs[MINIOS_EXEC_STACK_MAX_ENVC];
    uint64_t sp = stack_top;
    uint64_t strings_bytes = 0;
    memset(stack_mem, 0, (size_t)stack_size);

    for (uint64_t i = argc; i > 0; --i) {
        const char *s = argv[i - 1];
        if (s == NULL) {
            return -4;
        }
        uint64_t len = userproc_strlen(s) + 1ULL;
        if (sp < stack_base + len) {
            return -5;
        }
        sp -= len;
        uint64_t off = sp - stack_base;
        memcpy(stack_mem + off, s, (size_t)len);
        argv_ptrs[i - 1] = sp;
        strings_bytes += len;
    }

    for (uint64_t i = envc; i > 0; --i) {
        const char *s = envp[i - 1];
        if (s == NULL) {
            return -4;
        }
        uint64_t len = userproc_strlen(s) + 1ULL;
        if (sp < stack_base + len) {
            return -5;
        }
        sp -= len;
        uint64_t off = sp - stack_base;
        memcpy(stack_mem + off, s, (size_t)len);
        env_ptrs[i - 1] = sp;
        strings_bytes += len;
    }

    sp &= ~0xFULL;

    if (sp < stack_base + 16ULL) {
        return -5;
    }
    sp -= 16ULL;
    if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, 0) != 0 ||
        userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp + 8ULL, 0) != 0) {
        return -7;
    }
    uint64_t auxv_user = sp;

    if (sp < stack_base + 8ULL) {
        return -5;
    }
    sp -= 8ULL;
    if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, 0) != 0) {
        return -7;
    }
    for (uint64_t i = envc; i > 0; --i) {
        if (sp < stack_base + 8ULL) {
            return -5;
        }
        sp -= 8ULL;
        if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, env_ptrs[i - 1]) != 0) {
            return -7;
        }
    }
    uint64_t envp_user = sp;

    if (sp < stack_base + 8ULL) {
        return -5;
    }
    sp -= 8ULL;
    if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, 0) != 0) {
        return -7;
    }
    for (uint64_t i = argc; i > 0; --i) {
        if (sp < stack_base + 8ULL) {
            return -5;
        }
        sp -= 8ULL;
        if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, argv_ptrs[i - 1]) != 0) {
            return -7;
        }
    }
    uint64_t argv_user = sp;

    if (sp < stack_base + 8ULL) {
        return -5;
    }
    sp -= 8ULL;
    if (userproc_stack_write_u64(stack_mem, stack_base, stack_top, sp, argc) != 0) {
        return -7;
    }

    out->initial_rsp = sp;
    out->argc = argc;
    out->argv_user = argv_user;
    out->envp_user = envp_user;
    out->auxv_user = auxv_user;
    out->strings_bytes = strings_bytes;
    out->used_bytes = stack_top - sp;
    return 0;
}

uint64_t userproc_dispatch(uint64_t syscall,
                           uint64_t arg1,
                           uint64_t arg2,
                           uint64_t arg3,
                           uint64_t arg4,
                           uint64_t arg5,
                           uint64_t arg6) {
    (void)arg6;
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
        case MINIOS_LINUX_SYSCALL_READ:
            return userproc_linux_read(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_WRITE:
            return userproc_linux_write(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_CLOSE:
            return userproc_linux_close(arg1);
        case MINIOS_LINUX_SYSCALL_FSTAT:
            return userproc_linux_fstat(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_LSEEK:
            return userproc_linux_lseek(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_MMAP:
            return userproc_linux_mmap(arg1, arg2, arg3, arg4, arg5, arg6);
        case MINIOS_LINUX_SYSCALL_MPROTECT:
            return userproc_linux_mprotect(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_MUNMAP:
            return userproc_linux_munmap(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_RT_SIGACTION:
            return userproc_linux_rt_sigaction(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_RT_SIGPROCMASK:
            return userproc_linux_rt_sigprocmask(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_IOCTL:
            return userproc_linux_ioctl(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_WRITEV:
            return userproc_linux_writev(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_PREAD64:
            return userproc_linux_pread64(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_BRK:
            return userproc_linux_brk(arg1);
        case MINIOS_LINUX_SYSCALL_MADVISE:
            return userproc_linux_madvise(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_DUP:
            return userproc_linux_dup(arg1);
        case MINIOS_LINUX_SYSCALL_DUP2:
            return userproc_linux_dup2(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_ACCESS:
            return userproc_linux_faccessat((uint64_t)(int64_t)MINIOS_AT_FDCWD, arg1, arg2, 0);
        case MINIOS_LINUX_SYSCALL_UNAME:
            return userproc_linux_uname(arg1);
        case MINIOS_LINUX_SYSCALL_FCNTL:
            return userproc_linux_fcntl(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_GETRLIMIT:
            return userproc_linux_getrlimit(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_READLINK:
            return userproc_linux_readlinkat((uint64_t)(int64_t)MINIOS_AT_FDCWD, arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_GETCWD:
            return userproc_linux_getcwd(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_GETPID:
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_GETTID:
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_SCHED_GETAFFINITY:
            return userproc_linux_sched_getaffinity(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_GETPPID:
            return 1;
        case MINIOS_LINUX_SYSCALL_GETUID:
        case MINIOS_LINUX_SYSCALL_GETGID:
        case MINIOS_LINUX_SYSCALL_GETEUID:
        case MINIOS_LINUX_SYSCALL_GETEGID:
            return MINIOS_USERPROC_ROOT_ID;
        case MINIOS_LINUX_SYSCALL_GETDENTS64:
            return userproc_linux_getdents64(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS:
            g_userproc_tid_addr = arg1;
            return 1000 + g_userproc_current_app_id;
        case MINIOS_LINUX_SYSCALL_ARCH_PRCTL:
            return userproc_linux_arch_prctl(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_FUTEX:
            return userproc_linux_futex(arg1, arg2, arg3, arg4, arg5, arg6);
        case MINIOS_LINUX_SYSCALL_CLOCK_GETTIME:
            return userproc_linux_clock_gettime(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_GETTIMEOFDAY:
            return userproc_linux_gettimeofday(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_SYSINFO:
            return userproc_linux_sysinfo(arg1);
        case MINIOS_LINUX_SYSCALL_OPENAT:
            return userproc_linux_openat(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_NEWFSTATAT:
            return userproc_linux_newfstatat(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_READLINKAT:
            return userproc_linux_readlinkat(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_FACCESSAT:
            return userproc_linux_faccessat(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_SET_ROBUST_LIST:
            return userproc_linux_set_robust_list(arg1, arg2);
        case MINIOS_LINUX_SYSCALL_GET_ROBUST_LIST:
            return userproc_linux_get_robust_list(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_DUP3:
            return userproc_linux_dup3(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_PRLIMIT64:
            return userproc_linux_prlimit64(arg1, arg2, arg3, arg4);
        case MINIOS_LINUX_SYSCALL_GETRANDOM:
            return userproc_linux_getrandom(arg1, arg2, arg3);
        case MINIOS_LINUX_SYSCALL_STATX:
            return userproc_linux_statx(arg1, arg2, arg3, arg4, arg5);
        case MINIOS_LINUX_SYSCALL_EXECVE: {
            mvos_userimg_report_t report;
            mvos_user_stack_layout_t layout;
            int64_t exec_rc = userproc_linux_execve(arg1, arg2, arg3, &report, &layout);
            if (exec_rc != 0) {
                console_write_string("userapp execve scaffold failed errno=");
                console_write_u64((uint64_t)exec_rc);
                console_write_string("\n");
                return userproc_errno(exec_rc);
            }

            console_write_string("userapp execve ok entry=");
            console_write_u64(report.mapped_entry);
            console_write_string(" rsp=");
            console_write_u64(layout.initial_rsp);
            console_write_string(" argc=");
            console_write_u64(layout.argc);
            console_write_string("\n");

            uint64_t enter_rc = userproc_enter_execve_and_wait(report.mapped_entry,
                                                               layout.initial_rsp,
                                                               layout.argc,
                                                               layout.argv_user,
                                                               layout.envp_user);
            console_write_string("userapp execve returned\n");
            return enter_rc;
        }
        case MINIOS_LINUX_SYSCALL_EXIT:
        case MINIOS_LINUX_SYSCALL_EXIT_GROUP:
            g_userproc_running = false;
            g_userproc_strict_user_memory = false;
            console_write_string("userapp exit requested code=");
            console_write_u64(arg1);
            console_write_string("\n");
            return 1;
        default:
            console_write_string("userapp syscall not implemented nr=");
            console_write_u64(syscall);
            console_write_string("\n");
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
        sizeof(msg1) - 1,
        0,
        0,
        0);
    userproc_probe_print_ret("write.ret", ret);

    static const char msg2[] = "[linux-abi] writev[0] ";
    static const char msg3[] = "writev[1]\n";
    minios_iovec_t iov[2];
    iov[0].iov_base = (uint64_t)(uintptr_t)msg2;
    iov[0].iov_len = sizeof(msg2) - 1;
    iov[1].iov_base = (uint64_t)(uintptr_t)msg3;
    iov[1].iov_len = sizeof(msg3) - 1;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_WRITEV, 1, (uint64_t)(uintptr_t)iov, 2, 0, 0, 0);
    userproc_probe_print_ret("writev.ret", ret);

    userproc_probe_print_ret("getpid.ret", userproc_dispatch(MINIOS_LINUX_SYSCALL_GETPID, 0, 0, 0, 0, 0, 0));
    userproc_probe_print_ret("gettid.ret", userproc_dispatch(MINIOS_LINUX_SYSCALL_GETTID, 0, 0, 0, 0, 0, 0));

    uint64_t brk0 = userproc_dispatch(MINIOS_LINUX_SYSCALL_BRK, 0, 0, 0, 0, 0, 0);
    userproc_probe_print_ret("brk.current", brk0);
    uint64_t brk1 = userproc_dispatch(MINIOS_LINUX_SYSCALL_BRK, brk0 + 0x1000, 0, 0, 0, 0, 0);
    userproc_probe_print_ret("brk.bumped", brk1);

    minios_utsname_t uts;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_UNAME, (uint64_t)(uintptr_t)&uts, 0, 0, 0, 0, 0);
    userproc_probe_print_ret("uname.ret", ret);
    if (!userproc_is_errno(ret)) {
        console_write_string("[linux-abi] uname.sysname=");
        console_write_string(uts.sysname);
        console_write_string(" release=");
        console_write_string(uts.release);
        console_write_string("\n");
    }

    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_ARCH_PRCTL, MINIOS_ARCH_SET_FS, 0x700000001000ULL, 0, 0, 0, 0);
    userproc_probe_print_ret("arch_prctl.set_fs", ret);
    uint64_t fs_base = 0;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_ARCH_PRCTL,
                            MINIOS_ARCH_GET_FS,
                            (uint64_t)(uintptr_t)&fs_base,
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("arch_prctl.get_fs", ret);
    userproc_probe_print_ret("arch_prctl.fs_base", fs_base);

    uint64_t tid_storage = 0;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_SET_TID_ADDRESS,
                            (uint64_t)(uintptr_t)&tid_storage,
                            0,
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("set_tid_address.ret", ret);
    userproc_probe_print_ret("set_tid_address.ptr", g_userproc_tid_addr);

    static const char readme_path[] = "/boot/init/readme.txt";
    char read_buf[80];
    minios_linux_stat_t st;
    uint64_t fd = userproc_dispatch(MINIOS_LINUX_SYSCALL_OPENAT,
                                    (uint64_t)(int64_t)MINIOS_AT_FDCWD,
                                    (uint64_t)(uintptr_t)readme_path,
                                    0,
                                    0,
                                    0,
                                    0);
    userproc_probe_print_ret("openat.readme_fd", fd);
    if (!userproc_is_errno(fd)) {
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_FSTAT,
                                fd,
                                (uint64_t)(uintptr_t)&st,
                                0,
                                0,
                                0,
                                0);
        userproc_probe_print_ret("fstat.readme_ret", ret);
        if (!userproc_is_errno(ret)) {
            userproc_probe_print_ret("fstat.readme_size", (uint64_t)st.st_size);
        }
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_READ,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                sizeof(read_buf) - 1,
                                0,
                                0,
                                0);
        userproc_probe_print_ret("read.readme_ret", ret);
        if (!userproc_is_errno(ret)) {
            read_buf[ret] = '\0';
            console_write_string("[linux-abi] read.readme_text=");
            console_write_string(read_buf);
            if (ret == 0 || read_buf[ret - 1] != '\n') {
                console_write_string("\n");
            }
        }
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_LSEEK, fd, 0, MINIOS_SEEK_SET, 0, 0, 0);
        userproc_probe_print_ret("lseek.readme_ret", ret);
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_PREAD64,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                6,
                                0,
                                0,
                                0);
        userproc_probe_print_ret("pread64.readme_ret", ret);
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_FCNTL, fd, MINIOS_F_GETFL, 0, 0, 0, 0);
        userproc_probe_print_ret("fcntl.getfl_ret", ret);
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_IOCTL, fd, 0, 0, 0, 0, 0);
        userproc_probe_print_ret("ioctl.readme_ret", ret);
        uint64_t file_map = userproc_dispatch(MINIOS_LINUX_SYSCALL_MMAP,
                                              0,
                                              4096,
                                              MINIOS_PROT_READ,
                                              MINIOS_MAP_PRIVATE,
                                              fd,
                                              0);
        userproc_probe_print_ret("mmap.file_addr", file_map);
        if (!userproc_is_errno(file_map)) {
            ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_MUNMAP, file_map, 4096, 0, 0, 0, 0);
            userproc_probe_print_ret("munmap.file_ret", ret);
        }
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_CLOSE, fd, 0, 0, 0, 0, 0);
        userproc_probe_print_ret("close.readme_ret", ret);
    }

    uint64_t map_addr = userproc_dispatch(MINIOS_LINUX_SYSCALL_MMAP,
                                          0,
                                          8192,
                                          MINIOS_PROT_READ | MINIOS_PROT_WRITE,
                                          MINIOS_MAP_PRIVATE | MINIOS_MAP_ANONYMOUS,
                                          UINT64_MAX,
                                          0);
    userproc_probe_print_ret("mmap.anon_addr", map_addr);
    if (!userproc_is_errno(map_addr)) {
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_MPROTECT,
                                map_addr,
                                8192,
                                MINIOS_PROT_READ,
                                0,
                                0,
                                0);
        userproc_probe_print_ret("mprotect.anon_ret", ret);
        ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_MUNMAP, map_addr, 8192, 0, 0, 0, 0);
        userproc_probe_print_ret("munmap.anon_ret", ret);
    }

    char cwd_buf[8];
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_GETCWD,
                            (uint64_t)(uintptr_t)cwd_buf,
                            sizeof(cwd_buf),
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("getcwd.ret", ret);
    if (!userproc_is_errno(ret)) {
        console_write_string("[linux-abi] getcwd.path=");
        console_write_string(cwd_buf);
        console_write_string("\n");
    }
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_ACCESS,
                            (uint64_t)(uintptr_t)readme_path,
                            0,
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("access.readme_ret", ret);
    minios_timespec_t ts;
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_CLOCK_GETTIME,
                            MINIOS_CLOCK_MONOTONIC,
                            (uint64_t)(uintptr_t)&ts,
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("clock_gettime.ret", ret);
    uint8_t random_buf[16];
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_GETRANDOM,
                            (uint64_t)(uintptr_t)random_buf,
                            sizeof(random_buf),
                            0,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("getrandom.ret", ret);

    static const char exec_path[] = "/bin/hello_linux_tiny";
    static const char *const exec_argv[] = {"hello_linux_tiny", "--probe", NULL};
    static const char *const exec_envp[] = {"TERM=minios", NULL};
    ret = userproc_dispatch(MINIOS_LINUX_SYSCALL_EXECVE,
                            (uint64_t)(uintptr_t)exec_path,
                            (uint64_t)(uintptr_t)exec_argv,
                            (uint64_t)(uintptr_t)exec_envp,
                            0,
                            0,
                            0);
    userproc_probe_print_ret("execve.ret", ret);

    g_userproc_running = prev_running;
    g_userproc_current_app_id = prev_app_id;
}

/* Entry point implemented in assembly:
 * pushes a user-mode IRET frame and jumps via iretq.
 */
extern void userproc_enter_asm(uint64_t entry, uint64_t user_stack);
extern void userproc_enter_execve_asm(uint64_t entry,
                                      uint64_t user_stack,
                                      uint64_t argc,
                                      uint64_t argv_user,
                                      uint64_t envp_user);
extern uint64_t userproc_execve_trampoline_asm(uint64_t entry,
                                               uint64_t user_stack,
                                               uint64_t argc,
                                               uint64_t argv_user,
                                               uint64_t envp_user);

void userproc_enter(uint64_t entry, uint64_t user_stack_top, uint64_t return_rip, uint64_t return_stack) {
    g_userproc_running = true;
    g_userproc_strict_user_memory = true;
    userproc_set_return_context(return_rip, return_stack);
    g_userproc_current_app_id = 0;
    userproc_enter_asm(entry, user_stack_top);
    g_userproc_strict_user_memory = false;
}

void userproc_enter_execve(uint64_t entry,
                           uint64_t user_stack_top,
                           uint64_t argc,
                           uint64_t argv_user,
                           uint64_t envp_user,
                           uint64_t return_rip,
                           uint64_t return_stack) {
    g_userproc_running = true;
    g_userproc_strict_user_memory = true;
    userproc_set_return_context(return_rip, return_stack);
    userproc_enter_execve_asm(entry, user_stack_top, argc, argv_user, envp_user);
    g_userproc_strict_user_memory = false;
}

uint64_t userproc_enter_execve_and_wait(uint64_t entry,
                                        uint64_t user_stack_top,
                                        uint64_t argc,
                                        uint64_t argv_user,
                                        uint64_t envp_user) {
    uint64_t previous_rsp0 = gdt_get_rsp0();
    uint64_t execve_rsp0 = (uint64_t)(uintptr_t)(g_userproc_execve_kernel_stack +
                                                MINIOS_EXECVE_KERNEL_STACK_SIZE);
    g_userproc_running = true;
    g_userproc_strict_user_memory = true;
    g_userproc_syscall_stack_top = execve_rsp0;
    gdt_set_rsp0(execve_rsp0);
    uint64_t rc = userproc_execve_trampoline_asm(entry, user_stack_top, argc, argv_user, envp_user);
    g_userproc_strict_user_memory = false;
    gdt_set_rsp0(previous_rsp0);
    return rc;
}
