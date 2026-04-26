#include <mvos/shell_vfs_commands.h>
#include <mvos/console.h>
#include <mvos/vfs.h>
#include <mvos/keyboard.h>
#include <mvos/serial.h>
#include <mvos/shell_py_runtime.h>
#include <stdint.h>
#include <stddef.h>

#define MINI_NANO_MAX_BYTES 512
#define MINI_NANO_MAX_LINES 64
#define MINI_NANO_MAX_LINE_LEN 96

typedef struct {
    char path[64];
    char lines[MINI_NANO_MAX_LINES][MINI_NANO_MAX_LINE_LEN];
    uint32_t line_count;
    uint32_t current_line;
    uint32_t current_col;
    uint32_t file_exists;
    uint32_t dirty;
} shell_nano_state_t;

static size_t shell_vfs_strlen_n(const char *s) {
    size_t len = 0;
    while (s != NULL && s[len] != '\0') {
        ++len;
    }
    return len;
}

static size_t shell_vfs_strlen(const char *s) {
    size_t len = 0;
    while (s != NULL && s[len] != '\0') {
        ++len;
    }
    return len;
}

static const char *shell_vfs_skip_spaces(const char *s) {
    if (s == NULL) {
        return "";
    }
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static int shell_vfs_parse_path_only(const char *arg, char *path_out, size_t path_cap) {
    const char *p = shell_vfs_skip_spaces(arg);
    if (*p == '\0' || path_cap < 2) {
        return -1;
    }
    size_t i = 0;
    while (p[i] != '\0' && p[i] != ' ') {
        if (i + 1 >= path_cap) {
            return -1;
        }
        path_out[i] = p[i];
        ++i;
    }
    path_out[i] = '\0';
    return 0;
}

static int shell_vfs_parse_path_payload(const char *arg,
                                        char *path_out,
                                        size_t path_cap,
                                        const char **payload_out) {
    const char *p = shell_vfs_skip_spaces(arg);
    if (*p == '\0' || path_cap < 2 || payload_out == NULL) {
        return -1;
    }
    size_t i = 0;
    while (p[i] != '\0' && p[i] != ' ') {
        if (i + 1 >= path_cap) {
            return -1;
        }
        path_out[i] = p[i];
        ++i;
    }
    path_out[i] = '\0';
    p += i;
    p = shell_vfs_skip_spaces(p);
    *payload_out = p;
    return 0;
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

void shell_cmd_ls(const char *arg) {
    arg = shell_vfs_skip_spaces(arg);
    const char *prefix = (*arg == '\0') ? "/" : arg;
    uint64_t listed = vfs_list(shell_vfs_list_visitor, prefix, NULL);
    console_write_string("[vfs] listed=");
    console_write_u64(listed);
    console_write_string(" entries for prefix \"");
    console_write_string(prefix);
    console_write_string("\"\n");
}

void shell_cmd_cat(const char *arg) {
    arg = shell_vfs_skip_spaces(arg);
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

static void shell_vfs_print_write_error(int rc) {
    if (rc == -2) {
        console_write_string("vfs write rejected: writable paths are limited to /tmp/*\n");
        return;
    }
    if (rc == -3) {
        console_write_string("vfs write failed: no free /tmp slots\n");
        return;
    }
    if (rc == -4) {
        console_write_string("vfs write failed: file too large (max 512 bytes)\n");
        return;
    }
    console_write_string("vfs write failed\n");
}

void shell_cmd_write_file(const char *arg, int append) {
    char path[64];
    const char *payload = NULL;
    if (shell_vfs_parse_path_payload(arg, path, sizeof(path), &payload) != 0 || payload[0] == '\0') {
        if (append) {
            console_write_string("append usage: append <path> <text>\n");
        } else {
            console_write_string("write usage: write <path> <text>\n");
        }
        console_write_string("example: write /tmp/note.txt hello\n");
        return;
    }

    int rc = vfs_write_file(path, payload, (uint64_t)shell_vfs_strlen(payload), append);
    if (rc != 0) {
        shell_vfs_print_write_error(rc);
        return;
    }

    console_write_string(append ? "vfs append ok: " : "vfs write ok: ");
    console_write_string(path);
    console_write_string("\n");
}

void shell_cmd_touch(const char *arg) {
    char path[64];
    if (shell_vfs_parse_path_only(arg, path, sizeof(path)) != 0) {
        console_write_string("touch usage: touch <path>\n");
        console_write_string("example: touch /tmp/empty.txt\n");
        return;
    }

    int rc = vfs_write_file(path, NULL, 0, 0);
    if (rc != 0) {
        shell_vfs_print_write_error(rc);
        return;
    }
    console_write_string("vfs touch ok: ");
    console_write_string(path);
    console_write_string("\n");
}

void shell_cmd_rm(const char *arg) {
    char path[64];
    if (shell_vfs_parse_path_only(arg, path, sizeof(path)) != 0) {
        console_write_string("rm usage: rm <path>\n");
        console_write_string("example: rm /tmp/note.txt\n");
        return;
    }

    int rc = vfs_remove_file(path);
    if (rc == 0) {
        console_write_string("vfs remove ok: ");
        console_write_string(path);
        console_write_string("\n");
        return;
    }
    if (rc == -3) {
        console_write_string("vfs remove rejected: readonly path\n");
        return;
    }
    if (rc == -2) {
        console_write_string("vfs remove failed: not found\n");
        return;
    }
    console_write_string("vfs remove failed\n");
}

static int shell_nano_read_key(void) {
    for (;;) {
        int c = keyboard_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        c = serial_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        __asm__ volatile("pause");
    }
}

static int shell_nano_is_printable(int c) {
    return c >= 32 && c < 127;
}

static void shell_nano_clear_screen(void) {
    console_write_string("\x1b[2J\x1b[H");
}

static void shell_nano_redraw(const shell_nano_state_t *s, const char *status) {
    shell_nano_clear_screen();
    console_write_string("miniOS nano-like editor: ");
    console_write_string(s->path);
    console_write_string(" | ");
    console_write_string(status);
    console_write_string(" | ");
    console_write_string(s->dirty ? "modified" : "clean");
    console_write_string("\n");
    console_write_string("Ctrl+S: save, Ctrl+Q: quit, arrows: move, Enter: split, Backspace: delete\n\n");

    for (uint32_t line = 0; line < s->line_count; ++line) {
        if (line == s->current_line) {
            console_write_string("> ");
        } else {
            console_write_string("  ");
        }
        console_write_u64((uint64_t)(line + 1));
        console_write_string(" ");

        const char *text = s->lines[line];
        if (text[0] == '\0') {
            console_write_string("~\n");
            continue;
        }
        console_write_string(text);
        console_write_string("\n");
    }
}

static void shell_nano_init_from_string(shell_nano_state_t *s, const char *content) {
    for (uint32_t i = 0; i < MINI_NANO_MAX_LINES; ++i) {
        s->lines[i][0] = '\0';
    }
    s->line_count = 1;
    s->current_line = 0;
    s->current_col = 0;
    s->dirty = 0;

    if (content == NULL || content[0] == '\0') {
        return;
    }

    uint32_t line = 0;
    size_t pos = 0;
    for (size_t i = 0; content[i] != '\0' && line < MINI_NANO_MAX_LINES; ++i) {
        char ch = content[i];
        if (ch == '\n') {
            if (line + 1 < MINI_NANO_MAX_LINES) {
                if (s->lines[line][0] == '\0' && pos == 0) {
                    s->lines[line][0] = '\0';
                } else {
                    s->lines[line][pos] = '\0';
                }
                ++line;
                pos = 0;
                continue;
            }
            break;
        }
        if (pos >= MINI_NANO_MAX_LINE_LEN - 1) {
            break;
        }
        s->lines[line][pos++] = ch;
    }
    s->lines[line][pos] = '\0';
    s->line_count = line + 1;
}

static size_t shell_nano_line_len(const shell_nano_state_t *s, uint32_t line) {
    return shell_vfs_strlen_n(s->lines[line]);
}

static size_t shell_nano_text_bytes(const shell_nano_state_t *s) {
    size_t bytes = 0;
    for (uint32_t i = 0; i < s->line_count; ++i) {
        bytes += shell_nano_line_len(s, i);
        if (i + 1 < s->line_count) {
            ++bytes;
        }
    }
    return bytes;
}

static void shell_nano_insert_char(shell_nano_state_t *s, char ch) {
    if (s->current_line >= s->line_count) {
        return;
    }
    uint32_t line = s->current_line;
    size_t len = shell_nano_line_len(s, line);
    if (shell_nano_text_bytes(s) >= MINI_NANO_MAX_BYTES - 1) {
        return;
    }
    if (s->current_col > len) {
        s->current_col = (uint32_t)len;
    }
    if (len + 1 >= MINI_NANO_MAX_LINE_LEN) {
        return;
    }
    for (size_t i = len + 1; i > s->current_col; --i) {
        s->lines[line][i] = s->lines[line][i - 1];
    }
    s->lines[line][s->current_col] = ch;
    ++s->current_col;
    s->dirty = 1;
}

static void shell_nano_backspace(shell_nano_state_t *s) {
    uint32_t line = s->current_line;
    if (line >= s->line_count) {
        return;
    }
    size_t len = shell_nano_line_len(s, line);
    if (s->current_col > len) {
        s->current_col = (uint32_t)len;
    }
    if (s->current_col > 0) {
        for (size_t i = s->current_col - 1; i < len; ++i) {
            s->lines[line][i] = s->lines[line][i + 1];
        }
        if (s->current_col > 0) {
            --s->current_col;
        }
        s->dirty = 1;
        return;
    }
    if (line == 0) {
        return;
    }
    if (s->line_count <= 1) {
        return;
    }
    size_t prev_len = shell_nano_line_len(s, line - 1);
    if (prev_len >= MINI_NANO_MAX_LINE_LEN - 1) {
        return;
    }
    if (prev_len + len >= MINI_NANO_MAX_LINE_LEN - 1) {
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        s->lines[line - 1][prev_len + i] = s->lines[line][i];
    }
    s->lines[line - 1][prev_len + len] = '\0';
    for (uint32_t i = line; i + 1 < s->line_count; ++i) {
        for (uint32_t j = 0; j < MINI_NANO_MAX_LINE_LEN; ++j) {
            s->lines[i][j] = s->lines[i + 1][j];
        }
    }
    if (s->line_count > 0) {
        --s->line_count;
    }
    s->current_line = line - 1;
    s->current_col = (uint32_t)prev_len;
    s->dirty = 1;
}

static void shell_nano_enter(shell_nano_state_t *s) {
    if (s->line_count >= MINI_NANO_MAX_LINES) {
        return;
    }
    uint32_t line = s->current_line;
    if (line >= s->line_count) {
        return;
    }
    size_t len = shell_nano_line_len(s, line);
    if (s->current_col > len) {
        s->current_col = (uint32_t)len;
    }
    for (uint32_t i = s->line_count; i > line + 1; --i) {
        for (uint32_t j = 0; j < MINI_NANO_MAX_LINE_LEN; ++j) {
            s->lines[i][j] = s->lines[i - 1][j];
        }
    }
    ++s->line_count;
    for (uint32_t i = 0; i < MINI_NANO_MAX_LINE_LEN; ++i) {
        s->lines[line + 1][i] = '\0';
    }
    for (size_t i = s->current_col; i < len; ++i) {
        s->lines[line + 1][i - s->current_col] = s->lines[line][i];
    }
    s->lines[line + 1][len - s->current_col] = '\0';
    s->lines[line][s->current_col] = '\0';
    s->current_line = line + 1;
    s->current_col = 0;
    s->dirty = 1;
}

static void shell_nano_move_left(shell_nano_state_t *s) {
    if (s->current_col > 0) {
        --s->current_col;
        return;
    }
    if (s->current_line == 0) {
        return;
    }
    --s->current_line;
    s->current_col = (uint32_t)shell_nano_line_len(s, s->current_line);
}

static void shell_nano_move_right(shell_nano_state_t *s) {
    uint32_t line = s->current_line;
    size_t len = shell_nano_line_len(s, line);
    if (s->current_col < len) {
        ++s->current_col;
        return;
    }
    if (line + 1 >= s->line_count) {
        return;
    }
    ++s->current_line;
    s->current_col = 0;
}

static void shell_nano_move_up(shell_nano_state_t *s) {
    if (s->current_line == 0) {
        return;
    }
    --s->current_line;
    size_t len = shell_nano_line_len(s, s->current_line);
    if (s->current_col > len) {
        s->current_col = (uint32_t)len;
    }
}

static void shell_nano_move_down(shell_nano_state_t *s) {
    if (s->current_line + 1 >= s->line_count) {
        return;
    }
    ++s->current_line;
    size_t len = shell_nano_line_len(s, s->current_line);
    if (s->current_col > len) {
        s->current_col = (uint32_t)len;
    }
}

static void shell_nano_write_file(shell_nano_state_t *s) {
    char out[MINI_NANO_MAX_BYTES + 1];
    size_t out_len = 0;
    for (uint32_t i = 0; i < s->line_count; ++i) {
        size_t len = shell_nano_line_len(s, i);
        for (size_t j = 0; j < len; ++j) {
            if (out_len < sizeof(out) - 1) {
                out[out_len++] = s->lines[i][j];
            }
        }
        if (i + 1 < s->line_count && out_len < sizeof(out) - 1) {
            out[out_len++] = '\n';
        }
    }
    if (s->line_count == 0) {
        out_len = 0;
    }
    if (out_len >= sizeof(out)) {
        out_len = sizeof(out) - 1;
    }
    out[out_len] = '\0';

    (void)vfs_write_file(s->path, NULL, 0, 0);
    int rc = vfs_write_file(s->path, out, (uint64_t)out_len, 0);
    if (rc != 0) {
        s->dirty = 1;
        shell_vfs_print_write_error(rc);
        return;
    }
    s->dirty = 0;
    console_write_string("nano: saved\n");
}

static void shell_nano_load_content(shell_nano_state_t *s, const char *path) {
    mvos_vfs_file_t file;
    s->file_exists = 0;
    if (vfs_open(path, &file) != 0) {
        shell_nano_init_from_string(s, "");
        return;
    }

    s->file_exists = 1;
    char raw[MINI_NANO_MAX_BYTES + 1];
    size_t total = 0;
    for (;;) {
        uint64_t read_bytes = 0;
        int status = vfs_read(&file, raw + total, MINI_NANO_MAX_BYTES - total, &read_bytes);
        if (status != 0 || read_bytes == 0 || total >= MINI_NANO_MAX_BYTES) {
            break;
        }
        total += (size_t)read_bytes;
        if (total >= MINI_NANO_MAX_BYTES) {
            break;
        }
    }
    raw[total] = '\0';
    shell_nano_init_from_string(s, raw);
    vfs_close(&file);
}

void shell_cmd_nano(const char *arg) {
    char path[64];
    if (shell_vfs_parse_path_only(arg, path, sizeof(path)) != 0) {
        console_write_string("nano usage: nano <path>\n");
        console_write_string("example: nano /tmp/note.txt\n");
        return;
    }

    shell_nano_state_t state = {
        .line_count = 1,
        .current_line = 0,
        .current_col = 0,
        .file_exists = 0,
        .dirty = 0,
    };
    for (size_t i = 0; i < sizeof(state.path); ++i) {
        state.path[i] = '\0';
    }
    for (size_t i = 0; i < sizeof(path) && i < sizeof(state.path); ++i) {
        state.path[i] = path[i];
        if (path[i] == '\0') {
            break;
        }
    }

    shell_nano_load_content(&state, path);

    shell_nano_redraw(&state, state.file_exists ? "loaded" : "new");
    while (1) {
        if (state.current_line >= state.line_count) {
            state.current_line = state.line_count ? state.line_count - 1 : 0;
        }
        int ch = shell_nano_read_key();
        if (ch == 0x11) {
            if (!state.dirty) {
                console_write_string("nano: quit\n");
                return;
            }
            shell_nano_redraw(&state, "modified; Ctrl+Q again to discard");
            int confirm = shell_nano_read_key();
            if (confirm == 0x11) {
                console_write_string("nano: quit (discard)\n");
                return;
            }
            if (confirm == 0x13) {
                shell_nano_write_file(&state);
                if (!state.dirty) {
                    return;
                }
            }
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == 0x13) {
            shell_nano_write_file(&state);
            shell_nano_redraw(&state, "saved");
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            shell_nano_enter(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == '\b' || ch == 0x7f) {
            shell_nano_backspace(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == KEY_LEFT) {
            shell_nano_move_left(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == KEY_RIGHT) {
            shell_nano_move_right(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == KEY_UP) {
            shell_nano_move_up(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (ch == KEY_DOWN) {
            shell_nano_move_down(&state);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        if (shell_nano_is_printable(ch)) {
            shell_nano_insert_char(&state, (char)ch);
            shell_nano_redraw(&state, "edit");
            continue;
        }
        shell_nano_redraw(&state, "edit (unsupported key)");
    }
}

void shell_cmd_python(const char *arg) {
    shell_py_exec(arg);
}
