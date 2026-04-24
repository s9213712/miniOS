#include <mvos/vmm.h>
#include <stdint.h>

#define MVOS_VMM_MAX_REGIONS 32

typedef struct {
    int in_use;
    uint64_t base;
    uint64_t size;
    uint64_t flags;
    char tag[24];
} mvos_vmm_region_t;

static uint64_t g_hhdm_offset;
static mvos_vmm_region_t g_regions[MVOS_VMM_MAX_REGIONS];

static uint64_t g_user_heap_base;
static uint64_t g_user_heap_mapped_end;
static uint64_t g_user_heap_limit;
static uint64_t g_user_brk;
static int g_user_heap_ready;

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    return (value + align - 1ULL) & ~(align - 1ULL);
}

static int add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static void copy_tag(char *dst, uint64_t cap, const char *src) {
    if (cap == 0) {
        return;
    }
    if (src == 0) {
        src = "(anon)";
    }
    uint64_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static int range_overlaps(uint64_t base_a, uint64_t size_a, uint64_t base_b, uint64_t size_b) {
    uint64_t end_a = 0;
    uint64_t end_b = 0;
    if (add_overflow_u64(base_a, size_a, &end_a) != 0) {
        return 1;
    }
    if (add_overflow_u64(base_b, size_b, &end_b) != 0) {
        return 1;
    }
    return !(end_a <= base_b || end_b <= base_a);
}

void vmm_init(uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;
    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        g_regions[i] = (mvos_vmm_region_t){0};
    }
    g_user_heap_base = 0;
    g_user_heap_mapped_end = 0;
    g_user_heap_limit = 0;
    g_user_brk = 0;
    g_user_heap_ready = 0;
}

int vmm_map_range(uint64_t vaddr, uint64_t size, uint64_t flags, const char *tag) {
    if (vaddr == 0 || size == 0) {
        return -1;
    }
    if ((vaddr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0 || (size & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        return -2;
    }

    uint64_t end = 0;
    if (add_overflow_u64(vaddr, size, &end) != 0 || end <= vaddr) {
        return -3;
    }

    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (!g_regions[i].in_use) {
            continue;
        }
        if (range_overlaps(vaddr, size, g_regions[i].base, g_regions[i].size)) {
            return -4;
        }
    }

    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (g_regions[i].in_use) {
            continue;
        }
        g_regions[i].in_use = 1;
        g_regions[i].base = vaddr;
        g_regions[i].size = size;
        g_regions[i].flags = flags;
        copy_tag(g_regions[i].tag, sizeof(g_regions[i].tag), tag);
        return 0;
    }
    return -5;
}

int vmm_unmap_range(uint64_t vaddr, uint64_t size) {
    if (vaddr == 0 || size == 0) {
        return -1;
    }
    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (!g_regions[i].in_use) {
            continue;
        }
        if (g_regions[i].base == vaddr && g_regions[i].size == size) {
            g_regions[i].in_use = 0;
            g_regions[i].base = 0;
            g_regions[i].size = 0;
            g_regions[i].flags = 0;
            g_regions[i].tag[0] = '\0';
            return 0;
        }
    }
    return -2;
}

uint32_t vmm_region_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (g_regions[i].in_use) {
            ++count;
        }
    }
    return count;
}

int vmm_region_at(uint32_t index, mvos_vmm_region_info_t *out) {
    if (out == 0) {
        return -1;
    }
    uint32_t seen = 0;
    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (!g_regions[i].in_use) {
            continue;
        }
        if (seen == index) {
            out->base = g_regions[i].base;
            out->size = g_regions[i].size;
            out->flags = g_regions[i].flags;
            copy_tag(out->tag, sizeof(out->tag), g_regions[i].tag);
            return 0;
        }
        ++seen;
    }
    return -2;
}

void vmm_user_heap_init(uint64_t base, uint64_t reserve_bytes, uint64_t max_bytes) {
    uint64_t aligned_base = align_up_u64(base, MVOS_VMM_PAGE_SIZE);
    uint64_t reserve = align_up_u64(reserve_bytes == 0 ? MVOS_VMM_PAGE_SIZE : reserve_bytes, MVOS_VMM_PAGE_SIZE);
    uint64_t max = align_up_u64(max_bytes == 0 ? reserve : max_bytes, MVOS_VMM_PAGE_SIZE);
    if (max < reserve) {
        max = reserve;
    }

    uint64_t mapped_end = 0;
    uint64_t limit = 0;
    if (add_overflow_u64(aligned_base, reserve, &mapped_end) != 0) {
        return;
    }
    if (add_overflow_u64(aligned_base, max, &limit) != 0) {
        return;
    }

    if (vmm_map_range(aligned_base,
                      reserve,
                      MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER,
                      "user-brk") != 0) {
        return;
    }

    g_user_heap_base = aligned_base;
    g_user_heap_mapped_end = mapped_end;
    g_user_heap_limit = limit;
    g_user_brk = aligned_base;
    g_user_heap_ready = 1;
}

uint64_t vmm_user_brk_get(void) {
    return g_user_brk;
}

uint64_t vmm_user_brk_set(uint64_t new_brk) {
    if (!g_user_heap_ready) {
        return 0;
    }
    if (new_brk == 0) {
        return g_user_brk;
    }
    if (new_brk < g_user_heap_base || new_brk > g_user_heap_limit) {
        return g_user_brk;
    }

    uint64_t needed_end = align_up_u64(new_brk, MVOS_VMM_PAGE_SIZE);
    while (needed_end > g_user_heap_mapped_end) {
        if (g_user_heap_mapped_end >= g_user_heap_limit) {
            return g_user_brk;
        }
        uint64_t chunk = needed_end - g_user_heap_mapped_end;
        if (chunk > (g_user_heap_limit - g_user_heap_mapped_end)) {
            chunk = g_user_heap_limit - g_user_heap_mapped_end;
        }
        if (chunk == 0) {
            break;
        }
        if (vmm_map_range(g_user_heap_mapped_end,
                          chunk,
                          MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER,
                          "user-brk") != 0) {
            return g_user_brk;
        }
        g_user_heap_mapped_end += chunk;
    }

    g_user_brk = new_brk;
    return g_user_brk;
}

uint64_t vmm_user_brk_limit(void) {
    return g_user_heap_limit;
}
