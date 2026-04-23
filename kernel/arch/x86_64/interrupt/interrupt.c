#include <mvos/log.h>
#include <mvos/panic.h>
#include <mvos/serial.h>
#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static void fault_log_and_panic(const char *name, uint64_t vector, uint64_t error_code, const struct interrupt_frame *frame) {
    serial_init();
    klogln("[fault]");
    klog(name);
    klog(" vector=");
    serial_write_u64(vector);
    if (error_code != UINT64_MAX) {
        klog(" error=");
        serial_write_u64(error_code);
    }
    if (frame) {
        klog(" rip=");
        serial_write_u64(frame->rip);
    }
    klogln("");
    panic("kernel exception");
}

void isr_divide_by_zero(struct interrupt_frame *frame) __attribute__((interrupt)) {
    fault_log_and_panic("Divide Error", 0, UINT64_MAX, frame);
}

void isr_invalid_opcode(struct interrupt_frame *frame) __attribute__((interrupt)) {
    fault_log_and_panic("Invalid Opcode", 6, UINT64_MAX, frame);
}

void isr_general_protection(struct interrupt_frame *frame, uint64_t error_code) __attribute__((interrupt)) {
    fault_log_and_panic("General Protection Fault", 13, error_code, frame);
}

void isr_page_fault(struct interrupt_frame *frame, uint64_t error_code) __attribute__((interrupt)) {
    fault_log_and_panic("Page Fault", 14, error_code, frame);
}
