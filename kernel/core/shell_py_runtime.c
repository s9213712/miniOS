#include <mvos/shell_py_runtime.h>
#include <mvos/console.h>
#include <mvos/vfs.h>
#include <mvos/keyboard.h>
#include <mvos/interrupt.h>
#include <mvos/serial.h>
#include <stdint.h>
#include <stddef.h>

#define MINI_PY_MAX_VARS 16
#define MINI_PY_MAX_NAME 16
#define MINI_PY_MAX_SCRIPT 1024

typedef struct {
    char name[MINI_PY_MAX_NAME];
    int64_t value;
    uint32_t used;
} shell_py_var_t;

static const char *shell_py_skip_ws(const char *s) {
    while (s != NULL && (*s == ' ' || *s == '\t')) {
        ++s;
    }
    return s == NULL ? "" : s;
}

static int shell_py_streq(const char *lhs, const char *rhs) {
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    size_t i = 0;
    while (lhs[i] != '\0' && rhs[i] != '\0' && lhs[i] == rhs[i]) {
        ++i;
    }
    return lhs[i] == '\0' && rhs[i] == '\0';
}

static int shell_py_copy_token(const char **cursor, char *out, size_t out_cap) {
    const char *p = shell_py_skip_ws(*cursor);
    if (p == NULL || *p == '\0' || out_cap < 2) {
        return -1;
    }
    size_t n = 0;
    while (p[n] != '\0' && p[n] != ' ' && p[n] != '\t' && p[n] != '\r' && p[n] != '\n') {
        if (n + 1 >= out_cap) {
            return -1;
        }
        out[n] = p[n];
        ++n;
    }
    out[n] = '\0';
    *cursor = p + n;
    return 0;
}

