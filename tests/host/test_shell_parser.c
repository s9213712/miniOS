#include <mvos/shell_parser.h>
#include <stdio.h>
#include <string.h>

static int expect_parse(const char *line, const char *command, const char *arg) {
    mvos_shell_parse_result_t parsed;
    int rc = shell_parse_line(line, &parsed);
    if (rc != 0) {
        fprintf(stderr, "[test_shell_parser] parse failed for '%s': %d\n", line, rc);
        return 1;
    }
    if (strcmp(parsed.command, command) != 0 || strcmp(parsed.arg, arg) != 0) {
        fprintf(stderr,
                "[test_shell_parser] parse mismatch for '%s': command='%s' arg='%s'\n",
                line,
                parsed.command,
                parsed.arg);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_parse("run", "run", "") != 0) {
        return 1;
    }
    if (expect_parse("run list", "run", "list") != 0) {
        return 1;
    }
    if (expect_parse("  cat  /boot/init/readme.txt  ", "cat", "/boot/init/readme.txt") != 0) {
        return 1;
    }
    if (expect_parse("write /tmp/a hello world", "write", "/tmp/a hello world") != 0) {
        return 1;
    }
    if (expect_parse("echo hello", "echo", "hello") != 0) {
        return 1;
    }

    mvos_shell_parse_result_t parsed;
    if (shell_parse_line("    ", &parsed) != -2) {
        fprintf(stderr, "[test_shell_parser] expected blank input rejection\n");
        return 1;
    }

    printf("[test_shell_parser] PASS\n");
    return 0;
}
