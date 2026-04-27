#include <stdint.h>
#include <stdio.h>
#include <string.h>

int printf_min(const char *fmt, ...);

static char g_output[512];
static size_t g_output_len;

void serial_init(void) {
}

void serial_write_char(char c) {
    if (g_output_len + 1 < sizeof(g_output)) {
        g_output[g_output_len++] = c;
        g_output[g_output_len] = '\0';
    }
}

void serial_write_string(const char *str) {
    while (str && *str) {
        serial_write_char(*str++);
    }
}

void serial_write_u64(uint64_t value) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
    if (len > 0) {
        serial_write_string(buf);
    }
}

int serial_read_char_nonblocking(void) {
    return -1;
}

int serial_read_char(void) {
    return -1;
}

static void reset_output(void) {
    g_output[0] = '\0';
    g_output_len = 0;
}

static int expect_output(const char *expect, int written, const char *label) {
    if (strcmp(g_output, expect) != 0) {
        fprintf(stderr, "[test_printf_min] %s output mismatch: got='%s' expect='%s'\n",
                label, g_output, expect);
        return 1;
    }
    if ((int)strlen(expect) != written) {
        fprintf(stderr, "[test_printf_min] %s count mismatch: got=%d expect=%zu\n",
                label, written, strlen(expect));
        return 1;
    }
    return 0;
}

int main(void) {
    int written;

    reset_output();
    written = printf_min("hello %s %d %u %x %c %%", "world", -42, 42u, 0x2au, '!');
    if (expect_output("hello world -42 42 2a ! %", written, "mixed-format") != 0) {
        return 1;
    }

    reset_output();
    written = printf_min("%s", (const char *)0);
    if (expect_output("(null)", written, "null-string") != 0) {
        return 1;
    }

    reset_output();
    written = printf_min("%d %u %x", -2147483647 - 1, 0u, 0u);
    if (expect_output("-2147483648 0 0", written, "integer-edges") != 0) {
        return 1;
    }

    reset_output();
    written = printf_min("tail-percent %");
    if (expect_output("tail-percent ", written, "trailing-percent") != 0) {
        return 1;
    }

    printf("[test_printf_min] PASS\n");
    return 0;
}
