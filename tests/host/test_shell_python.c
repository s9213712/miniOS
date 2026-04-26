#include <mvos/shell_vfs_commands.h>
#include <mvos/vfs.h>
#include <mvos/keyboard.h>
#include <mvos/serial.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEST_OUTPUT_CAP 4096

static char g_console_output[TEST_OUTPUT_CAP];
static size_t g_console_output_len;
static const char *g_keyboard_feed = NULL;
static size_t g_keyboard_feed_pos = 0;
static size_t g_keyboard_feed_len = 0;

static void test_console_reset(void) {
    g_console_output_len = 0;
    g_console_output[0] = '\0';
}

static void test_console_append(const char *text) {
    if (text == NULL) {
        return;
    }
    size_t n = strlen(text);
    if (n == 0) {
        return;
    }
    if (g_console_output_len + n >= TEST_OUTPUT_CAP) {
        n = (g_console_output_len + n < TEST_OUTPUT_CAP) ? n : (TEST_OUTPUT_CAP - g_console_output_len - 1);
    }
    memcpy(g_console_output + g_console_output_len, text, n);
    g_console_output_len += n;
    g_console_output[g_console_output_len] = '\0';
}

static void test_console_append_char(char ch) {
    if (g_console_output_len + 2 >= TEST_OUTPUT_CAP) {
        return;
    }
    g_console_output[g_console_output_len++] = ch;
    g_console_output[g_console_output_len] = '\0';
}

void console_write_string(const char *str) {
    test_console_append(str);
}

void console_write_char(char c) {
    test_console_append_char(c);
}

void console_write_u64(uint64_t value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
    if (n < 0) {
        return;
    }
    test_console_append(buf);
}

void klog(const char *msg) {
    (void)msg;
}

void klogln(const char *msg) {
    (void)msg;
}

void klog_u64(uint64_t value) {
    (void)value;
}

static void test_set_keyboard_feed(const char *feed) {
    g_keyboard_feed = feed;
    g_keyboard_feed_pos = 0;
    g_keyboard_feed_len = (feed == NULL) ? 0 : strlen(feed);
}

int keyboard_read_char_nonblocking(void) {
    if (g_keyboard_feed == NULL || g_keyboard_feed_pos >= g_keyboard_feed_len) {
        return -1;
    }
    return (unsigned char)g_keyboard_feed[g_keyboard_feed_pos++];
}

int serial_read_char_nonblocking(void) {
    return -1;
}

static int test_assert_contains(const char *needle, const char *name) {
    if (strstr(g_console_output, needle) == NULL) {
        fprintf(stderr, "[test_shell_python] missing output marker: %s\n", name);
        fprintf(stderr, "[test_shell_python] output:\n%s\n", g_console_output);
        return 1;
    }
    return 0;
}

static int test_assert_line(const char *line, const char *name) {
    char pattern[128];
    int n = snprintf(pattern, sizeof(pattern), "\n%s\n", line);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        fprintf(stderr, "[test_shell_python] line pattern too long: %s\n", line);
        return 1;
    }
    return test_assert_contains(pattern, name);
}

static int write_script(const char *path, const char *content) {
    return vfs_write_file(path, content, (uint64_t)strlen(content), 0);
}

static int run_python_script(const char *path, const char *input_feed) {
    char command[128];
    snprintf(command, sizeof(command), "python %s", path);
    test_set_keyboard_feed(input_feed);
    test_console_reset();
    shell_cmd_python(command);
    return 0;
}

int main(void) {
    if (write_script(
            "/tmp/mini_demo_ok.py",
            "x = 2\n"
            "y = x * 3 + 4\n"
            "z = (y - 2) / 2\n"
            "print(x)\n"
            "print(y)\n"
            "print(z)\n"
            "print(2 * (x + 1))\n"
            "print(y + x * 3)\n") != 0) {
        fprintf(stderr, "[test_shell_python] failed to write ok script\n");
        return 1;
    }

    test_console_reset();
    shell_cmd_python("python /tmp/mini_demo_ok.py");

    if (test_assert_contains("mini python executing: /tmp/mini_demo_ok.py", "run start")) {
        return 1;
    }
    if (test_assert_line("2", "print x")) {
        return 1;
    }
    if (test_assert_line("10", "print y")) {
        return 1;
    }
    if (test_assert_line("4", "print z")) {
        return 1;
    }
    if (test_assert_line("6", "print precedence-1")) {
        return 1;
    }
    if (test_assert_line("16", "print precedence-2")) {
        return 1;
    }

    if (write_script(
            "/tmp/mini_demo_err.py",
            "x = 2\n"
            "print(10 / (x - 2))\n") != 0) {
        fprintf(stderr, "[test_shell_python] failed to write error script\n");
        return 1;
    }

    test_console_reset();
    shell_cmd_python("python /tmp/mini_demo_err.py");

    if (test_assert_contains("python: completed with 1 error(s)", "error summary")) {
        return 1;
    }
    if (strstr(g_console_output, "line 2") == NULL && strstr(g_console_output, "line 1") == NULL) {
        fprintf(stderr, "[test_shell_python] missing error line number\n");
        fprintf(stderr, "[test_shell_python] output:\n%s\n", g_console_output);
        return 1;
    }
    if (strstr(g_console_output, "syntax error") == NULL) {
        fprintf(stderr, "[test_shell_python] missing syntax error marker\n");
        fprintf(stderr, "[test_shell_python] output:\n%s\n", g_console_output);
        return 1;
    }

    if (write_script(
            "/tmp/1a2b_test.py",
            "print(\"1A2B Python version (miniOS mini-python)\")\n"
            "play_1a2b()\n"
            "print(\"Python game done\")\n") != 0) {
        fprintf(stderr, "[test_shell_python] failed to write 1A2B script\n");
        return 1;
    }

    run_python_script("/tmp/1a2b_test.py", "1234\n");
    if (test_assert_line("1A2B Game (Python version)", "1a2b title")) {
        return 1;
    }
    if (test_assert_line("4A0B", "1a2b win result")) {
        return 1;
    }
    if (test_assert_line("You win! secret=1234", "1a2b win text")) {
        return 1;
    }
    if (strstr(g_console_output, "Python game done") == NULL) {
        fprintf(stderr, "[test_shell_python] missing python script footer\n");
        fprintf(stderr, "[test_shell_python] output:\n%s\n", g_console_output);
        return 1;
    }

    run_python_script("/tmp/1a2b_test.py", "q\n");
    if (test_assert_line("game quit.", "1a2b quit text")) {
        return 1;
    }
    if (strstr(g_console_output, "Python game done") == NULL) {
        fprintf(stderr, "[test_shell_python] missing python script footer after quit\n");
        fprintf(stderr, "[test_shell_python] output:\n%s\n", g_console_output);
        return 1;
    }

    printf("[test_shell_python] PASS\n");
    return 0;
}
