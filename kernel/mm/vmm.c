#include <mvos/vmm.h>
#include <stdint.h>

#define MVOS_VMM_MAX_REGIONS 32
#define MVOS_VMM_PTE_COUNT 512
#define MVOS_VMM_PTE_PRESENT 0x001ULL
#define MVOS_VMM_PTE_WRITE 0x002ULL
#define MVOS_VMM_PTE_USER 0x004ULL
#define MVOS_VMM_PTE_HUGE 0x080ULL
#define MVOS_VMM_PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

extern void *pmm_allocate_pages(uint64_t page_count) __attribute__((weak));
extern void pmm_release_pages(void *start, uint64_t page_count) __attribute__((weak));

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

static void vmm_log_region_full(void) {
    extern void console_write_string(const char *msg) __attribute__((weak));
    if (console_write_string) {
        console_write_string("[vmm] region table full (max=32)\n");
    }
}

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    if (value > UINT64_MAX - (align - 1ULL)) {
        return UINT64_MAX;
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

static void zero_page(void *page) {
    uint8_t *p = (uint8_t *)page;
    for (uint64_t i = 0; i < MVOS_VMM_PAGE_SIZE; ++i) {
        p[i] = 0;
    }
}

static uint64_t kernel_virt_to_phys(const void *ptr) {
    return (uint64_t)(uintptr_t)ptr - g_hhdm_offset;
}

static uint64_t *phys_to_kernel_virt(uint64_t phys) {
    return (uint64_t *)(uintptr_t)(phys + g_hhdm_offset);
}

static uint64_t read_cr3_phys(void) {
    uint64_t cr3 = 0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & MVOS_VMM_PTE_ADDR_MASK;
}

static void flush_user_page(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static uint64_t *ensure_next_table(uint64_t *table, uint64_t index) {
    uint64_t entry = table[index];
    if ((entry & MVOS_VMM_PTE_PRESENT) != 0) {
        if ((entry & MVOS_VMM_PTE_HUGE) != 0) {
            return 0;
        }
        if ((entry & MVOS_VMM_PTE_USER) == 0) {
            return 0;
        }
        return phys_to_kernel_virt(entry & MVOS_VMM_PTE_ADDR_MASK);
    }

    if (pmm_allocate_pages == 0) {
        return 0;
    }
    void *page = pmm_allocate_pages(1);
    if (page == 0) {
        return 0;
    }
    zero_page(page);
    table[index] = kernel_virt_to_phys(page) |
                   MVOS_VMM_PTE_PRESENT |
                   MVOS_VMM_PTE_WRITE |
                   MVOS_VMM_PTE_USER;
    return (uint64_t *)page;
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

static void clear_user_backing_range(uint64_t vaddr, uint64_t size) {
    if (g_hhdm_offset == 0 || size == 0) {
        return;
    }

    uint64_t end = 0;
    if (add_overflow_u64(vaddr, size, &end) != 0) {
        return;
    }

    uint64_t *pml4 = phys_to_kernel_virt(read_cr3_phys());
    for (uint64_t page = vaddr; page < end; page += MVOS_VMM_PAGE_SIZE) {
        uint64_t pml4_i = (page >> 39) & 0x1FFULL;
        uint64_t pdpt_i = (page >> 30) & 0x1FFULL;
        uint64_t pd_i = (page >> 21) & 0x1FFULL;
        uint64_t pt_i = (page >> 12) & 0x1FFULL;

        uint64_t pml4e = pml4[pml4_i];
        if ((pml4e & MVOS_VMM_PTE_PRESENT) == 0 || (pml4e & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pdpt = phys_to_kernel_virt(pml4e & MVOS_VMM_PTE_ADDR_MASK);
        uint64_t pdpte = pdpt[pdpt_i];
        if ((pdpte & MVOS_VMM_PTE_PRESENT) == 0 || (pdpte & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pd = phys_to_kernel_virt(pdpte & MVOS_VMM_PTE_ADDR_MASK);
        uint64_t pde = pd[pd_i];
        if ((pde & MVOS_VMM_PTE_PRESENT) == 0 || (pde & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pt = phys_to_kernel_virt(pde & MVOS_VMM_PTE_ADDR_MASK);
        if ((pt[pt_i] & MVOS_VMM_PTE_PRESENT) != 0) {
            if (pmm_release_pages != 0) {
                uint64_t phys = pt[pt_i] & MVOS_VMM_PTE_ADDR_MASK;
                pmm_release_pages((void *)(uintptr_t)(phys + g_hhdm_offset), 1);
            }
            pt[pt_i] = 0;
            flush_user_page(page);
        }
    }
}

static void protect_user_backing_range(uint64_t vaddr, uint64_t size, uint64_t flags) {
    if (g_hhdm_offset == 0 || size == 0) {
        return;
    }

    uint64_t end = 0;
    if (add_overflow_u64(vaddr, size, &end) != 0) {
        return;
    }

    uint64_t pte_flags = MVOS_VMM_PTE_PRESENT | MVOS_VMM_PTE_USER;
    if ((flags & MVOS_VMM_FLAG_WRITE) != 0) {
        pte_flags |= MVOS_VMM_PTE_WRITE;
    }

    uint64_t *pml4 = phys_to_kernel_virt(read_cr3_phys());
    for (uint64_t page = vaddr; page < end; page += MVOS_VMM_PAGE_SIZE) {
        uint64_t pml4_i = (page >> 39) & 0x1FFULL;
        uint64_t pdpt_i = (page >> 30) & 0x1FFULL;
        uint64_t pd_i = (page >> 21) & 0x1FFULL;
        uint64_t pt_i = (page >> 12) & 0x1FFULL;

        uint64_t pml4e = pml4[pml4_i];
        if ((pml4e & MVOS_VMM_PTE_PRESENT) == 0 || (pml4e & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pdpt = phys_to_kernel_virt(pml4e & MVOS_VMM_PTE_ADDR_MASK);
        uint64_t pdpte = pdpt[pdpt_i];
        if ((pdpte & MVOS_VMM_PTE_PRESENT) == 0 || (pdpte & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pd = phys_to_kernel_virt(pdpte & MVOS_VMM_PTE_ADDR_MASK);
        uint64_t pde = pd[pd_i];
        if ((pde & MVOS_VMM_PTE_PRESENT) == 0 || (pde & MVOS_VMM_PTE_HUGE) != 0) {
            continue;
        }
        uint64_t *pt = phys_to_kernel_virt(pde & MVOS_VMM_PTE_ADDR_MASK);
        if ((pt[pt_i] & MVOS_VMM_PTE_PRESENT) != 0) {
            uint64_t phys = pt[pt_i] & MVOS_VMM_PTE_ADDR_MASK;
            pt[pt_i] = phys | pte_flags;
            flush_user_page(page);
        }
    }
}

static int map_user_backing_range(uint64_t base, uint64_t size, uint64_t flags) {
    if (g_hhdm_offset == 0) {
        return 0;
    }
    uint64_t end = 0;
    if (add_overflow_u64(base, size, &end) != 0) {
        return -1;
    }
    for (uint64_t page = base; page < end; page += MVOS_VMM_PAGE_SIZE) {
        void *backing = 0;
        if (vmm_map_user_backed_page(page, flags, &backing) != 0) {
            clear_user_backing_range(base, page - base);
            return -1;
        }
    }
    return 0;
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
    vmm_log_region_full();
    return -5;
}

int vmm_unmap_range(uint64_t vaddr, uint64_t size) {
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

    uint64_t cursor = vaddr;
    while (cursor < end) {
        int found = 0;
        for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
            if (!g_regions[i].in_use) {
                continue;
            }
            uint64_t region_end = 0;
            if (add_overflow_u64(g_regions[i].base, g_regions[i].size, &region_end) != 0) {
                continue;
            }
            if (cursor < g_regions[i].base || cursor >= region_end) {
                continue;
            }
            if (g_regions[i].base < vaddr || region_end > end) {
                return -2; /* Partial unmap splitting is not modeled yet. */
            }
            cursor = region_end;
            found = 1;
            break;
        }
        if (!found) {
            return -2;
        }
    }

    clear_user_backing_range(vaddr, size);
    for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
        if (!g_regions[i].in_use) {
            continue;
        }
        uint64_t region_end = 0;
        if (add_overflow_u64(g_regions[i].base, g_regions[i].size, &region_end) != 0) {
            continue;
        }
        if (g_regions[i].base >= vaddr && region_end <= end) {
            g_regions[i].in_use = 0;
            g_regions[i].base = 0;
            g_regions[i].size = 0;
            g_regions[i].flags = 0;
            g_regions[i].tag[0] = '\0';
        }
    }
    return 0;
}

int vmm_protect_range(uint64_t vaddr, uint64_t size, uint64_t flags) {
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
        if ((g_regions[i].flags & MVOS_VMM_FLAG_USER) == 0) {
            continue;
        }
        uint64_t region_end = 0;
        if (add_overflow_u64(g_regions[i].base, g_regions[i].size, &region_end) != 0) {
            continue;
        }
        if (vaddr < g_regions[i].base || end > region_end) {
            continue;
        }

        uint64_t pre_size = vaddr - g_regions[i].base;
        uint64_t post_size = region_end - end;
        uint32_t free_slots[2] = {0, 0};
        uint32_t needed_slots = ((pre_size != 0) ? 1u : 0u) + ((post_size != 0) ? 1u : 0u);
        uint32_t found_slots = 0;
        for (uint32_t j = 0; j < MVOS_VMM_MAX_REGIONS && found_slots < needed_slots; ++j) {
            if (!g_regions[j].in_use) {
                free_slots[found_slots++] = j;
            }
        }
        if (found_slots < needed_slots) {
            return -5;
        }

        mvos_vmm_region_t original = g_regions[i];
        uint32_t slot_cursor = 0;
        if (pre_size != 0) {
            uint32_t slot = free_slots[slot_cursor++];
            g_regions[slot] = original;
            g_regions[slot].base = original.base;
            g_regions[slot].size = pre_size;
        }
        if (post_size != 0) {
            uint32_t slot = free_slots[slot_cursor++];
            g_regions[slot] = original;
            g_regions[slot].base = end;
            g_regions[slot].size = post_size;
        }

        g_regions[i].base = vaddr;
        g_regions[i].size = size;
        g_regions[i].flags = flags;
        copy_tag(g_regions[i].tag, sizeof(g_regions[i].tag), original.tag);
        protect_user_backing_range(vaddr, size, flags);
        return 0;
    }
    return -3;
}

int vmm_map_user_backed_page(uint64_t vaddr, uint64_t flags, void **out_kernel_page) {
    if (out_kernel_page == 0) {
        return -1;
    }
    *out_kernel_page = 0;
    if (vaddr == 0 || (vaddr & (MVOS_VMM_PAGE_SIZE - 1ULL)) != 0) {
        return -2;
    }
    if ((flags & MVOS_VMM_FLAG_USER) == 0) {
        return -3;
    }

    /* Host-side tests run VMM as metadata only, without PMM/HHDM/CR3 access. */
    if (g_hhdm_offset == 0 || pmm_allocate_pages == 0) {
        return 0;
    }

    uint64_t *pml4 = phys_to_kernel_virt(read_cr3_phys());
    uint64_t pml4_i = (vaddr >> 39) & 0x1FFULL;
    uint64_t pdpt_i = (vaddr >> 30) & 0x1FFULL;
    uint64_t pd_i = (vaddr >> 21) & 0x1FFULL;
    uint64_t pt_i = (vaddr >> 12) & 0x1FFULL;

    uint64_t *pdpt = ensure_next_table(pml4, pml4_i);
    if (pdpt == 0) {
        return -5;
    }
    uint64_t *pd = ensure_next_table(pdpt, pdpt_i);
    if (pd == 0) {
        return -6;
    }
    uint64_t *pt = ensure_next_table(pd, pd_i);
    if (pt == 0) {
        return -7;
    }
    uint64_t pte_flags = MVOS_VMM_PTE_PRESENT | MVOS_VMM_PTE_USER;
    if ((flags & MVOS_VMM_FLAG_WRITE) != 0) {
        pte_flags |= MVOS_VMM_PTE_WRITE;
    }

    uint64_t existing = pt[pt_i];
    if ((existing & MVOS_VMM_PTE_PRESENT) != 0) {
        void *backing = phys_to_kernel_virt(existing & MVOS_VMM_PTE_ADDR_MASK);
        zero_page(backing);
        pt[pt_i] = (existing & MVOS_VMM_PTE_ADDR_MASK) | pte_flags;
        flush_user_page(vaddr);
        *out_kernel_page = backing;
        return 0;
    }

    void *backing = pmm_allocate_pages(1);
    if (backing == 0) {
        return -4;
    }
    zero_page(backing);

    pt[pt_i] = kernel_virt_to_phys(backing) | pte_flags;
    flush_user_page(vaddr);
    *out_kernel_page = backing;
    return 0;
}

int vmm_user_range_check(uint64_t vaddr, uint64_t size, uint64_t required_flags) {
    if (size == 0) {
        return 0;
    }
    if (vaddr == 0) {
        return -1;
    }

    uint64_t end = 0;
    if (add_overflow_u64(vaddr, size, &end) != 0 || end <= vaddr) {
        return -2;
    }

    uint64_t cursor = vaddr;
    while (cursor < end) {
        const mvos_vmm_region_t *match = 0;
        for (uint32_t i = 0; i < MVOS_VMM_MAX_REGIONS; ++i) {
            if (!g_regions[i].in_use) {
                continue;
            }
            if ((g_regions[i].flags & MVOS_VMM_FLAG_USER) == 0) {
                continue;
            }
            if ((g_regions[i].flags & required_flags) != required_flags) {
                continue;
            }
            uint64_t region_end = 0;
            if (add_overflow_u64(g_regions[i].base, g_regions[i].size, &region_end) != 0) {
                continue;
            }
            if (cursor < g_regions[i].base || cursor >= region_end) {
                continue;
            }
            match = &g_regions[i];
            break;
        }
        if (match == 0) {
            return -3;
        }
        uint64_t match_end = match->base + match->size;
        if (match_end < match->base) {
            return -4;
        }
        cursor = (match_end < end) ? match_end : end;
    }
    return 0;
}

int vmm_copy_to_user(uint64_t vaddr, const void *src, uint64_t size) {
    if (size == 0) {
        return 0;
    }
    if (vaddr == 0 || src == 0) {
        return -1;
    }
    if (vmm_user_range_check(vaddr, size, 0) != 0) {
        return -2;
    }

    /* Host-side tests keep only metadata and validate layout contracts. */
    if (g_hhdm_offset == 0) {
        return 0;
    }

    uint8_t *dst = (uint8_t *)(uintptr_t)vaddr;
    const uint8_t *in = (const uint8_t *)src;
    for (uint64_t i = 0; i < size; ++i) {
        dst[i] = in[i];
    }
    return 0;
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
    if (aligned_base == UINT64_MAX || reserve == UINT64_MAX || max == UINT64_MAX) {
        return;
    }
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
    if (map_user_backing_range(aligned_base,
                               reserve,
                               MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER) != 0) {
        (void)vmm_unmap_range(aligned_base, reserve);
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
    if (needed_end == UINT64_MAX) {
        return g_user_brk;
    }
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
        if (map_user_backing_range(g_user_heap_mapped_end,
                                   chunk,
                                   MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER) != 0) {
            (void)vmm_unmap_range(g_user_heap_mapped_end, chunk);
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
