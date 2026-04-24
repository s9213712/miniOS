#include <mvos/shell.h>
#include <mvos/console.h>
#include <mvos/keyboard.h>
#include <mvos/pmm.h>
#include <mvos/scheduler.h>
#include <mvos/userapp.h>
#include <mvos/vfs.h>
#include <mvos/interrupt.h>
#include <mvos/panic.h>
#include <stdint.h>
#include <stddef.h>

#define SHELL_BUFFER_LEN 128

static void shell_print_prompt(void) {
    console_write_string("mvos> ");
}

static size_t shell_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static int shell_streq(const char *lhs, const char *rhs) {
    size_t i = 0;
    while (lhs[i] != '\0' && rhs[i] != '\0' && lhs[i] == rhs[i]) {
        ++i;
    }
    return lhs[i] == '\0' && rhs[i] == '\0';
}

static const char *shell_skip_spaces(const char *s) {
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static void shell_print_app_info(const char *app_name) {
    if (shell_streq(app_name, "app")) {
        console_write_string("app: framebuffer demo window (default boot style)\n");
        console_write_string("command: app, app launch app\n");
        return;
    }
    if (shell_streq(app_name, "alt")) {
        console_write_string("app: alternate framebuffer demo layout\n");
        console_write_string("command: app alt, app launch alt\n");
        return;
    }
    console_write_string("unknown app: ");
    console_write_string(app_name);
    console_write_string("\n");
}

static void shell_vfs_list_visitor(const char *path, uint64_t size, uint32_t checksum, void *user_data) {
    (void)user_data;
    console_write_string(path);
    console_write_string(" ");
    console_write_u64(size);
    console_write_string(" bytes checksum=0x");
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (checksum >> (28 - i * 4)) & 0xf;
        const char hex[] = "0123456789abcdef";
        console_write_char(hex[nibble]);
    }
    console_write_string("\n");
}

static void shell_cmd_ls(const char *arg) {
    while (*arg == ' ') {
        ++arg;
    }
    const char *prefix = (*arg == '\0') ? "/boot/init" : arg;
    uint64_t listed = vfs_list(shell_vfs_list_visitor, prefix, NULL);
    console_write_string("[vfs] listed=");
    console_write_u64(listed);
    console_write_string(" entries for prefix \"");
    console_write_string(prefix);
    console_write_string("\"\n");
}

static void shell_cmd_cat(const char *arg) {
    while (*arg == ' ') {
        ++arg;
    }
    if (*arg == '\0') {
        console_write_string("cat usage: cat <path>\n");
        console_write_string("examples: cat /boot/init/readme.txt\n");
        return;
    }

    mvos_vfs_file_t file;
    if (vfs_open(arg, &file) != 0) {
        console_write_string("cat: not found: ");
        console_write_string(arg);
        console_write_string("\n");
        return;
    }

    uint8_t buffer[64];
    console_write_string("[cat] ");
    console_write_string(arg);
    console_write_string(":\n");
    for (;;) {
        uint64_t read_bytes = 0;
        int status = vfs_read(&file, buffer, sizeof(buffer), &read_bytes);
        if (status != 0 || read_bytes == 0) {
            break;
        }
        for (uint64_t i = 0; i < read_bytes; ++i) {
            console_write_char((char)buffer[i]);
        }
    }
    console_write_string("\n");
    vfs_close(&file);
}

static void shell_cmd_run_help(void) {
    console_write_string("run usage:\n");
    console_write_string("  run                 list available user apps\n");
    console_write_string("  run --help          show this message\n");
    console_write_string("  run list            list available user apps\n");
    console_write_string("  run --status        show user apps and execution mode\n");
    console_write_string("  run help            same as run --help\n");
    console_write_string("  run <name>          execute app by name\n");
}

static void shell_cmd_run_list(void) {
    uint64_t count = userapp_count();
    console_write_string("available user apps:\n");
    for (uint64_t i = 0; i < count; ++i) {
        const char *name = userapp_name(i);
        const char *desc = userapp_desc(i);
        console_write_string("  ");
        console_write_string(name == NULL ? "(unnamed)" : name);
        console_write_string(" - ");
        console_write_string(desc == NULL ? "(no description)" : desc);
        console_write_string("\n");
    }
    if (count == 0) {
        console_write_string("  (none)\n");
    }
}

static void shell_cmd_run_status(void) {
    uint64_t count = userapp_count();
    uint64_t user_count = 0;
    uint64_t kernel_count = 0;
    console_write_string("run status:\n");
    console_write_string("  total=");
    console_write_u64(count);
    console_write_string(" apps\n");
    for (uint64_t i = 0; i < count; ++i) {
        const char *name = userapp_name(i);
        const char *desc = userapp_desc(i);
        int user_mode = userapp_is_user_mode(i);
        console_write_string("  ");
        console_write_u64(i);
        console_write_string(". ");
        console_write_string(name == NULL ? "(unnamed)" : name);
        console_write_string(" [");
        if (user_mode == 1) {
            console_write_string("user");
            ++user_count;
        } else {
            console_write_string("kernel");
            ++kernel_count;
        }
        console_write_string("] ");
        console_write_string(desc == NULL ? "(no description)" : desc);
        console_write_string("\n");
    }
    console_write_string("  summary: user=");
    console_write_u64(user_count);
    console_write_string(" kernel=");
    console_write_u64(kernel_count);
    console_write_string("\n");
}

