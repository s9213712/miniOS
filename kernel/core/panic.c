#include <mvos/panic.h>
#include <mvos/serial.h>
#include <mvos/log.h>
#include <stddef.h>

static void panic_write_context(const mvos_panic_context_t *context) {
    if (context == NULL) {
        return;
    }
    if (context->valid_mask & MVOS_PANIC_CTX_HAS_VECTOR) {
        klog("\n  vector=");
        serial_write_u64(context->vector);
    }
    if (context->valid_mask & MVOS_PANIC_CTX_HAS_ERROR) {
        klog(" error=");
        serial_write_u64(context->error_code);
    }
    if (context->valid_mask & MVOS_PANIC_CTX_HAS_CR2) {
        klog(" cr2=");
        serial_write_u64(context->cr2);
    }
    if (context->valid_mask & MVOS_PANIC_CTX_HAS_FRAME) {
        klog("\n  rip=");
        serial_write_u64(context->frame.rip);
        klog(" rsp=");
        serial_write_u64(context->frame.rsp);
        klog("\n  cs=");
        serial_write_u64(context->frame.cs);
        klog(" ss=");
        serial_write_u64(context->frame.ss);
        klog(" rflags=");
        serial_write_u64(context->frame.rflags);
    }
}

void panic_with_context(const char *message, const mvos_panic_context_t *context) {
    serial_init();
    klog("\n[panic] ");
    if (message) {
        klog(message);
    } else {
        klog("no message");
    }
    panic_write_context(context);
    klog("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void panic(const char *message) {
    panic_with_context(message, NULL);
}
