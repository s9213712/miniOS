#include <mvos/heap.h>
#include <mvos/pmm.h>
#include <mvos/log.h>
#include <stdint.h>

#define MVOS_HEAP_PAGE_SIZE 4096ULL
#define MVOS_HEAP_ALLOC_MAGIC 0x6d766f735f686561ULL

typedef struct {
    uint64_t magic;
    uint64_t page_count;
} mvos_heap_header_t;

static uint64_t heap_align_up(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    uint64_t mask = align - 1ULL;
    return (value + mask) & ~mask;
}

void heap_init(void) {
    klogln("[heap] init");
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint64_t total = heap_align_up((uint64_t)size + sizeof(mvos_heap_header_t), 16ULL);
    uint64_t page_count = heap_align_up(total, MVOS_HEAP_PAGE_SIZE) / MVOS_HEAP_PAGE_SIZE;
    if (page_count == 0) {
        return NULL;
    }

    mvos_heap_header_t *header = (mvos_heap_header_t *)pmm_allocate_pages(page_count);
    if (header == NULL) {
        return NULL;
    }
    header->magic = MVOS_HEAP_ALLOC_MAGIC;
    header->page_count = page_count;
    return (void *)(header + 1);
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    mvos_heap_header_t *header = ((mvos_heap_header_t *)ptr) - 1;
    if (header->magic != MVOS_HEAP_ALLOC_MAGIC || header->page_count == 0) {
        klog("[kfree] invalid heap pointer=");
        klog_u64((uint64_t)ptr);
        klogln("");
        return;
    }

    uint64_t page_count = header->page_count;
    header->magic = 0;
    header->page_count = 0;
    pmm_release_pages((void *)header, page_count);
}
