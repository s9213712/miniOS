#include <mvos/heap.h>
#include <mvos/limine.h>
#include <mvos/panic.h>
#include <mvos/pmm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    TEST_PAGE_SIZE = 4096,
};

static uint8_t g_fake_ram[TEST_PAGE_SIZE * 16] __attribute__((aligned(TEST_PAGE_SIZE)));

void klog(const char *message) {
    (void)message;
}

void klogln(const char *message) {
    (void)message;
}

void klog_u64(uint64_t value) {
    (void)value;
}

void panic(const char *message) {
    fprintf(stderr, "[test_heap_free] panic: %s\n", message ? message : "(null)");
    abort();
}

int main(void) {
    struct limine_memmap_entry entry = {
        .base = (uint64_t)(uintptr_t)g_fake_ram,
        .length = sizeof(g_fake_ram),
        .type = LIMINE_MEMMAP_USABLE,
    };
    struct limine_memmap_entry *entries[] = {&entry};
    struct limine_memmap_response response = {
        .revision = 0,
        .entry_count = 1,
        .entries = entries,
    };

    pmm_init(&response, 0);
    heap_init();

    uint64_t free_before = pmm_free_pages();
    void *a = kmalloc(256);
    if (a == NULL) {
        fprintf(stderr, "[test_heap_free] kmalloc(256) failed\n");
        return 1;
    }
    if (pmm_free_pages() != free_before - 1 || pmm_allocated_pages() != 1) {
        fprintf(stderr, "[test_heap_free] small allocation accounting mismatch free=%llu alloc=%llu\n",
                (unsigned long long)pmm_free_pages(),
                (unsigned long long)pmm_allocated_pages());
        return 1;
    }

    void *b = kmalloc(5000);
    if (b == NULL) {
        fprintf(stderr, "[test_heap_free] kmalloc(5000) failed\n");
        return 1;
    }
    if (pmm_allocated_pages() != 3) {
        fprintf(stderr, "[test_heap_free] expected 3 allocated pages after large alloc, got %llu\n",
                (unsigned long long)pmm_allocated_pages());
        return 1;
    }

    kfree(a);
    if (pmm_allocated_pages() != 2) {
        fprintf(stderr, "[test_heap_free] expected 2 allocated pages after freeing small block, got %llu\n",
                (unsigned long long)pmm_allocated_pages());
        return 1;
    }

    kfree(b);
    if (pmm_allocated_pages() != 0 || pmm_free_pages() != free_before) {
        fprintf(stderr, "[test_heap_free] free accounting mismatch free=%llu alloc=%llu baseline=%llu\n",
                (unsigned long long)pmm_free_pages(),
                (unsigned long long)pmm_allocated_pages(),
                (unsigned long long)free_before);
        return 1;
    }

    printf("[test_heap_free] PASS\n");
    return 0;
}
