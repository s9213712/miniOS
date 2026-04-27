#include <mvos/panic.h>
#include <mvos/serial.h>
#include <mvos/log.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MVOS_PANIC_MAX_BACKTRACE_DEPTH = 12,
    MVOS_PANIC_MAX_FRAME_DELTA = 1u << 20,
};

static int panic_backtrace_frame_ok(const uintptr_t *frame, uintptr_t stack_floor) {
    uintptr_t frame_addr = (uintptr_t)frame;
    if (frame == NULL || (frame_addr & (sizeof(uintptr_t) - 1u)) != 0) {
        return 0;
    }
    (void)stack_floor;
    return 1;
}

static int panic_backtrace_link_ok(const uintptr_t *frame, uintptr_t next_frame) {
    uintptr_t frame_addr = (uintptr_t)frame;
    if ((next_frame & (sizeof(uintptr_t) - 1u)) != 0) {
        return 0;
    }
    if (next_frame <= frame_addr) {
        return 0;
    }
    if ((next_frame - frame_addr) > MVOS_PANIC_MAX_FRAME_DELTA) {
        return 0;
    }
    return 1;
}

static void panic_write_backtrace(uintptr_t *frame, uintptr_t stack_floor) {
    if (!panic_backtrace_frame_ok(frame, stack_floor)) {
        return;
    }

    klog("\n  backtrace:");
    for (uint64_t depth = 0; depth < MVOS_PANIC_MAX_BACKTRACE_DEPTH; ++depth) {
        uintptr_t return_addr = frame[1];
        if (return_addr == 0) {
            break;
        }

        klog("\n    #");
        serial_write_u64(depth);
        klog(" ");
        serial_write_u64((uint64_t)return_addr);

        uintptr_t next_frame = frame[0];
        if (next_frame == 0 || !panic_backtrace_link_ok(frame, next_frame)) {
            break;
        }
        frame = (uintptr_t *)next_frame;
        if (!panic_backtrace_frame_ok(frame, stack_floor)) {
            break;
        }
    }
}

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
    uintptr_t stack_floor = 0;
    uintptr_t *frame = (uintptr_t *)__builtin_frame_address(0);

    if (context != NULL && (context->valid_mask & MVOS_PANIC_CTX_HAS_FRAME)) {
        stack_floor = (uintptr_t)context->frame.rsp;
    }

    serial_init();
    klog("\n[panic] ");
    if (message) {
        klog(message);
    } else {
        klog("no message");
    }
    panic_write_context(context);
    panic_write_backtrace(frame, stack_floor);
    klog("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void panic_dump_backtrace(void) {
    panic_write_backtrace((uintptr_t *)__builtin_frame_address(0), 0);
}

void panic(const char *message) {
    panic_with_context(message, NULL);
}
