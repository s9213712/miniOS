/* Interrupt subsystem is intentionally kept minimal for teaching:
 * timer tick generation + basic fault entry points.
 */
#include <mvos/log.h>
#include <mvos/panic.h>
#include <mvos/serial.h>
#include <mvos/interrupt.h>
#include <mvos/keyboard.h>
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

static inline uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(value));
    return value;
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

static uint64_t timer_tick_count;

static void timer_tick_bump(void) {
    /* ISR-side bump uses an atomic RMW so normal kernel reads never observe a torn update. */
    __atomic_add_fetch(&timer_tick_count, 1, __ATOMIC_RELAXED);
}

uint64_t timer_ticks(void) {
    /* Exposed as a read-only debug counter for task and test output. */
    return __atomic_load_n(&timer_tick_count, __ATOMIC_RELAXED);
}

void __attribute__((interrupt)) isr_timer(struct interrupt_frame *frame) {
    /* Timer interrupt handler:
     * bump debug tick, notify scheduler, then acknowledge the PIC.
     */
    (void)frame;
    timer_tick_bump();
    scheduler_on_timer_tick();

    outb(PIC1_COMMAND, PIC_EOI);
}

void __attribute__((interrupt)) isr_keyboard(struct interrupt_frame *frame) {
    (void)frame;
    keyboard_handle_irq();
    outb(PIC1_COMMAND, PIC_EOI);
}

static void remap_pic(void) {
    /* Re-map PIC IRQs to avoid clashing with CPU exception vectors 0x00-0x1f. */
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, PIC1_ICW1_INIT | PIC1_ICW1_ICW4);
    mvos_io_wait();
    outb(PIC2_COMMAND, PIC1_ICW1_INIT | PIC1_ICW1_ICW4);
    mvos_io_wait();
    outb(PIC1_DATA, PIC1_OFFSET);
    mvos_io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);
    mvos_io_wait();
    outb(PIC1_DATA, 4);
    mvos_io_wait();
    outb(PIC2_DATA, 2);
    mvos_io_wait();
    outb(PIC1_DATA, PIC1_ICW4_8086);
    mvos_io_wait();
    outb(PIC2_DATA, PIC1_ICW4_8086);
    mvos_io_wait();

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

static void pic_unmask_irq(uint8_t irq_line) {
    if (irq_line >= 8u) {
        return;
    }
    uint8_t mask = inb(PIC1_DATA);
    outb(PIC1_DATA, (uint8_t)(mask & (uint8_t)~(1u << irq_line)));
}

void timer_init(uint16_t hz) {
    /* Configure PIT channel0 periodic mode and enable timer IRQ. */
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
    mvos_io_wait();
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xff));
    mvos_io_wait();
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor >> 8));

    pic_unmask_irq(0);
    pic_unmask_irq(1);
    __asm__ volatile("sti");
}

static void fault_log_and_panic(const char *name, uint64_t vector, uint64_t error_code, const struct interrupt_frame *frame) {
    mvos_panic_context_t context = {
        .valid_mask = MVOS_PANIC_CTX_HAS_VECTOR,
        .vector = vector,
    };
    if (error_code != UINT64_MAX) {
        context.valid_mask |= MVOS_PANIC_CTX_HAS_ERROR;
        context.error_code = error_code;
    }
    if (frame) {
        context.valid_mask |= MVOS_PANIC_CTX_HAS_FRAME;
        context.frame = (mvos_panic_frame_t){
            .rip = frame->rip,
            .cs = frame->cs,
            .rflags = frame->rflags,
            .rsp = frame->rsp,
            .ss = frame->ss,
        };
    }
    klog("[fault] ");
    klogln(name);
    panic_with_context(name, &context);
}

void __attribute__((interrupt)) isr_divide_by_zero(struct interrupt_frame *frame) {
    fault_log_and_panic("Divide Error", 0, UINT64_MAX, frame);
}

void __attribute__((interrupt)) isr_invalid_opcode(struct interrupt_frame *frame) {
    fault_log_and_panic("Invalid Opcode", 6, UINT64_MAX, frame);
}

void __attribute__((interrupt)) isr_bounds_range_exceeded(struct interrupt_frame *frame) {
    fault_log_and_panic("Bounds Range Exceeded", 5, UINT64_MAX, frame);
}

void __attribute__((interrupt)) isr_double_fault(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Double Fault", 8, error_code, frame);
}

void __attribute__((interrupt)) isr_invalid_tss(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Invalid TSS", 10, error_code, frame);
}

void __attribute__((interrupt)) isr_segment_not_present(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Segment Not Present", 11, error_code, frame);
}

void __attribute__((interrupt)) isr_stack_segment_fault(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Stack Segment Fault", 12, error_code, frame);
}

void __attribute__((interrupt)) isr_general_protection(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("General Protection Fault", 13, error_code, frame);
}

void __attribute__((interrupt)) isr_page_fault(struct interrupt_frame *frame, uint64_t error_code) {
    /* Page Fault diagnostics need both RIP and CR2; CR2 identifies the faulting address. */
    uint64_t fault_addr = read_cr2();
    mvos_panic_context_t context = {
        .valid_mask = MVOS_PANIC_CTX_HAS_VECTOR | MVOS_PANIC_CTX_HAS_ERROR | MVOS_PANIC_CTX_HAS_CR2,
        .vector = 14,
        .error_code = error_code,
        .cr2 = fault_addr,
    };
    if (frame) {
        context.valid_mask |= MVOS_PANIC_CTX_HAS_FRAME;
        context.frame = (mvos_panic_frame_t){
            .rip = frame->rip,
            .cs = frame->cs,
            .rflags = frame->rflags,
            .rsp = frame->rsp,
            .ss = frame->ss,
        };
    }
    klogln("[fault] Page Fault");
    panic_with_context("Page Fault", &context);
}

void __attribute__((interrupt)) isr_alignment_check(struct interrupt_frame *frame, uint64_t error_code) {
    fault_log_and_panic("Alignment Check", 17, error_code, frame);
}