static void shell_print_help(void) {
    console_write_string("Available commands:\n");
    console_write_string("  help   - show this help\n");
    console_write_string("  mem    - print memory allocator stats\n");
    console_write_string("  ticks  - print current timer ticks\n");
    console_write_string("  tasks  - print scheduler task list\n");
    console_write_string("  reboot - reset the machine\n");
    console_write_string("  halt   - stop execution\n");
    console_write_string("  hello  - print hello from shell\n");
    console_write_string("  gui    - draw a tiny demo window (requires graphics backend)\n");
    console_write_string("  app    - launch a tiny GUI app demo (requires graphics backend)\n");
    console_write_string("         usage: app [alt|status|list|launch <name>|info <name>]\n");
    console_write_string("  run    - list or run built-in user apps\n");
    console_write_string("         usage: run [list|status|help|--help|<name>]\n");
    console_write_string("  ls     - list virtual filesystem entries\n");
    console_write_string("         usage: ls [prefix]\n");
    console_write_string("  cat    - print a virtual file content\n");
    console_write_string("         usage: cat <path>\n");
    console_write_string("  echo   - echo text after command\n");
    console_write_string("  panic  - trigger kernel panic path\n");
    console_write_string("  clear  - clear current command line\n");
    console_write_string("  version - show kernel phase banner\n");
    console_write_string("  quit   - halt execution\n");
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void shell_reboot(void) {
    console_write_string("rebooting...\n");

    outb(0x64, 0xfe);

    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void shell_halt(void) {
    console_write_string("halting...\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static int shell_read_line(char *buffer, size_t capacity) {
    size_t len = 0;
    for (;;) {
        int ch = keyboard_read_char();
        if (ch == '\b') {
            if (len > 0) {
                --len;
                console_write_char('\b');
                console_write_char(' ');
                console_write_char('\b');
            }
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            console_write_char('\n');
            buffer[len] = '\0';
            return 1;
        }
        if (ch == 0) {
            continue;
        }
        if (len + 1 < capacity && (char)ch >= ' ') {
            buffer[len++] = (char)ch;
            console_write_char((char)ch);
        }
    }
}

static void shell_exec(const char *line) {
    char trimmed[128];
    size_t len = shell_strlen(line);
    size_t start = 0;
    size_t end = len;

    while (end > start && line[end - 1] == ' ') {
        --end;
    }
    while (start < end && line[start] == ' ') {
        ++start;
    }
    if (end <= start) {
        return;
    }

    if ((end - start) >= sizeof trimmed) {
        end = start + (sizeof trimmed - 1);
    }
    for (size_t i = start; i < end; ++i) {
        trimmed[i - start] = line[i];
    }
    trimmed[end - start] = '\0';

    const char *trimmed_line = trimmed;
    const char *arg = trimmed_line;
    while (*arg != '\0' && *arg != ' ') {
        ++arg;
    }
    size_t cmd_len = (size_t)(arg - trimmed_line);

    if (cmd_len == 4 && shell_streq(trimmed_line, "help")) {
        while (*arg == ' ') {
            ++arg;
        }
        if (*arg == '\0') {
            shell_print_help();
            return;
        }
        if (shell_streq(arg, "run")) {
            shell_cmd_run_help();
            return;
        }
        console_write_string("unknown help topic: ");
        console_write_string(arg);
        console_write_string("\n");
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "mem")) {
        console_write_string("pmm free=");
        console_write_u64(pmm_free_pages());
        console_write_string(" total=");
        console_write_u64(pmm_total_pages());
        console_write_string(" allocated=");
        console_write_u64(pmm_allocated_pages());
        console_write_string("\n");
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "ticks")) {
        console_write_string("ticks=");
        console_write_u64(timer_ticks());
        console_write_string("\n");
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "tasks")) {
        uint32_t task_count = scheduler_task_count();
        console_write_string("scheduler tasks=");
        console_write_u64((uint64_t)task_count);
        console_write_string("\n");
        for (uint32_t i = 0; i < task_count; ++i) {
            const char *name = scheduler_task_name(i);
            uint64_t runs = scheduler_task_runs(i);
            console_write_string("  #");
            console_write_u64((uint64_t)i);
            console_write_string(" ");
            console_write_string(name == NULL ? "(nil)" : name);
            console_write_string(" runs=");
            console_write_u64(runs);
            console_write_string("\n");
        }
        return;
    }
    if (cmd_len == 6 && shell_streq(trimmed_line, "reboot")) {
        shell_reboot();
        return;
    }
    if (cmd_len == 4 && shell_streq(trimmed_line, "halt")) {
        shell_halt();
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "hello")) {
        console_write_string("hello from shell\n");
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "build")) {
        console_write_string("host build (outside miniOS):\n");
        console_write_string("  make host-programs\n");
        console_write_string("  python3 scripts/dev_status.py --build-programs\n");
        console_write_string("  python3 scripts/build_user_programs.py --source-dir samples/user-programs --out-dir build/host-programs\n");
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "gui")) {
        if (console_graphics_enabled()) {
            console_draw_gui_boot_window();
            console_write_string("GUI demo window drawn.\n");
        } else {
            console_write_string("GUI backend unavailable (no framebuffer graphics enabled).\n");
        }
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "run")) {
        while (*arg == ' ') {
            ++arg;
        }
        if (*arg == '\0') {
            shell_cmd_run_list();
            return;
        }
    if (shell_streq(arg, "status") || shell_streq(arg, "--status")) {
        shell_cmd_run_status();
        return;
    }
    if (shell_streq(arg, "list")) {
        shell_cmd_run_list();
        return;
    }
    if (shell_streq(arg, "-h") || shell_streq(arg, "--help")) {
        shell_cmd_run_help();
        return;
    }
        if (shell_streq(arg, "help")) {
            shell_cmd_run_help();
            return;
        }
        int status = userapp_run(arg);
        if (status == 0) {
            console_write_string("user app executed: ");
            console_write_string(arg);
            console_write_string("\n");
        } else if (status == -1) {
            shell_cmd_run_help();
        } else {
            console_write_string("unknown user app: ");
            console_write_string(arg);
            console_write_string("\n");
        }
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "app")) {
        if (!console_graphics_enabled()) {
            console_write_string("GUI backend unavailable (no framebuffer graphics enabled).\n");
            return;
        }
        while (*arg == ' ') {
            ++arg;
        }
        if (*arg == '\0') {
            console_launch_demo_gui_app();
            console_write_string("GUI app launched.\n");
            return;
        }
        if (shell_streq(arg, "alt")) {
            console_launch_demo_gui_alt_app();
            console_write_string("GUI alt app launched.\n");
            return;
        }
        if (shell_streq(arg, "list")) {
            console_write_string("available gui apps: app, app alt\n");
            return;
        }
        if (shell_streq(arg, "status")) {
            console_write_graphics_status();
            console_write_string("GUI app status requested.\n");
            return;
        }
        if (shell_streq(arg, "launch")) {
            const char *launch_target = shell_skip_spaces(arg + 6u);
            if (*launch_target == '\0') {
                console_write_string("GUI app launch usage: app launch <name>\n");
                return;
            }
            if (shell_streq(launch_target, "app")) {
                console_launch_demo_gui_app();
                console_write_string("GUI app launched.\n");
                return;
            }
            if (shell_streq(launch_target, "alt")) {
                console_launch_demo_gui_alt_app();
                console_write_string("GUI alt app launched.\n");
                return;
            }
            console_write_string("unknown app: ");
            console_write_string(launch_target);
            console_write_string("\n");
            return;
        }
        if (shell_streq(arg, "info")) {
            const char *target = shell_skip_spaces(arg + 4u);
            if (*target == '\0') {
                console_write_string("GUI app info usage: app info <name>\n");
                return;
            }
            shell_print_app_info(target);
            return;
        }
        console_write_string("GUI app usage: app [alt|list|status|launch <name>|info <name>]\n");
        return;
    }
    if (cmd_len == 2 && shell_streq(trimmed_line, "ls")) {
        shell_cmd_ls(arg);
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "cat")) {
        shell_cmd_cat(arg);
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "panic")) {
        panic("panic command issued");
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "clear")) {
        console_write_string("\x1b[2J\x1b[H");
        return;
    }
    if (cmd_len == 7 && shell_streq(trimmed_line, "version")) {
        console_write_string("MiniOS Phase 22 (phase-based tutorial kernel)\n");
        return;
    }
    if (cmd_len == 4 && shell_streq(trimmed_line, "echo")) {
        while (*arg == ' ') {
            ++arg;
        }
        if (*arg == '\0') {
            console_write_string("\n");
        } else {
            console_write_string(arg);
            console_write_string("\n");
        }
        return;
    }
    if (cmd_len == 4 && shell_streq(trimmed_line, "quit")) {
        console_write_string("halting...\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    console_write_string("unknown command: ");
    console_write_string(trimmed_line);
    console_write_string("\n");
}

void shell_run(void) {
    static char line[SHELL_BUFFER_LEN];
    console_write_string("MiniOS shell (phase 21)\n");
    shell_print_help();
    for (;;) {
        shell_print_prompt();
        if (shell_read_line(line, sizeof(line))) {
            shell_exec(line);
        }
    }
}
