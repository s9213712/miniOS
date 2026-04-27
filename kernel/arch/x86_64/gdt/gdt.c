#include <stddef.h>
#include <stdint.h>
#include <mvos/gdt.h>

struct gdt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

extern uint64_t kernel_stack_top;

static struct gdt_descriptor gdtr;
static uint64_t gdt[7];
static struct tss gdt_tss;
static uint8_t g_kernel_transition_stack[16384] __attribute__((aligned(16)));

static void gdt_set_entry(size_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    uint64_t descriptor = 0;
    descriptor |= (uint64_t)(limit & 0xFFFFu);
    descriptor |= (uint64_t)(base & 0xFFFFu) << 16;
    descriptor |= (uint64_t)((base >> 16) & 0xFFu) << 32;
    descriptor |= (uint64_t)access << 40;
    descriptor |= (uint64_t)((limit >> 16) & 0x0Fu) << 48;
    descriptor |= (uint64_t)(flags & 0x0Fu) << 52;
    descriptor |= (uint64_t)((base >> 24) & 0xFFu) << 56;

    gdt[index] = descriptor;
}

static void gdt_clear(void) {
    for (size_t i = 0; i < sizeof(gdt) / sizeof(gdt[0]); ++i) {
        gdt[i] = 0;
    }
}

void gdt_init(void) {
    gdt_clear();
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA); /* kernel code */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC); /* kernel data */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA); /* user code */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC); /* user data */

    gdt_tss = (struct tss){0};
    /* Keep ring-3 to ring-0 transitions off the bootstrap stack used by kmain(). */
    gdt_tss.rsp0 = (uint64_t)(uintptr_t)(g_kernel_transition_stack + sizeof(g_kernel_transition_stack));
    gdt_tss.io_map_base = (uint16_t)sizeof(struct tss);

    uint64_t tss_base = (uint64_t)&gdt_tss;
    const uint32_t tss_limit = (uint32_t)sizeof(struct tss) - 1;
    gdt_set_entry(5, (uint32_t)(tss_base & 0xFFFFFFFFu), tss_limit, 0x89, 0x0);
    gdt[6] = tss_base >> 32;

    gdtr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtr.base = (uint64_t)gdt;

    __asm__ volatile(
        "lgdt %0\n"
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        : "m"(gdtr)
        : "rax", "memory");

    __asm__ volatile(
        "movw $0x28, %%ax\n"
        "ltr %%ax\n"
        :
        : : "ax");
}

uint64_t gdt_get_rsp0(void) {
    return gdt_tss.rsp0;
}

void gdt_set_rsp0(uint64_t rsp0) {
    gdt_tss.rsp0 = rsp0;
}
