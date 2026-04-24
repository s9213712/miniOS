#pragma once

#include <stddef.h>

typedef struct {
    char command[16];
    char arg[112];
} mvos_shell_parse_result_t;

int shell_parse_line(const char *line, mvos_shell_parse_result_t *out);
