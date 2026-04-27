#include <mvos/userapp.h>
#include <mvos/userproc.h>
#include <mvos/console.h>
#include <mvos/log.h>
#include <mvos/heap.h>
#include <mvos/scheduler.h>
#include <mvos/interrupt.h>
#include <mvos/keyboard.h>
#include <mvos/serial.h>
#include <mvos/elf.h>
#include <mvos/userimg.h>
#include <mvos/vmm.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern const uint8_t minios_userapp_hello[];
extern const uint8_t minios_userapp_hello_end[];
extern const uint8_t minios_userapp_ticks[];
extern const uint8_t minios_userapp_ticks_end[];
extern void minios_userapp_scheduler(void);
extern void minios_userapp_linux_abi(void);
extern uint64_t minios_userapp_cpp_magic(void);

#define MINIOS_USERIMG_STACK_SCRATCH_SIZE 65536

#ifndef MINIOS_PHASE20_USER_MODE
#define MINIOS_PHASE20_USER_MODE 1
#endif

static uint8_t g_userimg_stack_scratch[MINIOS_USERIMG_STACK_SCRATCH_SIZE] __attribute__((aligned(16)));

enum {
    MINIOS_USERAPP_IMAGE_BASE = 0x0000400000800000ULL,
    MINIOS_USERAPP_STACK_GAP_BYTES = 0x100000ULL,
    MINIOS_USERAPP_STACK_SIZE = 0x10000ULL,
    MINIOS_USERAPP_KERNEL_STACK_SIZE = 16384ULL,
};

typedef struct {
    const char *name;
    const char *description;
    mvos_userapp_entry_t kernel_entry;
    const uint8_t *user_image;
    const uint8_t *user_image_end;
    const char *exe_path;
    bool user_mode;
} mvos_userapp_t;

