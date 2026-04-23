#include <mvos/serial.h>

void klog(const char *message) {
    serial_write_string(message);
}

void klogln(const char *message) {
    serial_write_string(message);
    serial_write_string("\n");
}

void klog_u64(uint64_t value) {
    serial_write_u64(value);
    serial_write_string("\n");
}