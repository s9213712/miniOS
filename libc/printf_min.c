#include <mvos/serial.h>
#include <stdarg.h>
#include <stdint.h>

static int emit_char(char c) {
    serial_write_char(c);
    return 1;
}

static int emit_string(const char *s) {
    int written = 0;
    if (s == 0) {
        s = "(null)";
    }
    while (*s != '\0') {
        serial_write_char(*s++);
        ++written;
    }
    return written;
}

static int emit_unsigned(uint64_t value, uint32_t base) {
    char buf[32];
    int pos = 0;
    int written = 0;
    const char digits[] = "0123456789abcdef";

    if (base < 2 || base > 16) {
        return 0;
    }
    if (value == 0) {
        return emit_char('0');
    }
    while (value != 0 && pos < (int)sizeof(buf)) {
        buf[pos++] = digits[value % base];
        value /= base;
    }
    while (pos > 0) {
        written += emit_char(buf[--pos]);
    }
    return written;
}

static int emit_signed(int64_t value) {
    uint64_t magnitude;
    int written = 0;

    if (value < 0) {
        written += emit_char('-');
        magnitude = (uint64_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint64_t)value;
    }
    return written + emit_unsigned(magnitude, 10);
}

__attribute__((weak)) int printf_min(const char *fmt, ...) {
    va_list ap;
    int written = 0;

    if (fmt == 0) {
        return 0;
    }

    va_start(ap, fmt);
    while (*fmt != '\0') {
        if (*fmt != '%') {
            written += emit_char(*fmt++);
            continue;
        }

        ++fmt;
        if (*fmt == '\0') {
            break;
        }
        switch (*fmt) {
            case '%':
                written += emit_char('%');
                break;
            case 'c':
                written += emit_char((char)va_arg(ap, int));
                break;
            case 's':
                written += emit_string(va_arg(ap, const char *));
                break;
            case 'd':
                written += emit_signed((int64_t)va_arg(ap, int));
                break;
            case 'u':
                written += emit_unsigned((uint64_t)va_arg(ap, unsigned int), 10);
                break;
            case 'x':
                written += emit_unsigned((uint64_t)va_arg(ap, unsigned int), 16);
                break;
            default:
                written += emit_char('%');
                written += emit_char(*fmt);
                break;
        }
        ++fmt;
    }
    va_end(ap);
    return written;
}
