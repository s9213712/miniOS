#include <stddef.h>
#include <stdint.h>
#include <mvos/gdt.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt_entries[3];
static struct gdt_descriptor gdt_ptr;

static void gdt_set_gate(uint8_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt_entries[index].base_low = (uint16_t)(base & 0xFFFF);
    gdt_entries[index].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt_entries[index].base_high = (uint8_t)((base >> 24) & 0xFF);

    gdt_entries[index].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt_entries[index].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt_entries[index].granularity |= granularity & 0xF0;
    gdt_entries[index].access = access;
}

void gdt_init(void) {
    for (size_t i = 0; i < sizeof(gdt_entries) / sizeof(gdt_entries[0]); ++i) {
        gdt_entries[i].limit_low = 0;
        gdt_entries[i].base_low = 0;
        gdt_entries[i].base_middle = 0;
        gdt_entries[i].base_high = 0;
        gdt_entries[i].access = 0;
        gdt_entries[i].granularity = 0;
    }

    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0); // code segment: present, exec/read, 64-bit
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xA0); // data segment: present, read/write, 64-bit

    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entries) - 1);
    gdt_ptr.base = (uint64_t)&gdt_entries;

    __asm__ volatile("lgdt %0\n"
                 "pushq $0x08\n"
                 "lea 1f(%%rip), %%rax\n"
                 "pushq %%rax\n"
                 "lretq\n"
                 "1:\n"
                 "movw $0x10, %%ax\n"
                 "movw %%ax, %%ds\n"
                 "movw %%ax, %%es\n"
                 "movw %%ax, %%ss\n"
                 "movw %%ax, %%fs\n"
                 "movw %%ax, %%gs\n"
                 :
                 : "m"(gdt_ptr)
                 : "rax", "memory");
}
