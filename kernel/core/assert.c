#include <mvos/log.h>
#include <mvos/serial.h>
#include <mvos/panic.h>

void kassert_fail(const char *expr, const char *file, int line, const char *func) {
    serial_init();
    klog("[assert] ");
    klog(expr);
    klog(" failed in ");
    klog(func);
    klog(" at ");
    klog(file);
    klog(":");
    klog_u64((unsigned long long)line);
    panic("kernel assertion failed");
}
