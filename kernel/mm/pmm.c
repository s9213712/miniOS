#include <mvos/pmm.h>
#include <mvos/limine.h>
#include <mvos/log.h>
#include <mvos/panic.h>

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#define MVOS_PAGE_SIZE 4096ULL
#define MVOS_PMM_MAX_RELEASED_PAGES 2048ULL

static uint64_t g_hhdm_offset;
static uint64_t g_cursor;
static uint64_t g_limit;
static uint64_t g_region_base;
static bool g_initialized;
static uint64_t g_total_pages;
static uint64_t g_allocated_pages;
static uint64_t g_released_pages[MVOS_PMM_MAX_RELEASED_PAGES];
static uint64_t g_released_page_count;

static int pmm_is_released(uint64_t phys_page) {
    for (uint64_t i = 0; i < g_released_page_count; ++i) {
        if (g_released_pages[i] == phys_page) {
            return 1;
        }
    }
    return 0;
}

static void pmm_push_released(uint64_t phys_page) {
    if (g_released_page_count >= MVOS_PMM_MAX_RELEASED_PAGES) {
        return;
    }
    g_released_pages[g_released_page_count++] = phys_page;
}

static int pmm_pop_released(uint64_t *out_page) {
    if (g_released_page_count == 0 || out_page == NULL) {
        return 0;
    }
    *out_page = g_released_pages[--g_released_page_count];
    return 1;
}

static uint64_t align_up(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    align -= 1;
    return (value + align) & ~align;
}

static uint64_t align_up_checked(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    if (value > UINT64_MAX - (align - 1)) {
        return UINT64_MAX;
    }
    return (value + (align - 1ULL)) & ~(align - 1ULL);
}

static uint64_t align_down(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    return value & ~(align - 1ULL);
}

static void *to_virt(uint64_t physical) {
    return (void *)(physical + g_hhdm_offset);
}

void pmm_init(const struct limine_memmap_response *response, uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;
    uint64_t usable_regions = 0;
    uint64_t best_base = 0;
    uint64_t best_limit = 0;
    uint64_t best_size = 0;

    if (response == NULL || response->entries == NULL || response->entry_count == 0) {
        panic("PMM init failed: no memory map");
    }

    klogln("[pmm] memory map");
    for (uint64_t i = 0; i < response->entry_count; ++i) {
        const struct limine_memmap_entry *entry = response->entries[i];
        if (!entry) {
            continue;
        }

        klog("- base=");
        klog_u64(entry->base);
        klog(" len=");
        klog_u64(entry->length);
        klog(" type=");
        klog_u64(entry->type);
        klogln("");

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        if (entry->length < MVOS_PAGE_SIZE) {
            continue;
        }
        if (entry->base > UINT64_MAX - entry->length) {
            continue;
        }

        const uint64_t aligned_base = align_up(entry->base, MVOS_PAGE_SIZE);
        const uint64_t aligned_end = align_down(entry->base + entry->length, MVOS_PAGE_SIZE);
        if (aligned_end <= aligned_base) {
            continue;
        }

        usable_regions += 1;
        uint64_t usable_size = aligned_end - aligned_base;
        if (usable_size > best_size) {
            best_base = aligned_base;
            best_limit = aligned_end;
            best_size = usable_size;
        }
    }

    if (usable_regions == 0 || best_limit <= best_base) {
        panic("PMM init failed: no usable region");
    }

    g_cursor = align_up(best_base, MVOS_PAGE_SIZE);
    g_region_base = g_cursor;
    g_limit = best_limit;
    g_total_pages = (best_limit - g_cursor) / MVOS_PAGE_SIZE;
    g_allocated_pages = 0;
    g_released_page_count = 0;
    g_initialized = true;

    klog("[pmm] selected region base=");
    klog_u64(g_cursor);
    klog(" limit=");
    klog_u64(g_limit);
    klogln("");
}

uint64_t pmm_free_pages(void) {
    if (!g_initialized || g_cursor < g_region_base) {
        return 0;
    }
    uint64_t linear_free = 0;
    if (g_limit > g_cursor) {
        linear_free = (g_limit - g_cursor) / MVOS_PAGE_SIZE;
    }
    return linear_free + g_released_page_count;
}

uint64_t pmm_total_pages(void) {
    return g_total_pages;
}

uint64_t pmm_allocated_pages(void) {
    return g_allocated_pages;
}

void *pmm_allocate_pages(uint64_t page_count) {
    if (!g_initialized) {
        return NULL;
    }
    if (page_count == 0) {
        return NULL;
    }

    if (page_count == 1) {
        uint64_t phys = 0;
        while (pmm_pop_released(&phys) != 0) {
            if ((phys & (MVOS_PAGE_SIZE - 1ULL)) != 0 ||
                phys < g_region_base || phys >= g_limit) {
                continue;
            }
            void *page = to_virt(phys);
            memset(page, 0, MVOS_PAGE_SIZE);
            g_allocated_pages += 1;
            return page;
        }
    }

    if (page_count > (UINTPTR_MAX / MVOS_PAGE_SIZE)) {
        return NULL;
    }

    const uint64_t needed = page_count * MVOS_PAGE_SIZE;
    const uint64_t start = align_up_checked(g_cursor, MVOS_PAGE_SIZE);
    if (start == UINT64_MAX || start < g_cursor) {
        return NULL;
    }
    if (start >= g_limit || needed > (g_limit - start)) {
        return NULL;
    }

    g_cursor = start + needed;
    g_allocated_pages += page_count;
    return to_virt(start);
}

void pmm_release_pages(void *start, uint64_t page_count) {
    if (!g_initialized || start == NULL || page_count == 0) {
        return;
    }

    uint64_t phys = (uint64_t)(uintptr_t)start;
    if (phys < g_hhdm_offset || (phys - g_hhdm_offset) & (MVOS_PAGE_SIZE - 1ULL)) {
        return;
    }
    uint64_t page = (phys - g_hhdm_offset);

    for (uint64_t i = 0; i < page_count; ++i) {
        uint64_t candidate = page + i * MVOS_PAGE_SIZE;
        if ((candidate & (MVOS_PAGE_SIZE - 1ULL)) != 0 ||
            candidate < g_region_base || candidate >= g_limit ||
            pmm_is_released(candidate)) {
            continue;
        }
        memset(to_virt(candidate), 0, MVOS_PAGE_SIZE);
        pmm_push_released(candidate);
        if (g_allocated_pages > 0) {
            g_allocated_pages -= 1;
        }
    }
}

void *pmm_alloc(size_t bytes) {
    if (!g_initialized) {
        return NULL;
    }
    if (bytes == 0) {
        return NULL;
    }

    if (bytes > (SIZE_MAX - MVOS_PAGE_SIZE)) {
        return NULL;
    }

    uint64_t needed = align_up((uint64_t)bytes, 16);
    if (needed < MVOS_PAGE_SIZE) {
        needed = MVOS_PAGE_SIZE;
    }

    uint64_t start = align_up_checked(g_cursor, MVOS_PAGE_SIZE);
    if (start == UINT64_MAX || start < g_cursor) {
        return NULL;
    }
    if (start >= g_limit || needed > (g_limit - start)) {
        return NULL;
    }

    g_cursor = start + needed;
    g_allocated_pages += align_up(needed, MVOS_PAGE_SIZE) / MVOS_PAGE_SIZE;
    return to_virt(start);
}
