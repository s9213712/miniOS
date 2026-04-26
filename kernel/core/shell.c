#include <mvos/shell.h>
#include <mvos/shell_parser.h>
#include <mvos/shell_vfs_commands.h>
#include <mvos/console.h>
#include <mvos/keyboard.h>
#include <mvos/pmm.h>
#include <mvos/scheduler.h>
#include <mvos/vfs.h>
#include <mvos/userapp.h>
#include <mvos/vmm.h>
#include <mvos/interrupt.h>
#include <mvos/panic.h>
#include <mvos/serial.h>
#include <stdint.h>
#include <stddef.h>

#define SHELL_BUFFER_LEN 128
#define SHELL_PROMPT "mvos> "
#define SHELL_HISTORY_LINES 12

static int shell_read_char_raw(void);

static char g_shell_history[SHELL_HISTORY_LINES][SHELL_BUFFER_LEN];
static int g_shell_history_count;
static int g_shell_history_cursor;

static void shell_print_prompt(void) {
    console_write_string(SHELL_PROMPT);
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

static int shell_is_token(const char *arg, const char *token) {
    if (arg == NULL || token == NULL) {
        return 0;
    }
    size_t i = 0;
    while (token[i] != '\0' && arg[i] != '\0' && token[i] == arg[i]) {
        ++i;
    }
    if (token[i] != '\0') {
        return 0;
    }
    return arg[i] == '\0' || arg[i] == ' ';
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

static inline void shell_io_wait(void) {
    __asm__ volatile("pause");
}

static void shell_history_push(const char *line) {
    if (line == NULL) {
        return;
    }

    size_t end = shell_strlen(line);
    size_t start = 0;
    while (start < end && line[start] == ' ') {
        ++start;
    }
    while (end > start && line[end - 1] == ' ') {
        --end;
    }
    if (end <= start) {
        return;
    }

    size_t normalized_len = end - start;
    if (normalized_len >= SHELL_BUFFER_LEN) {
        normalized_len = SHELL_BUFFER_LEN - 1;
    }

    if (g_shell_history_count > 0) {
        const char *latest = g_shell_history[g_shell_history_count - 1];
        size_t latest_len = shell_strlen(latest);
        if (latest_len == normalized_len && normalized_len > 0) {
            int duplicate = 1;
            for (size_t i = 0; i < normalized_len; ++i) {
                if (latest[i] != line[start + i]) {
                    duplicate = 0;
                    break;
                }
            }
            if (duplicate) {
                return;
            }
        }
    }

    if (g_shell_history_count >= SHELL_HISTORY_LINES) {
        for (size_t i = 1; i < SHELL_HISTORY_LINES; ++i) {
            for (size_t j = 0; j < SHELL_BUFFER_LEN; ++j) {
                g_shell_history[i - 1][j] = g_shell_history[i][j];
            }
        }
        g_shell_history_count = SHELL_HISTORY_LINES;
    }
    char *slot = g_shell_history[g_shell_history_count == SHELL_HISTORY_LINES ? SHELL_HISTORY_LINES - 1 : g_shell_history_count];
    for (size_t i = 0; i < normalized_len; ++i) {
        slot[i] = line[start + i];
    }
    slot[normalized_len] = '\0';

    if (g_shell_history_count < SHELL_HISTORY_LINES) {
        ++g_shell_history_count;
    }
}

static void shell_history_load(int index, char *line_out, size_t capacity, size_t *len_out) {
    if (line_out == NULL || capacity == 0 || index < 0 || index >= g_shell_history_count) {
        if (len_out != NULL) {
            *len_out = 0;
        }
        return;
    }

    size_t i = 0;
    while (g_shell_history[index][i] != '\0' && i + 1 < capacity) {
        line_out[i] = g_shell_history[index][i];
        ++i;
    }
    line_out[i] = '\0';
    if (len_out != NULL) {
        *len_out = i;
    }
}

static void shell_draw_edit_line(const char *line, size_t len, size_t cursor_pos, size_t previous_len) {
    console_write_string("\r");
    console_write_string(SHELL_PROMPT);
    for (size_t i = 0; i < len; ++i) {
        console_write_char(line[i]);
    }

    size_t cover_len = previous_len;
    if (len > cover_len) {
        cover_len = len;
    }
    if (cover_len > len) {
        for (size_t i = 0; i < (cover_len - len); ++i) {
            console_write_char(' ');
        }
    }

    size_t backspaces = cover_len;
    if (cover_len > cursor_pos) {
        backspaces = cover_len - cursor_pos;
    } else {
        backspaces = 0;
    }
    for (size_t i = 0; i < backspaces; ++i) {
        console_write_char('\b');
    }
}

static int shell_read_ansi_key(void) {
    int c = shell_read_char_raw();
    if (c < 0) {
        for (int timeout = 0; timeout < 1024; ++timeout) {
            shell_io_wait();
            c = shell_read_char_raw();
            if (c >= 0) {
                break;
            }
        }
    }
    if (c != '[') {
        return 0;
    }

    c = shell_read_char_raw();
    if (c < 0) {
        for (int timeout = 0; timeout < 1024; ++timeout) {
            shell_io_wait();
            c = shell_read_char_raw();
            if (c >= 0) {
                break;
            }
        }
    }
    if (c == 'A') {
        return KEY_UP;
    }
    if (c == 'B') {
        return KEY_DOWN;
    }
    if (c == 'C') {
        return KEY_RIGHT;
    }
    if (c == 'D') {
        return KEY_LEFT;
    }

    if (c >= '0' && c <= '9') {
        for (int timeout = 0; timeout < 1024; ++timeout) {
            int n = shell_read_char_raw();
            if (n < 0) {
                shell_io_wait();
                continue;
            }
            if (n == '~') {
                break;
            }
        }
    }

    return 0;
}

static int shell_read_char_raw(void) {
    int c = keyboard_read_char_nonblocking();
    if (c >= 0) {
        return c;
    }
    return serial_read_char_nonblocking();
}

static int shell_read_char(void) {
    for (;;) {
        int c = shell_read_char_raw();
        if (c < 0) {
            shell_io_wait();
            continue;
        }
        if (c == 0x1b) {
            int key = shell_read_ansi_key();
            if (key != 0) {
                return key;
            }
            continue;
        }
        return c;
    }
}

static int shell_parse_u32(const char *s, uint32_t *out) {
    if (s == NULL || s[0] == '\0' || out == NULL) {
        return -1;
    }
    uint64_t value = 0;
    uint64_t i = 0;
    while (s[i] != '\0') {
        char ch = s[i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = value * 10u + (uint64_t)(ch - '0');
        if (value > 0xffffffffu) {
            return -1;
        }
        ++i;
    }
    *out = (uint32_t)value;
    return 0;
}

static void shell_print_meter(const char *label, int has_sample, uint64_t percent) {
    const uint64_t bar_width = 24;
    uint64_t clamped = has_sample ? (percent > 100 ? 100 : percent) : 0;
    console_write_string(label);
    console_write_string(" ");
    console_write_string("[");
    for (uint64_t i = 0; i < bar_width; ++i) {
        console_write_char(i < (clamped * bar_width) / 100 ? '#' : '.');
    }
    console_write_string("] ");
    if (!has_sample) {
        console_write_string("n/a ");
    } else {
        if (clamped < 100) {
            console_write_char(' ');
        }
        if (clamped < 10) {
            console_write_char(' ');
        }
        console_write_u64(clamped);
        console_write_string("% ");
    }
}

static uint64_t shell_sum_task_runs(void) {
    uint32_t task_count = scheduler_task_count();
    uint64_t total = 0;
    for (uint32_t i = 0; i < task_count; ++i) {
        total += scheduler_task_runs(i);
    }
    return total;
}

static uint64_t g_htop_last_ticks;
static uint64_t g_htop_last_task_runs;
static int g_htop_has_history;

static int shell_htop_check_quit(void) {
    int key = shell_read_char_raw();
    return key == 'q' || key == 'Q';
}

static int shell_htop_sleep_ticks(uint64_t duration_ticks) {
    uint64_t start = timer_ticks();
    while ((timer_ticks() - start) < duration_ticks) {
        if (shell_htop_check_quit()) {
            return 1;
        }
        shell_io_wait();
    }
    return 0;
}

static void shell_cmd_htop(const char *arg) {
    arg = shell_skip_spaces(arg);
    if (arg[0] != '\0') {
        console_write_string("htop usage: htop\n");
        return;
    }

    const uint64_t refresh_ticks = 10;

    g_htop_has_history = 0;
    g_htop_last_ticks = timer_ticks();
    g_htop_last_task_runs = shell_sum_task_runs();

    console_write_string("miniOS monitor (htop-style, live)\n");
    console_write_string("press q to quit\n");

    for (;;) {
        if (shell_htop_check_quit()) {
            console_write_string("\n");
            return;
        }

        uint64_t current_ticks = timer_ticks();
        uint64_t current_task_runs = shell_sum_task_runs();
        uint64_t delta_ticks = 0;
        uint64_t delta_runs = 0;
        uint64_t cpu_percent = 0;
        int cpu_sample_ready = 0;

        if (g_htop_has_history) {
            if (current_ticks >= g_htop_last_ticks) {
                delta_ticks = current_ticks - g_htop_last_ticks;
            }
            if (current_task_runs >= g_htop_last_task_runs) {
                delta_runs = current_task_runs - g_htop_last_task_runs;
            }
            if (delta_ticks > 0 && delta_runs > 0) {
                cpu_percent = (delta_runs * 100ULL) / delta_ticks;
                if (cpu_percent > 100) {
                    cpu_percent = 100;
                }
                cpu_sample_ready = 1;
            }
            if (delta_ticks > 0 && delta_runs == 0) {
                cpu_percent = 0;
                cpu_sample_ready = 1;
            }
        }

        g_htop_last_ticks = current_ticks;
        g_htop_last_task_runs = current_task_runs;
        g_htop_has_history = 1;

        uint64_t free_pages = pmm_free_pages();
        uint64_t total_pages = pmm_total_pages();
        uint64_t used_pages = pmm_allocated_pages();
        uint64_t ram_percent = (total_pages == 0) ? 0 : (used_pages * 100ULL) / total_pages;

        int gpu_enabled = console_graphics_enabled();
        uint64_t gpu_percent = gpu_enabled ? 100 : 0;

        uint64_t disk_used = 0;
        uint64_t disk_capacity = 0;
        int disk_ok = vfs_disk_usage(&disk_used, &disk_capacity);
        uint64_t disk_percent = 0;
        int disk_sample_ready = 0;
        if (disk_ok == 0 && disk_capacity > 0) {
            disk_sample_ready = 1;
            disk_percent = (disk_used * 100ULL) / disk_capacity;
            if (disk_percent > 100) {
                disk_percent = 100;
            }
        }

        console_write_string("\x1b[2J\x1b[H");
        console_write_string("miniOS monitor (htop-style, live)\n");
        console_write_string("press q to quit\n");

        shell_print_meter("CPU", cpu_sample_ready, cpu_percent);
        if (cpu_sample_ready) {
            console_write_string("window=");
            console_write_u64(delta_runs);
            console_write_string("/");
            console_write_u64(delta_ticks);
            console_write_string(" ");
        } else {
            console_write_string("sampling... press q to quit\n");
        }
        console_write_string("\n");

        console_write_string("ticks=");
        console_write_u64(current_ticks);
        console_write_string("  tasks= ");
        console_write_u64(current_task_runs);
        console_write_string("\n");

        shell_print_meter("RAM", 1, ram_percent);
        console_write_string("used=");
        console_write_u64(used_pages);
        console_write_string(" free=");
        console_write_u64(free_pages);
        console_write_string(" total=");
        console_write_u64(total_pages);
        console_write_string(" pages\n");

        shell_print_meter("GPU", 1, gpu_percent);
        console_write_string(gpu_enabled ? "enabled\n" : "disabled\n");

        shell_print_meter("DISK", disk_sample_ready, disk_percent);
        if (disk_sample_ready) {
            console_write_string("used=");
            console_write_u64(disk_used);
            console_write_string(" total=");
            console_write_u64(disk_capacity);
            console_write_string(" bytes\n");
        } else {
            console_write_string("n/a (vfs statistics unavailable)\n");
        }

        if (shell_htop_sleep_ticks(refresh_ticks)) {
            console_write_string("\n");
            return;
        }
    }
}

static void shell_tasks_list(void) {
    uint32_t task_count = scheduler_task_count();
    console_write_string("scheduler tasks=");
    console_write_u64((uint64_t)task_count);
    console_write_string("\n");
    for (uint32_t i = 0; i < task_count; ++i) {
        const char *name = scheduler_task_name(i);
        uint64_t runs = scheduler_task_runs(i);
        int active = scheduler_task_active(i);
        console_write_string("  #");
        console_write_u64((uint64_t)i);
        console_write_string(" ");
        console_write_string(name == NULL ? "(nil)" : name);
        console_write_string(" [");
        console_write_string(active == 1 ? "active" : "stopped");
        console_write_string("] runs=");
        console_write_u64(runs);
        console_write_string("\n");
    }
}

static void shell_task_usage(void) {
    console_write_string("task usage:\n");
    console_write_string("  task list\n");
    console_write_string("  task start <id|name>\n");
    console_write_string("  task stop <id|name>\n");
    console_write_string("  task reset <id|name|all>\n");
}

static int shell_task_resolve_target(const char *token, uint32_t *out_index) {
    uint32_t idx = 0;
    if (shell_parse_u32(token, &idx) == 0) {
        if (idx < scheduler_task_count()) {
            *out_index = idx;
            return 0;
        }
        return -1;
    }
    int by_name = scheduler_find_task(token);
    if (by_name < 0) {
        return -1;
    }
    *out_index = (uint32_t)by_name;
    return 0;
}

static void shell_task_parse_action(const char *arg, char *action_out, size_t action_cap, const char **rest_out) {
    const char *p = shell_skip_spaces(arg);
    size_t i = 0;
    while (p[i] != '\0' && p[i] != ' ' && i + 1 < action_cap) {
        action_out[i] = p[i];
        ++i;
    }
    action_out[i] = '\0';
    p += i;
    *rest_out = shell_skip_spaces(p);
}

static void shell_task_parse_target(const char *arg, char *target_out, size_t target_cap) {
    const char *p = shell_skip_spaces(arg);
    size_t i = 0;
    while (p[i] != '\0' && p[i] != ' ' && i + 1 < target_cap) {
        target_out[i] = p[i];
        ++i;
    }
    target_out[i] = '\0';
}

static void shell_cmd_task(const char *arg) {
    char action[16];
    const char *rest = NULL;
    shell_task_parse_action(arg, action, sizeof(action), &rest);
    if (action[0] == '\0') {
        shell_task_usage();
        return;
    }
    if (shell_streq(action, "list")) {
        shell_tasks_list();
        return;
    }
    if (shell_streq(action, "help")) {
        shell_task_usage();
        return;
    }

    char target[32];
    shell_task_parse_target(rest, target, sizeof(target));
    if (target[0] == '\0') {
        shell_task_usage();
        return;
    }

    if (shell_streq(action, "reset") && shell_streq(target, "all")) {
        scheduler_reset_all_task_runs();
        console_write_string("task reset ok: all\n");
        return;
    }

    uint32_t index = 0;
    if (shell_task_resolve_target(target, &index) != 0) {
        console_write_string("task target not found: ");
        console_write_string(target);
        console_write_string("\n");
        return;
    }

    if (shell_streq(action, "start")) {
        scheduler_set_task_active(index, 1);
        console_write_string("task started: #");
        console_write_u64(index);
        console_write_string(" ");
        console_write_string(scheduler_task_name(index));
        console_write_string("\n");
        return;
    }
    if (shell_streq(action, "stop")) {
        scheduler_set_task_active(index, 0);
        console_write_string("task stopped: #");
        console_write_u64(index);
        console_write_string(" ");
        console_write_string(scheduler_task_name(index));
        console_write_string("\n");
        return;
    }
    if (shell_streq(action, "reset")) {
        scheduler_reset_task_runs(index);
        console_write_string("task runs reset: #");
        console_write_u64(index);
        console_write_string(" ");
        console_write_string(scheduler_task_name(index));
        console_write_string("\n");
        return;
    }

    shell_task_usage();
}

static void shell_vmm_print_flags(uint64_t flags) {
    console_write_char((flags & MVOS_VMM_FLAG_READ) ? 'r' : '-');
    console_write_char((flags & MVOS_VMM_FLAG_WRITE) ? 'w' : '-');
    console_write_char((flags & MVOS_VMM_FLAG_EXEC) ? 'x' : '-');
    console_write_char((flags & MVOS_VMM_FLAG_USER) ? 'u' : '-');
}

static void shell_cmd_vmm(const char *arg) {
    const char *sub = shell_skip_spaces(arg);
    if (sub[0] != '\0' && !shell_streq(sub, "list") && !shell_streq(sub, "status")) {
        console_write_string("vmm usage: vmm [list|status]\n");
        return;
    }

    uint32_t count = vmm_region_count();
    console_write_string("vmm regions=");
    console_write_u64((uint64_t)count);
    console_write_string("\n");
    for (uint32_t i = 0; i < count; ++i) {
        mvos_vmm_region_info_t region;
        if (vmm_region_at(i, &region) != 0) {
            continue;
        }
        console_write_string("  #");
        console_write_u64((uint64_t)i);
        console_write_string(" ");
        console_write_string(region.tag);
        console_write_string(" base=");
        console_write_u64(region.base);
        console_write_string(" size=");
        console_write_u64(region.size);
        console_write_string(" flags=");
        shell_vmm_print_flags(region.flags);
        console_write_string("\n");
    }
    console_write_string("vmm user_brk=");
    console_write_u64(vmm_user_brk_get());
    console_write_string(" limit=");
    console_write_u64(vmm_user_brk_limit());
    console_write_string("\n");
}

static void shell_cmd_run_help(void) {
    console_write_string("run usage:\n");
    console_write_string("  run                 list available user apps\n");
    console_write_string("  run --help          show this message\n");
    console_write_string("  run list            list available user apps\n");
    console_write_string("  run --status        show user apps and execution mode\n");
    console_write_string("  run python <path>    execute mini python subset script\n");
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

static void shell_cmd_capabilities(void) {
    uint64_t count = userapp_count();
    uint64_t user_count = 0;
    uint64_t kernel_count = 0;

    console_write_string("miniOS capability matrix:\n");
    console_write_string("  boot:\n");
    console_write_string("    - Limine handoff + serial-first startup logging\n");
    console_write_string("    - framebuffer diagnostics (if available)\n");
    console_write_string("  runtime:\n");
    console_write_string("    - kernel-mode shell + builtin user-app demos\n");
    console_write_string("    - VFS bootstrap files via ls/cat + writable /tmp overlay\n");
    console_write_string("    - scheduler + timer observability with task start/stop/reset controls\n");
    console_write_string("    - VMM region metadata + user brk arena tracking\n");
    console_write_string("  user apps in table:\n");
    for (uint64_t i = 0; i < count; ++i) {
        const char *name = userapp_name(i);
        const char *desc = userapp_desc(i);
        int user_mode = userapp_is_user_mode(i);
        console_write_string("    ");
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
    console_write_string("  totals: user=");
    console_write_u64(user_count);
    console_write_string(" kernel=");
    console_write_u64(kernel_count);
    console_write_string("\n");
    console_write_string("  host tooling:\n");
    console_write_string("    - `make host-programs` compiles host C/C++ demos\n");
    console_write_string("    - `c-compile` prints host C/C++ compile modes in-shell\n");
    console_write_string("    - `make test-vfs-rw` validates writable /tmp VFS behavior\n");
    console_write_string("    - `make test-scheduler-ctl` validates scheduler task controls\n");
    console_write_string("    - `make test-vmm-basic` validates VMM map/unmap and brk bounds\n");
    console_write_string("    - `make test-userimg-loader` validates ELF load+stack mapping, handoff checks, and exec stack prep\n");
    console_write_string("    - `python3 scripts/dev_status.py --build-programs` validates build chain\n");
    console_write_string("    - `make refresh-elf-sample` regenerates embedded Linux ELF sample blob\n");
    console_write_string("    - `make test-elf-sample` validates regenerated ELF sample contract\n");
    console_write_string("  linux abi preview:\n");
    console_write_string("    - user syscall subset: read/write/writev/close/fstat/lseek/mmap/mprotect/munmap/brk/access/getcwd/uname/getpid/gettid/set_tid_address/arch_prctl/clock_gettime/execve/exit_group/openat/newfstatat/faccessat/getrandom\n");
    console_write_string("    - try: run linux-abi\n");
    console_write_string("    - inspect sample ELF: run elf-inspect\n");
    console_write_string("  not yet supported in miniOS runtime:\n");
    console_write_string("    - Full Python interpreter\n");
    console_write_string("    - Linux native executables (transmission/htop/nano)\n");
    console_write_string("    - full user page-table mapping + real ring3 isolation\n");
    console_write_string("    - general ELF userspace execution beyond the built-in tiny static sample\n");
    console_write_string("    - persistent disk filesystem\n");
}

static void shell_cmd_c_compile(const char *arg) {
    const char *mode = shell_skip_spaces(arg);
    console_write_string("miniOS host C/C++ compile helper:\n");

    if (mode[0] == '\0' || shell_streq(mode, "help") || shell_streq(mode, "-h") || shell_streq(mode, "--help")) {
        console_write_string("  c-compile [mode]\n");
        console_write_string("  modes:\n");
        console_write_string("    default : make host-programs (dynamic link)\n");
        console_write_string("    static  : MHOST_STATIC=1 make host-programs\n");
        console_write_string("    nocache : MHOST_NO_CACHE=1 make host-programs\n");
        console_write_string("    full    : static + nocache\n");
        console_write_string("    script  : python3 scripts/build_user_programs.py --source-dir samples/user-programs --out-dir build/host-programs\n");
        console_write_string("  example:\n");
        console_write_string("    c-compile full\n");
        console_write_string("  compiled output:\n");
        console_write_string("    build/host-programs/<stem>_c and build/host-programs/<stem>_cpp\n");
        return;
    }

    if (shell_streq(mode, "default")) {
        console_write_string("  cd <miniOS-repo-root>\n");
        console_write_string("  make host-programs\n");
        return;
    }
    if (shell_streq(mode, "static")) {
        console_write_string("  cd <miniOS-repo-root>\n");
        console_write_string("  MHOST_STATIC=1 make host-programs\n");
        return;
    }
    if (shell_streq(mode, "nocache")) {
        console_write_string("  cd <miniOS-repo-root>\n");
        console_write_string("  MHOST_NO_CACHE=1 make host-programs\n");
        return;
    }
    if (shell_streq(mode, "full")) {
        console_write_string("  cd <miniOS-repo-root>\n");
        console_write_string("  MHOST_STATIC=1 MHOST_NO_CACHE=1 make host-programs\n");
        return;
    }
    if (shell_streq(mode, "script")) {
        console_write_string("  cd <miniOS-repo-root>\n");
        console_write_string("  python3 scripts/build_user_programs.py --source-dir samples/user-programs --out-dir build/host-programs\n");
        return;
    }

    console_write_string("unknown c-compile mode: ");
    console_write_string(mode);
    console_write_string("\n");
    console_write_string("run 'c-compile help' for supported modes\n");
}

static void shell_print_help(void) {
    console_write_string("Available commands:\n");
    console_write_string("  help   - show this help\n");
    console_write_string("  mem    - print memory allocator stats\n");
    console_write_string("  ticks  - print current timer ticks\n");
    console_write_string("  tasks  - print scheduler task list\n");
    console_write_string("  task   - control scheduler tasks\n");
    console_write_string("         usage: task [list|start <id|name>|stop <id|name>|reset <id|name|all>]\n");
    console_write_string("  vmm    - print VMM regions and user brk status\n");
    console_write_string("         usage: vmm [list|status]\n");
    console_write_string("  reboot - reset the machine\n");
    console_write_string("  halt   - stop execution\n");
    console_write_string("  hello  - print hello from shell\n");
    console_write_string("  gui    - draw a tiny demo window (requires graphics backend)\n");
    console_write_string("  app    - launch a tiny GUI app demo (requires graphics backend)\n");
    console_write_string("         usage: app [alt|status|list|launch <name>|info <name>]\n");
    console_write_string("  htop   - live system monitor with CPU/RAM/GPU/DISK bars (press q to quit)\n");
    console_write_string("  sysmon - alias of htop\n");
    console_write_string("  c-compile - show host-side C/C++ compile workflow\n");
    console_write_string("  run    - list or run built-in user apps\n");
    console_write_string("         usage: run [list|status|help|--help|<name>]\n");
    console_write_string("  run python - run a tiny Python subset on a file path\n");
    console_write_string("  cap    - print capability matrix for current miniOS build\n");
    console_write_string("  capabilities - alias of cap\n");
    console_write_string("  ls     - list virtual filesystem entries\n");
    console_write_string("         usage: ls [prefix]\n");
    console_write_string("  cat    - print a virtual file content\n");
    console_write_string("         usage: cat <path>\n");
    console_write_string("  write  - create/overwrite a writable /tmp file\n");
    console_write_string("         usage: write <path> <text>\n");
    console_write_string("  append - append text to a writable /tmp file\n");
    console_write_string("         usage: append <path> <text>\n");
    console_write_string("  touch  - create an empty writable /tmp file\n");
    console_write_string("         usage: touch <path>\n");
    console_write_string("  rm     - remove a writable /tmp file\n");
    console_write_string("         usage: rm <path>\n");
    console_write_string("  echo   - echo text after command\n");
    console_write_string("  nano   - open nano-like editor for /tmp files\n");
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
    g_shell_history_cursor = -1;
    size_t len = 0;
    size_t cursor = 0;
    size_t previous_len = 0;

    for (;;) {
        int ch = shell_read_char();
        if (ch == '\b' || ch == 0x7f) {
            if (cursor == 0) {
                continue;
            }
            previous_len = len;
            --cursor;
            --len;
            for (size_t i = cursor; i < len; ++i) {
                buffer[i] = buffer[i + 1];
            }
            shell_draw_edit_line(buffer, len, cursor, previous_len);
            continue;
        }
        if (ch == KEY_LEFT) {
            if (cursor > 0) {
                --cursor;
                console_write_char('\b');
            }
            continue;
        }
        if (ch == KEY_RIGHT) {
            if (cursor < len) {
                console_write_char(buffer[cursor]);
                ++cursor;
            }
            continue;
        }
        if (ch == KEY_UP) {
            if (g_shell_history_count == 0) {
                continue;
            }
            previous_len = len;
            if (g_shell_history_cursor < 0) {
                g_shell_history_cursor = g_shell_history_count - 1;
            } else if (g_shell_history_cursor > 0) {
                --g_shell_history_cursor;
            }
            shell_history_load(g_shell_history_cursor, buffer, capacity, &len);
            cursor = len;
            shell_draw_edit_line(buffer, len, cursor, previous_len);
            continue;
        }
        if (ch == KEY_DOWN) {
            if (g_shell_history_cursor < 0) {
                continue;
            }
            previous_len = len;
            if (g_shell_history_cursor >= g_shell_history_count - 1) {
                g_shell_history_cursor = -1;
                len = 0;
                cursor = 0;
                buffer[0] = '\0';
                shell_draw_edit_line(buffer, len, cursor, previous_len);
                continue;
            }
            ++g_shell_history_cursor;
            shell_history_load(g_shell_history_cursor, buffer, capacity, &len);
            cursor = len;
            shell_draw_edit_line(buffer, len, cursor, previous_len);
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            console_write_char('\n');
            buffer[len] = '\0';
            shell_history_push(buffer);
            return 1;
        }
        if (ch == 0) {
            continue;
        }
        if (ch < 0x20 && ch != '\t') {
            continue;
        }
        if (len + 1 < capacity) {
            if (cursor == len) {
                previous_len = len;
                buffer[len++] = (char)ch;
                buffer[len] = '\0';
                ++cursor;
                console_write_char((char)ch);
            } else {
                previous_len = len;
                for (size_t i = len; i > cursor; --i) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[cursor] = (char)ch;
                ++cursor;
                ++len;
                buffer[len] = '\0';
                shell_draw_edit_line(buffer, len, cursor, previous_len);
            }
        }
    }
}

static void shell_exec(const char *line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }
    mvos_shell_parse_result_t parsed;
    if (shell_parse_line(line, &parsed) != 0) {
        return;
    }
    const char *trimmed_line = parsed.command;
    const char *arg = parsed.arg;
    size_t cmd_len = shell_strlen(trimmed_line);

    if (cmd_len == 4 && shell_streq(trimmed_line, "help")) {
        while (*arg == ' ') {
            ++arg;
        }
        if (*arg == '\0') {
            shell_print_help();
            return;
        }
        if (shell_streq(arg, "c-compile")) {
            shell_cmd_c_compile("help");
            return;
        }
        if (shell_streq(arg, "run")) {
            shell_cmd_run_help();
            return;
        }
        if (shell_streq(arg, "task")) {
            shell_task_usage();
            return;
        }
        if (shell_streq(arg, "vmm")) {
            console_write_string("vmm usage: vmm [list|status]\n");
            return;
        }
        if (shell_streq(arg, "htop") || shell_streq(arg, "sysmon")) {
            console_write_string("htop - show a quick 4-metric monitor\n");
            console_write_string("command: htop\n");
            return;
        }
        if (shell_streq(arg, "nano")) {
            console_write_string("nano - nano-like editor\n");
            console_write_string("command: nano <path>\n");
            return;
        }
        if (shell_streq(arg, "python")) {
            console_write_string("python - mini python subset runner\n");
            console_write_string("command: run python <path>\n");
            console_write_string("supports: print('str'|expr), name = expr, + - * / and parentheses\n");
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
        shell_tasks_list();
        return;
    }
    if (cmd_len == 4 && shell_streq(trimmed_line, "task")) {
        shell_cmd_task(arg);
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "vmm")) {
        shell_cmd_vmm(arg);
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
    if (
        (cmd_len == 3 && shell_streq(trimmed_line, "cap")) ||
        (cmd_len == 4 && shell_streq(trimmed_line, "caps")) ||
        (cmd_len == 11 && shell_streq(trimmed_line, "capabilities"))
    ) {
        shell_cmd_capabilities();
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "build")) {
        console_write_string("host build (outside miniOS):\n");
        console_write_string("  c-compile            # see supported host compile modes\n");
        console_write_string("  make host-programs\n");
        console_write_string("  make test-vfs-rw\n");
        console_write_string("  make test-scheduler-ctl\n");
        console_write_string("  make test-vmm-basic\n");
        console_write_string("  make test-userimg-loader\n");
        console_write_string("  make test-elf-sample\n");
        console_write_string("  python3 scripts/dev_status.py --build-programs\n");
        console_write_string("  python3 scripts/build_user_programs.py --source-dir samples/user-programs --out-dir build/host-programs\n");
        return;
    }
    if (cmd_len == 9 && shell_streq(trimmed_line, "c-compile")) {
        shell_cmd_c_compile(arg);
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
        if (shell_is_token(arg, "python")) {
            shell_cmd_python(arg);
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
    if ((cmd_len == 4 && shell_streq(trimmed_line, "htop")) ||
        (cmd_len == 6 && shell_streq(trimmed_line, "sysmon"))) {
        shell_cmd_htop(arg);
        return;
    }
    if (cmd_len == 2 && shell_streq(trimmed_line, "ls")) {
        shell_cmd_ls(arg);
        return;
    }
    if (cmd_len == 4 && shell_streq(trimmed_line, "nano")) {
        shell_cmd_nano(arg);
        return;
    }
    if (cmd_len == 3 && shell_streq(trimmed_line, "cat")) {
        shell_cmd_cat(arg);
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "write")) {
        shell_cmd_write_file(arg, 0);
        return;
    }
    if (cmd_len == 6 && shell_streq(trimmed_line, "append")) {
        shell_cmd_write_file(arg, 1);
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "touch")) {
        shell_cmd_touch(arg);
        return;
    }
    if (cmd_len == 2 && shell_streq(trimmed_line, "rm")) {
        shell_cmd_rm(arg);
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
        console_write_string("MiniOS Stage 4 (Phase 47: fcntl dupfd variants)\n");
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
    console_write_string("MiniOS shell (stage 4, phase 47)\n");
    shell_print_help();
    for (;;) {
        shell_print_prompt();
        if (shell_read_line(line, sizeof(line))) {
            shell_exec(line);
        }
    }
}
