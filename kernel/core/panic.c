#include <mvos/panic.h>
#include <mvos/serial.h>
#include <mvos/log.h>

void panic(const char *message) {
    serial_init();
    klog("\n[panic] ");
    if (message) {
        klog(message);
    } else {
        klog("no message");
    }
    klog("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
