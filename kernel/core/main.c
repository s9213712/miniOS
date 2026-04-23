#include <mvos/serial.h>
#include <mvos/log.h>
#include <mvos/panic.h>
#include <mvos/gdt.h>
#include <mvos/idt.h>
#include <mvos/pmm.h>
#include <mvos/heap.h>
#include <mvos/limine.h>
#include <stdint.h>

static volatile uint64_t request_start[4]
    __attribute__((used, section(".requests"))) = LIMINE_REQUESTS_START_MARKER;

static volatile struct limine_bootloader_info_request bootloader_info_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile struct limine_stack_size_request stack_size_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .response = NULL,
    .stack_size = 1024 * 16
};

static volatile struct limine_hhdm_request hhdm_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile struct limine_memmap_request memmap_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile uint64_t request_end[2]
    __attribute__((used, section(".requests"))) = LIMINE_REQUESTS_END_MARKER;

static void trigger_divide_by_zero_fault(void) {
    asm volatile(
        "mov $1, %%rax\n\t"
        "xor %%rdx, %%rdx\n\t"
        "mov $0, %%rbx\n\t"
        "div %%rbx\n\t"
        :
        :
        : "rax", "rbx", "rdx", "cc");
}

static void trigger_invalid_opcode_fault(void) {
    asm volatile(".byte 0x0f, 0xff");
}

static void trigger_general_protection_fault(void) {
    asm volatile(
        "mov $0x28, %ax\n\t"
        "mov %ax, %gs\n\t"
        :
        :
        : "ax");
}

static void trigger_page_fault(void) {
    volatile uint64_t *bad = (volatile uint64_t *)0;
    *bad = 0x12345678;
}

void kmain(void) {
    serial_init();
    klogln("MiniOS Phase 3 bootstrap");
    klogln("boot banner: kernel entering C");
    klogln("hello from kernel");

#ifdef MINIOS_PANIC_TEST
    panic("panic test path enabled");
#endif

    gdt_init();
    idt_init();
    klogln("gdt/idt initialized");

    if (hhdm_request.response == NULL) {
        panic("missing HHDM request response");
    }
    if (memmap_request.response == NULL) {
        panic("missing memmap request response");
    }

    klogln("[phase3] collecting boot memory map");
    klog("hhdm_offset=");
    klog_u64(hhdm_request.response->offset);
    klogln("");
    pmm_init(memmap_request.response, hhdm_request.response->offset);
    klog("free pages: ");
    klog_u64((uint64_t)pmm_free_pages());
    klogln("");
    heap_init();

    void *page = pmm_allocate_pages(1);
    if (!page) {
        panic("pmm_allocate_pages(1) failed in phase 3");
    }
    klog("pmm_allocate_pages(1) = ");
    klog_u64((uint64_t)page);
    klogln("");

    void *heap_block = kmalloc(256);
    if (!heap_block) {
        panic("kmalloc(256) failed in phase 3");
    }
    klog("kmalloc(256) = ");
    klog_u64((uint64_t)heap_block);
    klogln("");
    klogln("[phase3] memory allocator ready");

#ifdef MINIOS_FAULT_TEST_DIVIDE_BY_ZERO
    klogln("triggering divide-by-zero fault test");
    trigger_divide_by_zero_fault();
#endif

#ifdef MINIOS_FAULT_TEST_INVALID_OPCODE
    klogln("triggering invalid opcode fault test");
    trigger_invalid_opcode_fault();
#endif

#ifdef MINIOS_FAULT_TEST_GP_FAULT
    klogln("triggering general-protection fault test");
    trigger_general_protection_fault();
#endif

#ifdef MINIOS_FAULT_TEST_PAGE_FAULT
    klogln("triggering page-fault test");
    trigger_page_fault();
#endif

    for (;;) {
        asm volatile("hlt");
    }
}
