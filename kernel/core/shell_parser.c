#include <mvos/shell_parser.h>

static size_t parser_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        ++len;
    }
    return len;
}

int shell_parse_line(const char *line, mvos_shell_parse_result_t *out) {
    if (line == 0 || out == 0) {
        return -1;
    }

    out->command[0] = '\0';
    out->arg[0] = '\0';

    size_t start = 0;
    size_t end = parser_strlen(line);
    while (start < end && line[start] == ' ') {
        ++start;
    }
    while (end > start && line[end - 1] == ' ') {
        --end;
    }
    if (end <= start) {
        return -2;
    }

    size_t pos = start;
    size_t cmd_len = 0;
    while (pos < end && line[pos] != ' ') {
        if (cmd_len + 1 >= sizeof(out->command)) {
            return -3;
        }
        out->command[cmd_len++] = line[pos++];
    }
    out->command[cmd_len] = '\0';

    while (pos < end && line[pos] == ' ') {
        ++pos;
    }

    size_t arg_len = 0;
    while (pos < end) {
        if (arg_len + 1 >= sizeof(out->arg)) {
            return -4;
        }
        out->arg[arg_len++] = line[pos++];
    }
    out->arg[arg_len] = '\0';
    return 0;
}
