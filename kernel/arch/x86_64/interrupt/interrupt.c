#include <mvos/log.h>
#include <mvos/panic.h>
#include <mvos/serial.h>
#include <mvos/interrupt.h>
#include <mvos/scheduler.h>
#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"((uint8_t)0));
}

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND 0x43
#define PIT_BASE_HZ 1193182

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xa0
#define PIC2_DATA 0xa1
#define PIC_EOI 0x20

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

#define PIC1_ICW1_INIT 0x10
#define PIC1_ICW1_ICW4 0x01
#define PIC1_ICW4_8086 0x01

static volatile uint64_t timer_tick_count;

static void timer_tick_bump(void) {
    ++timer_tick_count;
}

uint64_t timer_ticks(void) {
    return timer_tick_count;
}

void __attribute__((interrupt)) isr_timer(struct interrupt_frame *frame) {
    (void)frame;
    timer_tick_bump();
    scheduler_on_timer_tick();

    outb(PIC1_COMMAND, PIC_EOI);
}

static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, PIC1_ICW1_INIT | PIC1_ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, PIC1_ICW1_INIT | PIC1_ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);
    io_wait();
    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();
    outb(PIC1_DATA, PIC1_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC1_ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

static void pic_unmask_timer_irq(void) {
    uint8_t mask = inb(PIC1_DATA);
    outb(PIC1_DATA, (uint8_t)(mask & ~(1u << 0)));
}

void timer_init(uint16_t hz) {
    if (hz == 0) {
        panic("timer_init requires non-zero frequency");
    }

    __asm__ volatile("cli");
    remap_pic();

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor < 1) {
        divisor = 1;
    } else if (divisor > 0xffff) {
        divisor = 0xffff;
    }

    outb(PIT_COMMAND, 0x36);
    io_wait();
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xff));
    io_wait();
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor >> 8));

    pic_unmask_timer_irq();
    __asm__ volatile("sti");
}

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

void __attribute__((interrupt)) isr_divide_by_zero(struct interrupt_frame *frame) {
    fault_log_and_panic("Divide Error", 0, UINT64_MAX, frame);
}

void __attribute__((interrupt)) isr_invalid_opcode(struct interrupt_frame *frame) {
    fault_log_and_panic("Invalid Opcode", 6, UINT64_MAX, frame);
}

void __attribute__((interrupt)) isr_general_protection(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("General Protection Fault", 13, error_code, frame);
}

void __attribute__((interrupt)) isr_page_fault(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Page Fault", 14, error_code, frame);
}
