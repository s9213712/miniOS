#include <mvos/serial.h>

__attribute__((weak)) int printf_min(const char *fmt, ...) {
    (void)fmt;
    serial_write_string("[printf_min] not implemented\n");
    return 0;
}