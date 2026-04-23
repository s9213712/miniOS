#include <mvos/heap.h>
#include <mvos/pmm.h>

void heap_init(void) {
}

void *kmalloc(size_t size) {
    return pmm_alloc(size);
}

void kfree(void *ptr) {
    (void)ptr;
}
