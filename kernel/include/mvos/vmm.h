#pragma once

#include <stdint.h>

#define MVOS_VMM_PAGE_SIZE 4096ULL

enum {
    MVOS_VMM_FLAG_READ = 1u << 0,
    MVOS_VMM_FLAG_WRITE = 1u << 1,
    MVOS_VMM_FLAG_EXEC = 1u << 2,
    MVOS_VMM_FLAG_USER = 1u << 3,
};

typedef struct {
    uint64_t base;
    uint64_t size;
    uint64_t flags;
    char tag[24];
} mvos_vmm_region_info_t;

void vmm_init(uint64_t hhdm_offset);
int vmm_map_range(uint64_t vaddr, uint64_t size, uint64_t flags, const char *tag);
int vmm_unmap_range(uint64_t vaddr, uint64_t size);
int vmm_protect_range(uint64_t vaddr, uint64_t size, uint64_t flags);
int vmm_map_user_backed_page(uint64_t vaddr, uint64_t flags, void **out_kernel_page);
int vmm_user_range_check(uint64_t vaddr, uint64_t size, uint64_t required_flags);
int vmm_copy_to_user(uint64_t vaddr, const void *src, uint64_t size);
uint32_t vmm_region_count(void);
int vmm_region_at(uint32_t index, mvos_vmm_region_info_t *out);

void vmm_user_heap_init(uint64_t base, uint64_t reserve_bytes, uint64_t max_bytes);
uint64_t vmm_user_brk_get(void);
uint64_t vmm_user_brk_set(uint64_t new_brk);
uint64_t vmm_user_brk_limit(void);
void vmm_reset_user_state(void);
