#include <mvos/console.h>

void klog(const char *message) {
    console_write_string(message);
}

void klogln(const char *message) {
    console_write_string(message);
    console_write_string("\n");
}

void klog_u64(uint64_t value) {
    console_write_u64(value);
    console_write_string("\n");
}
