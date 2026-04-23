#include <mvos/shell.h>
#include <mvos/console.h>
#include <mvos/keyboard.h>
#include <mvos/panic.h>
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

static void shell_print_help(void) {
    console_write_string("Available commands:\n");
    console_write_string("  help   - show this help\n");
    console_write_string("  hello  - print hello from shell\n");
    console_write_string("  echo   - echo text after command\n");
    console_write_string("  panic  - trigger kernel panic path\n");
    console_write_string("  clear  - clear current command line\n");
    console_write_string("  version- show kernel phase banner\n");
    console_write_string("  quit   - halt execution\n");
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
        shell_print_help();
        return;
    }
    if (cmd_len == 5 && shell_streq(trimmed_line, "hello")) {
        console_write_string("hello from shell\n");
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
        console_write_string("MiniOS Phase 4 (serial+keyboard)\n");
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
    console_write_string("MiniOS shell (phase 4)\n");
    shell_print_help();
    for (;;) {
        shell_print_prompt();
        if (shell_read_line(line, sizeof(line))) {
            shell_exec(line);
        }
    }
}
