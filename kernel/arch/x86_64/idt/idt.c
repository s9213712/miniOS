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
extern void isr_invalid_opcode(struct interrupt_frame *frame);
extern void isr_general_protection(struct interrupt_frame *frame, uint64_t error_code);
extern void isr_page_fault(struct interrupt_frame *frame, uint64_t error_code);

static struct idt_entry idt[256];
static struct idt_pointer idt_ptr;

static void idt_set_gate(uint8_t index, void *handler) {
    uint64_t address = (uint64_t)handler;
    idt[index].offset_low = (uint16_t)(address & 0xFFFF);
    idt[index].selector = 0x08;
    idt[index].ist = 0;
    idt[index].type_attributes = 0x8E;
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

    idt_set_gate(0, isr_divide_by_zero);
    idt_set_gate(6, isr_invalid_opcode);
    idt_set_gate(13, isr_general_protection);
    idt_set_gate(14, isr_page_fault);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uint64_t)&idt;

    asm volatile("lidt %0" : : "m"(idt_ptr));
}
