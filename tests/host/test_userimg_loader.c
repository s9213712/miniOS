#include <mvos/userimg.h>
#include <mvos/userproc.h>
#include <mvos/vfs.h>
#include <mvos/vmm.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t g_console_last_u64;
static uint32_t g_console_u64_count;
static uint64_t g_enter_execve_entry;
static uint64_t g_enter_execve_stack;
static uint64_t g_enter_execve_argc;
static uint64_t g_enter_execve_argv;
static uint64_t g_enter_execve_envp;
static uint32_t g_enter_execve_count;

/* Host test stubs for elf.c diagnostic symbols. */
void console_write_string(const char *str) {
    (void)str;
}

void console_write_u64(uint64_t value) {
    g_console_last_u64 = value;
    g_console_u64_count++;
}

void console_write_char(char ch) {
    (void)ch;
}

void klog(const char *msg) {
    (void)msg;
}

void klogln(const char *msg) {
    (void)msg;
}

void klog_u64(uint64_t value) {
    (void)value;
}

uint64_t timer_ticks(void) {
    return 0;
}

void syscall_entry(void) {
}

uint64_t gdt_get_rsp0(void) {
    return 0;
}

void gdt_set_rsp0(uint64_t rsp0) {
    (void)rsp0;
}

void userproc_enter_asm(uint64_t entry, uint64_t user_stack) {
    (void)entry;
    (void)user_stack;
}

void userproc_enter_execve_asm(uint64_t entry,
                               uint64_t user_stack,
                               uint64_t argc,
                               uint64_t argv_user,
                               uint64_t envp_user) {
    g_enter_execve_entry = entry;
    g_enter_execve_stack = user_stack;
    g_enter_execve_argc = argc;
    g_enter_execve_argv = argv_user;
    g_enter_execve_envp = envp_user;
    g_enter_execve_count++;
}

uint64_t userproc_execve_trampoline_asm(uint64_t entry,
                                        uint64_t user_stack,
                                        uint64_t argc,
                                        uint64_t argv_user,
                                        uint64_t envp_user) {
    g_enter_execve_entry = entry;
    g_enter_execve_stack = user_stack;
    g_enter_execve_argc = argc;
    g_enter_execve_argv = argv_user;
    g_enter_execve_envp = envp_user;
    g_enter_execve_count++;
    return 1;
}

extern uint64_t g_userproc_running;
extern uint64_t g_userproc_current_app_id;
extern uint64_t g_userproc_return_rip;
extern uint64_t g_userproc_return_stack;

enum {
    TEST_LINUX_SYSCALL_READ = 0,
    TEST_LINUX_SYSCALL_CLOSE = 3,
    TEST_LINUX_SYSCALL_FSTAT = 5,
    TEST_LINUX_SYSCALL_LSEEK = 8,
    TEST_LINUX_SYSCALL_MMAP = 9,
    TEST_LINUX_SYSCALL_MPROTECT = 10,
    TEST_LINUX_SYSCALL_MUNMAP = 11,
    TEST_LINUX_SYSCALL_IOCTL = 16,
    TEST_LINUX_SYSCALL_PREAD64 = 17,
    TEST_LINUX_SYSCALL_ACCESS = 21,
    TEST_LINUX_SYSCALL_GETCWD = 79,
    TEST_LINUX_SYSCALL_FCNTL = 72,
    TEST_LINUX_SYSCALL_EXECVE = 59,
    TEST_LINUX_SYSCALL_CLOCK_GETTIME = 228,
    TEST_LINUX_SYSCALL_OPENAT = 257,
    TEST_LINUX_SYSCALL_NEWFSTATAT = 262,
    TEST_LINUX_SYSCALL_FACCESSAT = 269,
    TEST_LINUX_SYSCALL_GETRANDOM = 318,
    TEST_LINUX_SYSCALL_UNIMPLEMENTED = 999,
    TEST_AT_FDCWD = -100,
    TEST_F_GETFD = 1,
    TEST_F_SETFD = 2,
    TEST_F_GETFL = 3,
    TEST_F_SETFL = 4,
    TEST_SEEK_SET = 0,
    TEST_SEEK_CUR = 1,
    TEST_PROT_READ = 0x1,
    TEST_PROT_WRITE = 0x2,
    TEST_MAP_PRIVATE = 0x02,
    TEST_MAP_ANONYMOUS = 0x20,
};

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
} test_linux_stat_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} test_timespec_t;

