#include <mvos/heap.h>
#include <mvos/pmm.h>
#include <mvos/log.h>

void heap_init(void) {
    klogln("[heap] init");
}

void *kmalloc(size_t size) {
    return pmm_alloc(size);
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    klog("[kfree] not supported yet, ptr=");
    klog_u64((uint64_t)ptr);
    klogln("");
    (void)ptr;
}