static int userapp_checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int userapp_align_up_u64(uint64_t value, uint64_t align, uint64_t *out) {
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

static int userapp_back_user_range(uint64_t base, uint64_t size, uint64_t flags) {
    uint64_t end = 0;
    if (userapp_checked_add_u64(base, size, &end) != 0) {
        return -1;
    }
    for (uint64_t page = base; page < end; page += MVOS_VMM_PAGE_SIZE) {
        void *backing = NULL;
        if (vmm_map_user_backed_page(page, flags, &backing) != 0) {
            return -1;
        }
    }
    return 0;
}

static int userapp_prepare_user_image(const mvos_userapp_t *app,
                                      uint64_t *out_entry,
                                      uint64_t *out_stack_top) {
    if (app == NULL || out_entry == NULL || out_stack_top == NULL ||
        app->user_image == NULL || app->user_image_end == NULL ||
        app->user_image_end <= app->user_image) {
        return -1;
    }

    uint64_t image_size = (uint64_t)((uintptr_t)app->user_image_end - (uintptr_t)app->user_image);
    uint64_t code_size = 0;
    if (userapp_align_up_u64(image_size, MVOS_VMM_PAGE_SIZE, &code_size) != 0) {
        return -1;
    }

    vmm_reset_user_state();

    if (vmm_map_range(MINIOS_USERAPP_IMAGE_BASE,
                      code_size,
                      MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_EXEC | MVOS_VMM_FLAG_USER,
                      "userimg-load") != 0) {
        vmm_reset_user_state();
        return -2;
    }
    if (userapp_back_user_range(MINIOS_USERAPP_IMAGE_BASE,
                                code_size,
                                MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_EXEC | MVOS_VMM_FLAG_USER) != 0) {
        vmm_reset_user_state();
        return -3;
    }
    if (vmm_copy_to_user(MINIOS_USERAPP_IMAGE_BASE, app->user_image, image_size) != 0) {
        vmm_reset_user_state();
        return -4;
    }
    if (vmm_protect_range(MINIOS_USERAPP_IMAGE_BASE,
                          code_size,
                          MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_EXEC | MVOS_VMM_FLAG_USER) != 0) {
        vmm_reset_user_state();
        return -5;
    }

    uint64_t stack_base = 0;
    if (userapp_checked_add_u64(MINIOS_USERAPP_IMAGE_BASE, code_size, &stack_base) != 0 ||
        userapp_checked_add_u64(stack_base, MINIOS_USERAPP_STACK_GAP_BYTES, &stack_base) != 0 ||
        userapp_align_up_u64(stack_base, MVOS_VMM_PAGE_SIZE, &stack_base) != 0) {
        vmm_reset_user_state();
        return -6;
    }

    uint64_t stack_top = 0;
    if (userapp_checked_add_u64(stack_base, MINIOS_USERAPP_STACK_SIZE, &stack_top) != 0) {
        vmm_reset_user_state();
        return -6;
    }

    if (vmm_map_range(stack_base,
                      MINIOS_USERAPP_STACK_SIZE,
                      MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER,
                      "userimg-stack") != 0) {
        vmm_reset_user_state();
        return -7;
    }
    if (userapp_back_user_range(stack_base,
                                MINIOS_USERAPP_STACK_SIZE,
                                MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER) != 0) {
        vmm_reset_user_state();
        return -8;
    }

    *out_entry = MINIOS_USERAPP_IMAGE_BASE;
    *out_stack_top = stack_top;
    return 0;
}

static int userapp_run_user_mode(const mvos_userapp_t *app, uint64_t app_id) {
    if (app == NULL) {
        return -1;
    }

    uint8_t *kernel_stack = (uint8_t *)kmalloc(MINIOS_USERAPP_KERNEL_STACK_SIZE);
    if (kernel_stack == NULL) {
        return -2;
    }

    uint64_t entry = 0;
    uint64_t stack_top = 0;
    int prep_rc = userapp_prepare_user_image(app, &entry, &stack_top);
    if (prep_rc != 0) {
        kfree(kernel_stack);
        return prep_rc;
    }

    if (userproc_handoff_dry_run(entry, stack_top) != 0) {
        vmm_reset_user_state();
        kfree(kernel_stack);
        return -3;
    }

    userproc_set_current_app_id(app_id);
    userproc_set_exe_path(app->exe_path != NULL ? app->exe_path : app->name);
    uint64_t enter_rc = userproc_enter_execve_and_wait(
        entry,
        stack_top,
        0,
        0,
        0,
        (uint64_t)(uintptr_t)(kernel_stack + MINIOS_USERAPP_KERNEL_STACK_SIZE));
    userproc_set_current_app_id(0);

    vmm_reset_user_state();
    kfree(kernel_stack);
    return enter_rc == 1 ? 0 : -4;
}

static void userapp_scheduler(void) {
    console_write_string("[userapp] scheduler app\n");
    console_write_string("[userapp] tasks=");
    console_write_u64((uint64_t)scheduler_task_count());
    console_write_string(" current run log snapshot:\n");
    for (uint32_t i = 0; i < scheduler_task_count(); ++i) {
        console_write_string("  ");
        console_write_string(scheduler_task_name(i));
        console_write_string(" runs=");
        console_write_u64(scheduler_task_runs(i));
        console_write_string("\n");
    }
}

static void userapp_fallback_hello(void) {
    console_write_string("user app fallback: hello from kernel mode\n");
}

static void userapp_fallback_ticks(void) {
    console_write_string("user app fallback: ticks=");
    console_write_u64(timer_ticks());
    console_write_string("\n");
}

static void userapp_fallback_cpp(void) {
    console_write_string("user app fallback: C++ demo from kernel mode\n");
    console_write_string("magic=");
    console_write_u64(minios_userapp_cpp_magic());
    console_write_string(" (0x");
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((minios_userapp_cpp_magic() >> (60u - i * 4u)) & 0xFu);
        const char hex[] = "0123456789abcdef";
        console_write_char(hex[nibble]);
    }
    console_write_string(")\n");
}

static int userapp_1a2b_read_char(void) {
    for (;;) {
        int c = keyboard_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        c = serial_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        mvos_idle_wait();
    }
}