static int shell_py_is_name_start(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static int shell_py_is_name_char(char c) {
    return shell_py_is_name_start(c) || (c >= '0' && c <= '9');
}

static void shell_py_write_int(int64_t value);

static int shell_py_parse_identifier(const char **cursor, char *out, size_t out_cap) {
    const char *p = shell_py_skip_ws(*cursor);
    if (p == NULL || !shell_py_is_name_start(*p) || out_cap < 2) {
        return -1;
    }
    size_t n = 0;
    while (shell_py_is_name_char(p[n])) {
        if (n + 1 >= out_cap) {
            return -1;
        }
        out[n] = p[n];
        ++n;
    }
    if (n == 0) {
        return -1;
    }
    out[n] = '\0';
    *cursor = p + n;
    return 0;
}

static int shell_py_read_char_nonblocking(void) {
    int c = keyboard_read_char_nonblocking();
    if (c >= 0) {
        return c;
    }
    return serial_read_char_nonblocking();
}

static int shell_py_read_char(void) {
    int c = -1;
    while (c < 0) {
        c = shell_py_read_char_nonblocking();
        if (c < 0) {
            mvos_idle_wait();
        }
    }
    return c;
}

static int shell_py_read_line(char *buffer, size_t cap, const char *prompt) {
    if (buffer == NULL || cap == 0) {
        return -1;
    }
    if (prompt != NULL) {
        console_write_string(prompt);
    }
    size_t len = 0;
    buffer[0] = '\0';
    while (1) {
        int c = shell_py_read_char();
        if (c == '\r' || c == '\n') {
            console_write_char('\n');
            buffer[len] = '\0';
            return 0;
        }
        if (c == '\b' || c == 0x7f) {
            if (len == 0) {
                continue;
            }
            --len;
            buffer[len] = '\0';
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
        buffer[len++] = (char)c;
        buffer[len] = '\0';
        console_write_char((char)c);
    }
}

static void shell_py_1a2b_print_result(uint64_t a, uint64_t b) {
    shell_py_write_int((int64_t)a);
    console_write_string("A");
    shell_py_write_int((int64_t)b);
    console_write_string("B\n");
}

static int shell_py_1a2b_parse_guess(const char *line, int guess[4]) {
    int seen[10] = {0};
    if (line == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (line[i] == '\0') {
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

static void shell_py_1a2b_eval_guess(const int secret[4], const int guess[4], uint64_t *a_out, uint64_t *b_out) {
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

static void shell_py_cmd_1a2b(void) {
    const int secret[4] = {1, 2, 3, 4};
    char line[16];
    int guess[4];
    int attempts = 0;
    int solved = 0;

    console_write_string("1A2B Game (Python version)\n");
    console_write_string("Secret: 4 distinct digits (0-9), max 8 tries.\n");
    console_write_string("Input 'q' to quit.\n");

    while (attempts < 8 && !solved) {
        ++attempts;
        if (shell_py_read_line(line, sizeof(line), "guess> ") != 0) {
            return;
        }
        if (line[0] == 'q' && line[1] == '\0') {
            console_write_string("game quit.\n");
            return;
        }
        if (shell_py_1a2b_parse_guess(line, guess) != 0) {
            console_write_string("invalid input, please input 4 distinct digits.\n");
            --attempts;
            continue;
        }
        uint64_t a = 0;
        uint64_t b = 0;
        shell_py_1a2b_eval_guess(secret, guess, &a, &b);
        shell_py_1a2b_print_result(a, b);
        if (a == 4) {
            solved = 1;
            console_write_string("You win! secret=1234\n");
            return;
        }
    }

    if (!solved) {
        console_write_string("Game over. answer=1234\n");
    }
}

static int shell_py_get_var(const shell_py_var_t *vars, const char *name, int64_t *value) {
    for (size_t i = 0; i < MINI_PY_MAX_VARS; ++i) {
        if (!vars[i].used) {
            continue;
        }
        size_t n = 0;
        while (vars[i].name[n] != '\0' && name[n] != '\0' && vars[i].name[n] == name[n]) {
            ++n;
        }
        if (vars[i].name[n] == '\0' && name[n] == '\0') {
            if (value != NULL) {
                *value = vars[i].value;
            }
            return 0;
        }
    }
    return -1;
}

static int shell_py_set_var(shell_py_var_t *vars, const char *name, int64_t value) {
    for (size_t i = 0; i < MINI_PY_MAX_VARS; ++i) {
        if (vars[i].used) {
            size_t n = 0;
            while (vars[i].name[n] != '\0' && name[n] != '\0' && vars[i].name[n] == name[n]) {
                ++n;
            }
            if (vars[i].name[n] == '\0' && name[n] == '\0') {
                vars[i].value = value;
                return 0;
            }
            continue;
        }
        size_t n = 0;
        while (name[n] != '\0' && n + 1 < sizeof(vars[i].name)) {
            vars[i].name[n] = name[n];
            ++n;
        }
        if (name[n] != '\0') {
            return -1;
        }
        vars[i].name[n] = '\0';
        vars[i].value = value;
        vars[i].used = 1;
        return 0;
    }
    return -1;
}

static void shell_py_write_u64(uint64_t value) {
    char digits[32];
    size_t n = 0;
    if (value == 0) {
        console_write_char('0');
        return;
    }
    while (value != 0 && n < sizeof(digits)) {
        digits[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0) {
        --n;
        console_write_char(digits[n]);
    }
}

static void shell_py_write_int(int64_t value) {
    if (value < 0) {
        console_write_char('-');
        uint64_t u = (uint64_t)(-(value + 1)) + 1;
        shell_py_write_u64(u);
        return;
    }
    shell_py_write_u64((uint64_t)value);
}

static int shell_py_eval_expr(const char **cursor, const shell_py_var_t *vars, int64_t *out);

static int shell_py_eval_atom(const char **cursor, const shell_py_var_t *vars, int64_t *out) {
    const char *p = shell_py_skip_ws(*cursor);
    if (p == NULL || *p == '\0') {
        return -1;
    }
    if (*p == '(') {
        ++p;
        if (shell_py_eval_expr(&p, vars, out) != 0) {
            return -1;
        }
        p = shell_py_skip_ws(p);
        if (*p != ')') {
            return -1;
        }
        ++p;
        *cursor = p;
        return 0;
    }
    if (shell_py_is_name_start(*p)) {
        char ident[MINI_PY_MAX_NAME];
        if (shell_py_parse_identifier(&p, ident, sizeof(ident)) != 0) {
            return -1;
        }
        if (shell_py_get_var(vars, ident, out) != 0) {
            return -1;
        }
        *cursor = p;
        return 0;
    }
    int neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
        p = shell_py_skip_ws(p);
    }
    if (*p < '0' || *p > '9') {
        return -1;
    }
    uint64_t num = 0;
    while (*p >= '0' && *p <= '9') {
        num = num * 10u + (uint64_t)(*p - '0');
        ++p;
    }
    *cursor = p;
    *out = neg ? -(int64_t)num : (int64_t)num;
    return 0;
}

static int shell_py_eval_factor(const char **cursor, const shell_py_var_t *vars, int64_t *out) {
    const char *p = shell_py_skip_ws(*cursor);
    int sign = 1;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        ++p;
        p = shell_py_skip_ws(p);
    }
    int64_t value = 0;
    if (shell_py_eval_atom(&p, vars, &value) != 0) {
        return -1;
    }
    value *= sign;

    while (1) {
        p = shell_py_skip_ws(p);
        if (*p != '*' && *p != '/') {
            break;
        }
        char op = *p;
        ++p;
        int64_t rhs = 0;
        int64_t rhs_sign = 1;
        p = shell_py_skip_ws(p);
        if (*p == '+' || *p == '-') {
            rhs_sign = (*p == '-') ? -1 : 1;
            ++p;
            p = shell_py_skip_ws(p);
        }
        if (shell_py_eval_atom(&p, vars, &rhs) != 0) {
            return -1;
        }
        rhs *= rhs_sign;
        if (op == '*') {
            value *= rhs;
        } else {
            if (rhs == 0) {
                return -1;
            }
            value /= rhs;
        }
    }

    *cursor = p;
    *out = value;
    return 0;
}

static int shell_py_eval_expr(const char **cursor, const shell_py_var_t *vars, int64_t *out) {
    const char *p = shell_py_skip_ws(*cursor);
    int64_t value = 0;
    if (shell_py_eval_factor(&p, vars, &value) != 0) {
        return -1;
    }
    while (1) {
        p = shell_py_skip_ws(p);
        if (*p != '+' && *p != '-') {
            break;
        }
        char op = *p;
        ++p;
        int64_t rhs = 0;
        if (shell_py_eval_factor(&p, vars, &rhs) != 0) {
            return -1;
        }
        if (op == '+') {
            value += rhs;
        } else {
            value -= rhs;
        }
        p = shell_py_skip_ws(p);
    }
    *cursor = p;
    *out = value;
    return 0;
}

static void shell_py_report_error(const char *path, uint32_t line_no, const char *msg) {
    console_write_string("python ");
    console_write_string(path);
    console_write_string(":line ");
    shell_py_write_int((int64_t)line_no);
    console_write_string(": ");
    console_write_string(msg);
    console_write_string("\n");
}

static int shell_py_run_line(const char *path, uint32_t line_no, const char *line, shell_py_var_t *vars, uint64_t *error_count) {
    const char *p = shell_py_skip_ws(line);
    if (p == NULL || p[0] == '\0' || p[0] == '#') {
        return 0;
    }

    char token[MINI_PY_MAX_NAME];
    if (shell_py_parse_identifier(&p, token, sizeof(token)) != 0) {
        shell_py_report_error(path, line_no, "syntax error: invalid statement");
        ++(*error_count);
        return -1;
    }

    p = shell_py_skip_ws(p);
    if (shell_py_streq(token, "play_1a2b")) {
        if (*p != '(') {
            shell_py_report_error(path, line_no, "syntax error: play_1a2b() expected");
            ++(*error_count);
            return -1;
        }
        ++p;
        p = shell_py_skip_ws(p);
        if (*p != ')') {
            shell_py_report_error(path, line_no, "syntax error: play_1a2b() expected");
            ++(*error_count);
            return -1;
        }
        ++p;
        p = shell_py_skip_ws(p);
        if (*p != '\0' && *p != '#') {
            shell_py_report_error(path, line_no, "syntax error: play_1a2b() expected");
            ++(*error_count);
            return -1;
        }
        shell_py_cmd_1a2b();
        return 0;
    }
    if (shell_py_streq(token, "print")) {
        if (*p != '(') {
            shell_py_report_error(path, line_no, "syntax error: print() expected");
            ++(*error_count);
            return -1;
        }
        ++p;
        p = shell_py_skip_ws(p);
        if (*p == '\"' || *p == '\'') {
            char quote = *p;
            ++p;
            while (*p != '\0' && *p != quote) {
                console_write_char(*p);
                ++p;
            }
            if (*p != quote) {
                shell_py_report_error(path, line_no, "syntax error: unterminated string");
                ++(*error_count);
                return -1;
            }
            ++p;
            p = shell_py_skip_ws(p);
            if (*p != ')') {
                shell_py_report_error(path, line_no, "syntax error: expected ')'");
                ++(*error_count);
                return -1;
            }
        } else {
            int64_t value = 0;
            if (shell_py_eval_expr(&p, vars, &value) != 0) {
                shell_py_report_error(path, line_no, "syntax error: invalid expression");
                ++(*error_count);
                return -1;
            }
            shell_py_write_int(value);
            p = shell_py_skip_ws(p);
            if (*p != ')') {
                shell_py_report_error(path, line_no, "syntax error: expected ')'");
                ++(*error_count);
                return -1;
            }
        }
        ++p;
        p = shell_py_skip_ws(p);
        if (*p != '\0' && *p != '#') {
            shell_py_report_error(path, line_no, "syntax error: trailing content");
            ++(*error_count);
            return -1;
        }
        console_write_char('\n');
        return 0;
    }

    if (*p != '=') {
        shell_py_report_error(path, line_no, "syntax error: assignment or print required");
        ++(*error_count);
        return -1;
    }
    ++p;
    int64_t value = 0;
    if (shell_py_eval_expr(&p, vars, &value) != 0) {
        shell_py_report_error(path, line_no, "syntax error: invalid expression");
        ++(*error_count);
        return -1;
    }
    p = shell_py_skip_ws(p);
    if (*p != '\0' && *p != '#') {
        shell_py_report_error(path, line_no, "syntax error: trailing content");
        ++(*error_count);
        return -1;
    }
    (void)shell_py_set_var(vars, token, value);
    return 0;
}

void shell_py_exec(const char *arg) {
    char cmd[16];
    const char *p = shell_py_skip_ws(arg);
    if (shell_py_copy_token(&p, cmd, sizeof(cmd)) != 0 || !shell_py_streq(cmd, "python")) {
        console_write_string("run python usage:\n");
        console_write_string("  run python <path>\n");
        console_write_string("  run python /tmp/foo.py\n");
        return;
    }

    char path[64];
    p = shell_py_skip_ws(p);
    if (shell_py_copy_token(&p, path, sizeof(path)) != 0) {
        console_write_string("run python: missing script path\n");
        return;
    }

    mvos_vfs_file_t file;
    if (vfs_open(path, &file) != 0) {
        console_write_string("python: file not found: ");
        console_write_string(path);
        console_write_string("\n");
        return;
    }

    char program[MINI_PY_MAX_SCRIPT + 1];
    size_t total = 0;
    for (;;) {
        uint64_t read_bytes = 0;
        int status = vfs_read(&file, program + total, (uint64_t)(MINI_PY_MAX_SCRIPT - total), &read_bytes);
        if (status != 0 || read_bytes == 0 || total >= MINI_PY_MAX_SCRIPT) {
            break;
        }
        total += (size_t)read_bytes;
        if (total >= MINI_PY_MAX_SCRIPT) {
            break;
        }
    }
    vfs_close(&file);
    if (total > MINI_PY_MAX_SCRIPT) {
        total = MINI_PY_MAX_SCRIPT;
    }
    program[total] = '\0';

    console_write_string("mini python executing: ");
    console_write_string(path);
    console_write_string("\n");

    shell_py_var_t vars[MINI_PY_MAX_VARS] = {0};
    uint64_t errors = 0;
    uint32_t line_no = 1;
    size_t start = 0;
    for (size_t i = 0; i <= total; ++i) {
        if (program[i] != '\n' && program[i] != '\r' && program[i] != '\0') {
            continue;
        }
        program[i] = '\0';
        (void)shell_py_run_line(path, line_no, program + start, vars, &errors);
        if (program[i] == '\n') {
            ++line_no;
        } else if (program[i] == '\r' && program[i + 1] == '\n') {
            ++line_no;
            ++i;
        }
        start = i + 1;
    }
    if (errors != 0) {
        console_write_string("python: completed with ");
        shell_py_write_u64(errors);
        console_write_string(" error(s)\n");
    }
}
