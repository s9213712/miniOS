#include <stddef.h>
#include <stdint.h>
#include <mvos/idt.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct interrupt_frame;
extern void isr_divide_by_zero(struct interrupt_frame *frame);
extern void isr_bounds_range_exceeded(struct interrupt_frame *frame);
extern void isr_invalid_opcode(struct interrupt_frame *frame);
extern void isr_double_fault(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_invalid_tss(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_segment_not_present(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_stack_segment_fault(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_general_protection(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_page_fault(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_alignment_check(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_timer(struct interrupt_frame *frame);
extern void isr_keyboard(struct interrupt_frame *frame);
extern void isr_user_syscall(void);

static struct idt_entry idt[256];
static struct idt_pointer idt_ptr;

static void idt_set_gate(uint8_t index, void *handler, uint8_t type_attributes) {
    uint64_t address = (uint64_t)handler;
    idt[index].offset_low = (uint16_t)(address & 0xFFFF);
    idt[index].selector = 0x08;
    idt[index].ist = 0;
    idt[index].type_attributes = type_attributes;
    idt[index].offset_middle = (uint16_t)((address >> 16) & 0xFFFF);
    idt[index].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFF);
    idt[index].zero = 0;
}

static void idt_clear(void) {
    for (size_t i = 0; i < sizeof(idt) / sizeof(idt[0]); ++i) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attributes = 0;
        idt[i].offset_middle = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }
}

void idt_init(void) {
    idt_clear();

    idt_set_gate(0, isr_divide_by_zero, 0x8E);
    idt_set_gate(5, isr_bounds_range_exceeded, 0x8E);
    idt_set_gate(6, isr_invalid_opcode, 0x8E);
    idt_set_gate(8, isr_double_fault, 0x8E);
    idt_set_gate(10, isr_invalid_tss, 0x8E);
    idt_set_gate(11, isr_segment_not_present, 0x8E);
    idt_set_gate(12, isr_stack_segment_fault, 0x8E);
    idt_set_gate(13, isr_general_protection, 0x8E);
    idt_set_gate(14, isr_page_fault, 0x8E);
    idt_set_gate(17, isr_alignment_check, 0x8E);
    idt_set_gate(32, isr_timer, 0x8E);
    idt_set_gate(33, isr_keyboard, 0x8E);
    idt_set_gate(0x80, isr_user_syscall, 0xEF);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