static int userapp_1a2b_read_line(char *line, size_t cap, const char *prompt) {
    if (line == NULL || cap == 0) {
        return -1;
    }
    if (prompt != NULL) {
        console_write_string(prompt);
    }
    size_t len = 0;
    line[0] = '\0';
    while (1) {
        int c = userapp_1a2b_read_char();
        if (c == '\r' || c == '\n') {
            console_write_char('\n');
            line[len] = '\0';
            return 0;
        }
        if (c == '\b' || c == 0x7f) {
            if (len == 0) {
                continue;
            }
            --len;
            line[len] = '\0';
            console_write_char('\b');
            console_write_char(' ');
            console_write_char('\b');
            continue;
        }
        if (c < 32 || c >= 127) {
            continue;
        }
        if (len + 1 >= cap) {
            continue;
        }
        line[len++] = (char)c;
        line[len] = '\0';
        console_write_char((char)c);
    }
}

static int userapp_1a2b_parse_guess(const char *line, int guess[4]) {
    int seen[10] = {0};
    for (uint32_t i = 0; i < 4; ++i) {
        if (line == NULL || line[i] == '\0') {
            return -1;
        }
        char ch = line[i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        int digit = (int)(ch - '0');
        if (seen[digit]) {
            return -1;
        }
        seen[digit] = 1;
        guess[i] = digit;
    }
    return line[4] == '\0' ? 0 : -1;
}

static void userapp_1a2b_write_int(uint64_t value) {
    char text[32];
    int len = 0;
    if (value == 0) {
        console_write_char('0');
        return;
    }
    while (value > 0 && len < (int)(sizeof(text) - 1)) {
        text[len++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (len > 0) {
        --len;
        console_write_char(text[len]);
    }
}

static void userapp_1a2b_print_result(uint64_t a, uint64_t b) {
    userapp_1a2b_write_int(a);
    console_write_string("A");
    userapp_1a2b_write_int(b);
    console_write_string("B\n");
}

static uint64_t g_userapp_1a2b_rng_state = 0ULL;

static uint64_t userapp_1a2b_rng_next(void) {
    if (g_userapp_1a2b_rng_state == 0ULL) {
        g_userapp_1a2b_rng_state = 0x1234abcdULL ^ timer_ticks();
    }
    g_userapp_1a2b_rng_state = (g_userapp_1a2b_rng_state * 6364136223846793005ULL) + 1ULL;
    return g_userapp_1a2b_rng_state;
}

static void userapp_1a2b_seed_rng(uint64_t seed) {
    g_userapp_1a2b_rng_state ^= seed | 1ULL;
    if (g_userapp_1a2b_rng_state == 0ULL) {
        g_userapp_1a2b_rng_state = 0x9e3779b97f4a7c15ULL;
    }
}

static void userapp_1a2b_init_secret(int secret[4]) {
    int used[10] = {0};
    uint32_t generated = 0;
    userapp_1a2b_seed_rng(timer_ticks() * 1103515245ULL + 12345ULL);
    while (generated < 4) {
        int digit = (int)(userapp_1a2b_rng_next() % 10ULL);
        if (used[digit]) {
            continue;
        }
        used[digit] = 1;
        secret[generated++] = digit;
    }
}

static void userapp_1a2b_print_secret(const int secret[4]) {
    for (uint32_t i = 0; i < 4; ++i) {
        console_write_char((char)('0' + secret[i]));
    }
    console_write_char('\n');
}

static void userapp_1a2b_evaluate_guess(const int secret[4], const int guess[4], uint64_t *a_out, uint64_t *b_out) {
    uint64_t a = 0;
    uint64_t b = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        if (secret[i] == guess[i]) {
            ++a;
            continue;
        }
        for (uint32_t j = 0; j < 4; ++j) {
            if (i == j) {
                continue;
            }
            if (guess[i] == secret[j]) {
                ++b;
                break;
            }
        }
    }
    *a_out = a;
    *b_out = b;
}

static void userapp_fallback_1a2b(void) {
    int secret[4];
    char line[16];
    int guess[4];
    int attempts = 0;
    int solved = 0;
    userapp_1a2b_init_secret(secret);

    console_write_string("1A2B Game (C version)\n");
    console_write_string("Secret: 4 distinct digits (0-9), max 8 tries.\n");
    console_write_string("Input 'q' to quit.\n");

    while (attempts < 8 && !solved) {
        if (userapp_1a2b_read_line(line, sizeof(line), "guess> ") != 0) {
            return;
        }
        if (line[0] == 'q' && line[1] == '\0') {
            break;
        }
        if (userapp_1a2b_parse_guess(line, guess) != 0) {
            console_write_string("invalid input: please input exactly 4 distinct digits (0-9).\n");
            continue;
        }
        ++attempts;
        uint64_t a = 0;
        uint64_t b = 0;
        userapp_1a2b_evaluate_guess(secret, guess, &a, &b);
        userapp_1a2b_print_result(a, b);
        if (a == 4) {
            solved = 1;
            console_write_string("You win! secret=");
            userapp_1a2b_print_secret(secret);
        }
    }

    if (!solved && attempts >= 8) {
        console_write_string("Game over. answer=");
        userapp_1a2b_print_secret(secret);
    }
    if (!solved && line[0] == 'q') {
        console_write_string("game quit.\n");
    }
}

static void userapp_fallback_python(void) {
    console_write_string("mini-python subset is available via `run python <path>`.\n");
    console_write_string("Mini syntax supported:\n");
    console_write_string("- print('text')\n");
    console_write_string("- print(name_or_expr)\n");
    console_write_string("- variable = expr\n");
    console_write_string("- +, -, *, / operators\n");
    console_write_string("- parentheses in expressions, e.g. 2*(3+1)\n");
}

static void userapp_fallback_linux_abi(void) {
    console_write_string("linux abi preview: kernel probe path\n");
    console_write_string("supported preview syscalls: read/write/writev/close/fstat/lseek/mmap/mprotect/munmap/brk/access/getcwd/uname/getpid/gettid/set_tid_address/arch_prctl/clock_gettime/execve/exit_group/openat/newfstatat/faccessat/getrandom\n");
    userproc_linux_abi_probe();
}

static void userapp_fallback_elf_inspect(void) {
    console_write_string("elf inspect: embedded linux-user sample\n");
    elf_sample_diagnostic();
}

static void userapp_fallback_elf_load(void) {
    mvos_userimg_report_t report;
    mvos_userimg_result_t rc = userimg_prepare_embedded_sample(&report);
    if (rc != MVOS_USERIMG_OK) {
        console_write_string("elf load failed: ");
        console_write_string(userimg_result_name(rc));
        console_write_string("\n");
        return;
    }

    console_write_string("elf load prepared (layout + dry-run)\n");
    console_write_string("entry=");
    console_write_u64(report.entry);
    console_write_string(" mapped_entry=");
    console_write_u64(report.mapped_entry);
    console_write_string("\n");
    console_write_string("orig_vaddr=[");
    console_write_u64(report.min_vaddr);
    console_write_string(",");
    console_write_u64(report.max_vaddr);
    console_write_string(") mapped=[");
    console_write_u64(report.mapped_base);
    console_write_string(",");
    console_write_u64(report.mapped_limit);
    console_write_string(")\n");
    console_write_string("load_segments=");
    console_write_u64(report.load_segments);
    console_write_string(" mapped_regions=");
    console_write_u64(report.mapped_regions);
    console_write_string(" mapped_bytes=");
    console_write_u64(report.mapped_bytes);
    console_write_string("\n");
    console_write_string("stack=[");
    console_write_u64(report.stack_base);
    console_write_string(",");
    console_write_u64(report.stack_top);
    console_write_string(") size=");
    console_write_u64(report.stack_size);
    console_write_string("\n");

    int handoff_rc = userproc_handoff_dry_run(report.mapped_entry, report.stack_top);
    console_write_string("handoff dry-run: ");
    console_write_string(userproc_handoff_result_name(handoff_rc));
    console_write_string(" (rc=");
    console_write_u64((uint64_t)(int64_t)handoff_rc);
    console_write_string(")\n");

    if (report.stack_size > MINIOS_USERIMG_STACK_SCRATCH_SIZE) {
        console_write_string("exec stack prep: scratch buffer too small\n");
        return;
    }

    const char *argv[] = {"hello_linux_tiny", "--demo"};
    const char *envp[] = {"TERM=minios", "PATH=/usr/bin"};
    mvos_user_stack_layout_t stack_layout;
    int stack_rc = userproc_prepare_exec_stack(
        g_userimg_stack_scratch,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        argv,
        2,
        envp,
        2,
        &stack_layout);
    console_write_string("exec stack prep: ");
    console_write_string(userproc_stack_result_name(stack_rc));
    console_write_string(" (rc=");
    console_write_u64((uint64_t)(int64_t)stack_rc);
    console_write_string(")\n");
    if (stack_rc == 0) {
        console_write_string("rsp=");
        console_write_u64(stack_layout.initial_rsp);
        console_write_string(" argv=");
        console_write_u64(stack_layout.argv_user);
        console_write_string(" envp=");
        console_write_u64(stack_layout.envp_user);
        console_write_string(" used=");
        console_write_u64(stack_layout.used_bytes);
        console_write_string("\n");
        userproc_set_exe_path("/boot/init/hello_linux_tiny");
    }
}

static const mvos_userapp_t g_userapps[] = {
    {"hello",
     "print hello from direct ring-3 userapp",
     userapp_fallback_hello,
     minios_userapp_hello,
     minios_userapp_hello_end,
     "/userapp/hello",
     true},
    {"ticks",
     "print current timer ticks via ring-3 syscall demo",
     userapp_fallback_ticks,
     minios_userapp_ticks,
     minios_userapp_ticks_end,
     "/userapp/ticks",
     true},
    {"linux-abi",
     "preview Linux x86_64 syscall subset with tiny execve ELF demo (kernel probe)",
     userapp_fallback_linux_abi,
     NULL,
     NULL,
     NULL,
     false},
    {"elf-inspect", "inspect embedded linux-user ELF metadata (kernel mode)", userapp_fallback_elf_inspect, NULL, NULL, NULL, false},
    {"elf-load", "prepare embedded linux-user ELF VMM layout (kernel mode)", userapp_fallback_elf_load, NULL, NULL, NULL, false},
    {"cpp", "print C++ demo result (kernel mode demo)", userapp_fallback_cpp, NULL, NULL, NULL, false},
    {"1a2b", "play 1A2B guessing game (C logic, kernel mode)", userapp_fallback_1a2b, NULL, NULL, NULL, false},
    {"1a2b-c", "play 1A2B guessing game (C logic, kernel mode)", userapp_fallback_1a2b, NULL, NULL, NULL, false},
    {"scheduler", "print scheduler snapshot (kernel mode)", userapp_scheduler, NULL, NULL, NULL, false},
    {"python", "run mini-python subset runner on VFS script", userapp_fallback_python, NULL, NULL, NULL, false},
};

static const uint64_t g_userapp_count = sizeof(g_userapps) / sizeof(g_userapps[0]);

void userapp_init(void) {
    /* Reserved for future init needs (e.g. loading manifest from VFS or initramfs). */
}

uint64_t userapp_count(void) {
    return g_userapp_count;
}

const char *userapp_name(uint64_t index) {
    if (index >= g_userapp_count) {
        return NULL;
    }
    return g_userapps[index].name;
}

const char *userapp_desc(uint64_t index) {
    if (index >= g_userapp_count) {
        return NULL;
    }
    return g_userapps[index].description;
}

int userapp_is_user_mode(uint64_t index) {
    if (index >= g_userapp_count) {
        return -1;
    }
    return g_userapps[index].user_mode ? 1 : 0;
}

int userapp_run(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }

    for (uint64_t i = 0; i < g_userapp_count; ++i) {
        const mvos_userapp_t *app = &g_userapps[i];
        if (app->name[0] == name[0]) {
            const char *a = app->name;
            const char *b = name;
            while (*a != '\0' && *b != '\0' && *a == *b) {
                ++a;
                ++b;
            }
            if (*a == '\0' && *b == '\0') {
                if (app->user_mode) {
                    if (!MINIOS_PHASE20_USER_MODE) {
                        if (app->kernel_entry == NULL) {
                            return -3;
                        }
                        klog("[userapp] warning: running ");
                        klog(app->name);
                        klogln(" via kernel-mode fallback; ring-3 isolation is disabled");
                        app->kernel_entry();
                        return 0;
                    }
                    return userapp_run_user_mode(app, i + 1);
                }
                if (app->kernel_entry == NULL) {
                    return -3;
                }
                app->kernel_entry();
                return 0;
            }
        }
    }
    return -2;
}