static uint64_t read_u64(const uint8_t *stack_mem, uint64_t stack_base, uint64_t stack_top, uint64_t addr) {
    if (addr < stack_base || addr + sizeof(uint64_t) > stack_top) {
        return UINT64_MAX;
    }
    uint64_t value = 0;
    memcpy(&value, stack_mem + (addr - stack_base), sizeof(value));
    return value;
}

int main(void) {
    vmm_init(0);
    vmm_user_heap_init(0x400000ULL, 0x2000ULL, 0x400000ULL);

    uint32_t base_regions = vmm_region_count();

    mvos_userimg_report_t report;
    if (userimg_prepare_embedded_sample(&report) != MVOS_USERIMG_OK) {
        fprintf(stderr, "[test_userimg_loader] prepare_embedded_sample failed\n");
        return 1;
    }
    mvos_vfs_file_t elf_file;
    if (vfs_open("/boot/init/hello_linux_tiny", &elf_file) != 0) {
        fprintf(stderr, "[test_userimg_loader] failed to open VFS ELF sample\n");
        return 1;
    }
    mvos_userimg_report_t vfs_report;
    if (userimg_prepare_image((const uint8_t *)elf_file.data, elf_file.size, &vfs_report) != MVOS_USERIMG_OK) {
        fprintf(stderr, "[test_userimg_loader] prepare_image from VFS failed\n");
        vfs_close(&elf_file);
        return 1;
    }
    vfs_close(&elf_file);
    if (vfs_report.mapped_entry == 0 || vfs_report.image_size != report.image_size) {
        fprintf(stderr, "[test_userimg_loader] VFS image report mismatch\n");
        return 1;
    }
    if (report.entry == 0 || report.mapped_entry == 0) {
        fprintf(stderr, "[test_userimg_loader] invalid entry values\n");
        return 1;
    }
    if (report.load_segments < 1 || report.mapped_regions < 1) {
        fprintf(stderr, "[test_userimg_loader] expected non-zero load/mapped region counts\n");
        return 1;
    }
    if (report.mapped_base >= report.mapped_limit) {
        fprintf(stderr, "[test_userimg_loader] invalid mapped range\n");
        return 1;
    }
    if (vmm_region_count() <= base_regions) {
        fprintf(stderr, "[test_userimg_loader] expected additional VMM regions after load prepare\n");
        return 1;
    }
    if (report.stack_base >= report.stack_top || report.stack_size == 0 || report.stack_top - report.stack_base != report.stack_size) {
        fprintf(stderr, "[test_userimg_loader] invalid stack report\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_entry, report.stack_top) != 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run success\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_entry, report.stack_top - 1ULL) == 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run to reject unaligned stack top\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_limit + 0x1000ULL, report.stack_top) == 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run to reject invalid entry\n");
        return 1;
    }

    const char *argv_demo[] = {"hello_linux_tiny", "--demo"};
    const char *envp_demo[] = {"TERM=minios", "PATH=/usr/bin"};
    uint8_t *stack_mem = calloc(1u, (size_t)report.stack_size);
    if (stack_mem == NULL) {
        fprintf(stderr, "[test_userimg_loader] failed to allocate stack scratch\n");
        return 1;
    }
    mvos_user_stack_layout_t layout;
    int stack_rc = userproc_prepare_exec_stack(
        stack_mem,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        argv_demo,
        2,
        envp_demo,
        2,
        &layout);
    if (stack_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] exec stack prep failed: %s (%d)\n",
                userproc_stack_result_name(stack_rc),
                stack_rc);
        free(stack_mem);
        return 1;
    }
    if (layout.argc != 2 || layout.initial_rsp < report.stack_base || layout.initial_rsp >= report.stack_top) {
        fprintf(stderr, "[test_userimg_loader] invalid stack layout header\n");
        free(stack_mem);
        return 1;
    }
    if (read_u64(stack_mem, report.stack_base, report.stack_top, layout.initial_rsp) != 2ULL) {
        fprintf(stderr, "[test_userimg_loader] expected argc=2 at initial_rsp\n");
        free(stack_mem);
        return 1;
    }

    uint64_t arg0_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user);
    uint64_t arg1_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user + 8ULL);
    uint64_t argn_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user + 16ULL);
    uint64_t env0_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user);
    uint64_t env1_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user + 8ULL);
    uint64_t envn_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user + 16ULL);
    if (arg0_ptr == UINT64_MAX || arg1_ptr == UINT64_MAX || env0_ptr == UINT64_MAX || env1_ptr == UINT64_MAX) {
        fprintf(stderr, "[test_userimg_loader] argv/envp pointer read out of range\n");
        free(stack_mem);
        return 1;
    }
    if (argn_ptr != 0 || envn_ptr != 0) {
        fprintf(stderr, "[test_userimg_loader] argv/envp terminator mismatch\n");
        free(stack_mem);
        return 1;
    }
    if (strcmp((const char *)(stack_mem + (arg0_ptr - report.stack_base)), argv_demo[0]) != 0 ||
        strcmp((const char *)(stack_mem + (arg1_ptr - report.stack_base)), argv_demo[1]) != 0 ||
        strcmp((const char *)(stack_mem + (env0_ptr - report.stack_base)), envp_demo[0]) != 0 ||
        strcmp((const char *)(stack_mem + (env1_ptr - report.stack_base)), envp_demo[1]) != 0) {
        fprintf(stderr, "[test_userimg_loader] argv/envp string payload mismatch\n");
        free(stack_mem);
        return 1;
    }
    if (read_u64(stack_mem, report.stack_base, report.stack_top, layout.auxv_user) != 0ULL ||
        read_u64(stack_mem, report.stack_base, report.stack_top, layout.auxv_user + 8ULL) != 0ULL) {
        fprintf(stderr, "[test_userimg_loader] expected auxv terminator pair\n");
        free(stack_mem);
        return 1;
    }
    if (layout.used_bytes == 0 || layout.strings_bytes == 0 || layout.used_bytes < layout.strings_bytes) {
        fprintf(stderr, "[test_userimg_loader] invalid used/strings byte counters\n");
        free(stack_mem);
        return 1;
    }

    g_userproc_running = 0;
    g_userproc_return_rip = 0;
    g_userproc_return_stack = 0;
    g_enter_execve_count = 0;
    userproc_enter_execve(report.mapped_entry,
                          layout.initial_rsp,
                          layout.argc,
                          layout.argv_user,
                          layout.envp_user,
                          0xCAFEBABEULL,
                          0xFEEDFACEULL);
    if (g_userproc_running != 1 ||
        g_userproc_return_rip != 0xCAFEBABEULL ||
        g_userproc_return_stack != 0xFEEDFACEULL ||
        g_enter_execve_count != 1 ||
        g_enter_execve_entry != report.mapped_entry ||
        g_enter_execve_stack != layout.initial_rsp ||
        g_enter_execve_argc != layout.argc ||
        g_enter_execve_argv != layout.argv_user ||
        g_enter_execve_envp != layout.envp_user) {
        fprintf(stderr, "[test_userimg_loader] execve userspace entry handoff mismatch\n");
        free(stack_mem);
        return 1;
    }

    uint8_t tiny_stack[64];
    stack_rc = userproc_prepare_exec_stack(
        tiny_stack,
        sizeof(tiny_stack),
        0x1000ULL,
        0x1040ULL,
        argv_demo,
        2,
        envp_demo,
        2,
        &layout);
    if (stack_rc != -5) {
        fprintf(stderr, "[test_userimg_loader] expected tiny stack overflow (-5), got %d\n", stack_rc);
        free(stack_mem);
        return 1;
    }

    stack_rc = userproc_prepare_exec_stack(
        stack_mem,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        NULL,
        1,
        envp_demo,
        2,
        &layout);
    if (stack_rc != -4) {
        fprintf(stderr, "[test_userimg_loader] expected null argv error (-4), got %d\n", stack_rc);
        free(stack_mem);
        return 1;
    }

    const char exec_path[] = "/bin/hello_linux_tiny";
    const char *exec_argv[] = {"hello_linux_tiny", "--host-test", NULL};
    const char *exec_envp[] = {"TERM=minios", NULL};
    g_userproc_running = 1;
    g_userproc_current_app_id = 41;
    uint32_t enter_count_before_execve = g_enter_execve_count;
    uint64_t exec_rc = userproc_dispatch(
        TEST_LINUX_SYSCALL_EXECVE,
        (uint64_t)(uintptr_t)exec_path,
        (uint64_t)(uintptr_t)exec_argv,
        (uint64_t)(uintptr_t)exec_envp,
        0,
        0,
        0);
    if (exec_rc != 1ULL) {
        fprintf(stderr, "[test_userimg_loader] expected execve scaffold success signal (1), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (g_enter_execve_count != enter_count_before_execve + 1) {
        fprintf(stderr, "[test_userimg_loader] expected execve to enter userspace handoff\n");
        free(stack_mem);
        return 1;
    }

    g_userproc_running = 1;
    exec_rc = userproc_dispatch(
        TEST_LINUX_SYSCALL_EXECVE,
        (uint64_t)(uintptr_t)"/bad/path",
        (uint64_t)(uintptr_t)exec_argv,
        (uint64_t)(uintptr_t)exec_envp,
        0,
        0,
        0);
    if ((int64_t)exec_rc != -2) {
        fprintf(stderr, "[test_userimg_loader] expected execve ENOENT (-2), got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (g_userproc_running != 1) {
        fprintf(stderr, "[test_userimg_loader] expected failed execve to keep running state\n");
        free(stack_mem);
        return 1;
    }

    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_EXECVE, 0, 0, 0, 0, 0, 0);
    if ((int64_t)exec_rc != -14) {
        fprintf(stderr, "[test_userimg_loader] expected execve EFAULT (-14), got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }

    const char readme_path[] = "/boot/init/readme.txt";
    uint64_t fd = userproc_dispatch(TEST_LINUX_SYSCALL_OPENAT,
                                    (uint64_t)(int64_t)TEST_AT_FDCWD,
                                    (uint64_t)(uintptr_t)readme_path,
                                    0,
                                    0,
                                    0,
                                    0);
    if (fd < 3 || fd > 10) {
        fprintf(stderr, "[test_userimg_loader] expected VFS fd from openat, got %lld\n", (long long)fd);
        free(stack_mem);
        return 1;
    }
    test_linux_stat_t st;
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FSTAT, fd, (uint64_t)(uintptr_t)&st, 0, 0, 0, 0);
    if (exec_rc != 0 || st.st_size <= 0 || (st.st_mode & 0100000) == 0) {
        fprintf(stderr, "[test_userimg_loader] expected regular file fstat, rc=%lld size=%lld mode=%o\n",
                (long long)exec_rc,
                (long long)st.st_size,
                st.st_mode);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, fd, TEST_F_GETFD, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected fcntl F_GETFD success, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, fd, TEST_F_GETFL, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected fcntl F_GETFL readonly flags, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, fd, TEST_F_SETFD, 1, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected fcntl F_SETFD success, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, fd, TEST_F_SETFL, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected fcntl F_SETFL success, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_IOCTL, fd, 0, 0, 0, 0, 0);
    if ((int64_t)exec_rc != -25) {
        fprintf(stderr, "[test_userimg_loader] expected ioctl ENOTTY (-25), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    char read_buf[64];
    memset(read_buf, 0, sizeof(read_buf));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_READ,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                sizeof(read_buf) - 1,
                                0,
                                0,
                                0);
    if (exec_rc == 0 || strstr(read_buf, "miniOS init filesystem") == NULL) {
        fprintf(stderr, "[test_userimg_loader] expected VFS read content, rc=%lld text=%s\n",
                (long long)exec_rc,
                read_buf);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_LSEEK, fd, 0, TEST_SEEK_SET, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected lseek rewind success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    memset(read_buf, 0, sizeof(read_buf));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_READ,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                6,
                                0,
                                0,
                                0);
    if (exec_rc != 6 || strcmp(read_buf, "miniOS") != 0) {
        fprintf(stderr, "[test_userimg_loader] expected read after lseek, rc=%lld text=%s\n",
                (long long)exec_rc,
                read_buf);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_LSEEK, fd, (uint64_t)(int64_t)-1, TEST_SEEK_CUR, 0, 0, 0);
    if (exec_rc != 5) {
        fprintf(stderr, "[test_userimg_loader] expected relative lseek to offset 5, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    memset(read_buf, 0, sizeof(read_buf));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_PREAD64,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                6,
                                0,
                                0,
                                0);
    if (exec_rc != 6 || strcmp(read_buf, "miniOS") != 0) {
        fprintf(stderr, "[test_userimg_loader] expected pread64 at offset 0, rc=%lld text=%s\n",
                (long long)exec_rc,
                read_buf);
        free(stack_mem);
        return 1;
    }
    memset(read_buf, 0, sizeof(read_buf));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_READ,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                1,
                                0,
                                0,
                                0);
    if (exec_rc != 1 || strcmp(read_buf, "S") != 0) {
        fprintf(stderr, "[test_userimg_loader] expected pread64 to preserve fd offset, rc=%lld text=%s\n",
                (long long)exec_rc,
                read_buf);
        free(stack_mem);
        return 1;
    }
    uint64_t file_map_addr = userproc_dispatch(TEST_LINUX_SYSCALL_MMAP,
                                               0,
                                               4096,
                                               TEST_PROT_READ,
                                               TEST_MAP_PRIVATE,
                                               fd,
                                               0);
    if ((int64_t)file_map_addr < 0 || (file_map_addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        fprintf(stderr, "[test_userimg_loader] expected file-backed mmap addr, got %lld\n",
                (long long)file_map_addr);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_MUNMAP, file_map_addr, 4096, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected file-backed munmap success, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_CLOSE, fd, 0, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected close success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_READ,
                                fd,
                                (uint64_t)(uintptr_t)read_buf,
                                sizeof(read_buf) - 1,
                                0,
                                0,
                                0);
    if ((int64_t)exec_rc != -9) {
        fprintf(stderr, "[test_userimg_loader] expected read after close EBADF (-9), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, fd, TEST_F_GETFD, 0, 0, 0, 0);
    if ((int64_t)exec_rc != -9) {
        fprintf(stderr, "[test_userimg_loader] expected fcntl after close EBADF (-9), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_IOCTL, fd, 0, 0, 0, 0, 0);
    if ((int64_t)exec_rc != -9) {
        fprintf(stderr, "[test_userimg_loader] expected ioctl after close EBADF (-9), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FCNTL, 1, TEST_F_GETFL, 0, 0, 0, 0);
    if (exec_rc != 1) {
        fprintf(stderr, "[test_userimg_loader] expected stdout fcntl F_GETFL O_WRONLY, got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_IOCTL, 1, 0, 0, 0, 0, 0);
    if ((int64_t)exec_rc != -25) {
        fprintf(stderr, "[test_userimg_loader] expected stdout ioctl ENOTTY (-25), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_NEWFSTATAT,
                                (uint64_t)(int64_t)TEST_AT_FDCWD,
                                (uint64_t)(uintptr_t)readme_path,
                                (uint64_t)(uintptr_t)&st,
                                0,
                                0,
                                0);
    if (exec_rc != 0 || st.st_size <= 0) {
        fprintf(stderr, "[test_userimg_loader] expected newfstatat success, rc=%lld size=%lld\n",
                (long long)exec_rc,
                (long long)st.st_size);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_OPENAT,
                                (uint64_t)(int64_t)TEST_AT_FDCWD,
                                (uint64_t)(uintptr_t)"/missing",
                                0,
                                0,
                                0,
                                0);
    if ((int64_t)exec_rc != -2) {
        fprintf(stderr, "[test_userimg_loader] expected openat ENOENT (-2), got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    char cwd[8];
    memset(cwd, 0, sizeof(cwd));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_GETCWD, (uint64_t)(uintptr_t)cwd, sizeof(cwd), 0, 0, 0, 0);
    if (exec_rc != (uint64_t)(uintptr_t)cwd || strcmp(cwd, "/") != 0) {
        fprintf(stderr, "[test_userimg_loader] expected getcwd '/', rc=%lld cwd=%s\n",
                (long long)exec_rc,
                cwd);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_ACCESS, (uint64_t)(uintptr_t)readme_path, 0, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected access success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_FACCESSAT,
                                (uint64_t)(int64_t)TEST_AT_FDCWD,
                                (uint64_t)(uintptr_t)readme_path,
                                0,
                                0,
                                0,
                                0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected faccessat success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    test_timespec_t ts;
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_CLOCK_GETTIME, 1, (uint64_t)(uintptr_t)&ts, 0, 0, 0, 0);
    if (exec_rc != 0 || ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000LL) {
        fprintf(stderr, "[test_userimg_loader] expected clock_gettime success, rc=%lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    uint8_t random_buf[16];
    memset(random_buf, 0, sizeof(random_buf));
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_GETRANDOM,
                                (uint64_t)(uintptr_t)random_buf,
                                sizeof(random_buf),
                                0,
                                0,
                                0,
                                0);
    if (exec_rc != sizeof(random_buf)) {
        fprintf(stderr, "[test_userimg_loader] expected getrandom byte count, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    uint64_t mmap_addr = userproc_dispatch(TEST_LINUX_SYSCALL_MMAP,
                                           0,
                                           8192,
                                           TEST_PROT_READ | TEST_PROT_WRITE,
                                           TEST_MAP_PRIVATE | TEST_MAP_ANONYMOUS,
                                           UINT64_MAX,
                                           0);
    if ((int64_t)mmap_addr < 0 || (mmap_addr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        fprintf(stderr, "[test_userimg_loader] expected page-aligned mmap addr, got %lld\n", (long long)mmap_addr);
        free(stack_mem);
        return 1;
    }
    int found_mmap = 0;
    for (uint32_t i = 0; i < vmm_region_count(); ++i) {
        mvos_vmm_region_info_t info;
        if (vmm_region_at(i, &info) == 0 && info.base == mmap_addr && strcmp(info.tag, "user-mmap") == 0) {
            found_mmap = 1;
        }
    }
    if (!found_mmap) {
        fprintf(stderr, "[test_userimg_loader] expected user-mmap VMM region\n");
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_MPROTECT, mmap_addr, 8192, TEST_PROT_READ, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected mprotect success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (vmm_user_range_check(mmap_addr, 4096, MVOS_VMM_FLAG_WRITE) == 0) {
        fprintf(stderr, "[test_userimg_loader] mmap write check unexpectedly passed after mprotect\n");
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_MUNMAP, mmap_addr, 8192, 0, 0, 0, 0);
    if (exec_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] expected munmap success, got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_MMAP,
                                0,
                                4096,
                                TEST_PROT_READ,
                                TEST_MAP_PRIVATE,
                                UINT64_MAX,
                                0);
    if ((int64_t)exec_rc != -9) {
        fprintf(stderr, "[test_userimg_loader] expected bad-fd file mmap EBADF (-9), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }

    g_console_last_u64 = 0;
    g_console_u64_count = 0;
    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_UNIMPLEMENTED, 0, 0, 0, 4, 5, 6);
    if ((int64_t)exec_rc != -38) {
        fprintf(stderr, "[test_userimg_loader] expected unimplemented syscall ENOSYS (-38), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (g_console_u64_count == 0 || g_console_last_u64 != TEST_LINUX_SYSCALL_UNIMPLEMENTED) {
        fprintf(stderr, "[test_userimg_loader] expected unimplemented syscall number in diagnostics\n");
        free(stack_mem);
        return 1;
    }
    free(stack_mem);

    uint32_t regions_after_first = vmm_region_count();
    int found_userimg = 0;
    int found_stack = 0;
    for (uint32_t i = 0; i < regions_after_first; ++i) {
        mvos_vmm_region_info_t info;
        if (vmm_region_at(i, &info) != 0) {
            continue;
        }
        if (strcmp(info.tag, "userimg-load") == 0) {
            found_userimg = 1;
            if ((info.flags & MVOS_VMM_FLAG_USER) == 0) {
                fprintf(stderr, "[test_userimg_loader] expected user flag on userimg region\n");
                return 1;
            }
        }
        if (strcmp(info.tag, "userimg-stack") == 0) {
            found_stack = 1;
            if ((info.flags & MVOS_VMM_FLAG_USER) == 0 || (info.flags & MVOS_VMM_FLAG_WRITE) == 0) {
                fprintf(stderr, "[test_userimg_loader] expected user writable stack region\n");
                return 1;
            }
            if (info.base != report.stack_base || info.size != report.stack_size) {
                fprintf(stderr, "[test_userimg_loader] stack region mismatch with report\n");
                return 1;
            }
        }
    }
    if (!found_userimg) {
        fprintf(stderr, "[test_userimg_loader] missing userimg-load region tag\n");
        return 1;
    }
    if (!found_stack) {
        fprintf(stderr, "[test_userimg_loader] missing userimg-stack region tag\n");
        return 1;
    }

    mvos_userimg_report_t report2;
    if (userimg_prepare_embedded_sample(&report2) != MVOS_USERIMG_OK) {
        fprintf(stderr, "[test_userimg_loader] second prepare_embedded_sample failed\n");
        return 1;
    }
    if (vmm_region_count() != regions_after_first) {
        fprintf(stderr, "[test_userimg_loader] region count changed after idempotent reload\n");
        return 1;
    }
    if (report.mapped_regions != report2.mapped_regions || report.mapped_entry != report2.mapped_entry) {
        fprintf(stderr, "[test_userimg_loader] report mismatch between repeated loads\n");
        return 1;
    }

    printf("[test_userimg_loader] PASS regions=%u mapped=%llu stack=%llu\n",
           regions_after_first,
           (unsigned long long)report.mapped_regions,
           (unsigned long long)report.stack_size);
    return 0;
}
