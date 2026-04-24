#include <mvos/shell_vfs_commands.h>
#include <mvos/console.h>
#include <mvos/vfs.h>
#include <stdint.h>
#include <stddef.h>

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
    const char *prefix = (*arg == '\0') ? "/boot/init" : arg;
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
