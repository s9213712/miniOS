#include <mvos/userimg.h>
#include <mvos/elf.h>
#include <mvos/vmm.h>
#include <stdint.h>

enum {
    ELF_MAGIC0 = 0x7f,
    ELF_MAGIC1 = 'E',
    ELF_MAGIC2 = 'L',
    ELF_MAGIC3 = 'F',
    ELF_PHDR_LOAD = 1,
    ELF_PHDR_FLAG_EXEC = 1,
    ELF_PHDR_FLAG_WRITE = 2,
    ELF_PHDR_FLAG_READ = 4,
};

enum {
    USERIMG_MAX_LAYOUT_REGIONS = 16,
    USERIMG_STACK_GAP_BYTES = 0x100000,
    USERIMG_STACK_SIZE_BYTES = 0x10000,
};

typedef struct __attribute__((packed)) {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

typedef struct __attribute__((packed)) {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

typedef struct {
    uint64_t base;
    uint64_t end;
    uint64_t flags;
} userimg_layout_region_t;

typedef struct {
    uint64_t base;
    uint64_t size;
} userimg_mapped_region_t;

extern const uint8_t g_linux_user_elf_sample[];
extern const uint64_t g_linux_user_elf_sample_len;

static userimg_mapped_region_t g_last_mapped[USERIMG_MAX_LAYOUT_REGIONS];
static uint32_t g_last_mapped_count;

static uint64_t align_down_u64(uint64_t value, uint64_t align) {
    if (align == 0) {
        return value;
    }
    return value & ~(align - 1ULL);
}

static int align_up_u64(uint64_t value, uint64_t align, uint64_t *out) {
    if (align == 0) {
        *out = value;
        return 0;
    }
    uint64_t mask = align - 1ULL;
    if (value > UINT64_MAX - mask) {
        return -1;
    }
    *out = (value + mask) & ~mask;
    return 0;
}

static int checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int ranges_overlap(uint64_t base_a, uint64_t end_a, uint64_t base_b, uint64_t end_b) {
    return !(end_a <= base_b || end_b <= base_a);
}

static int append_or_merge_region(userimg_layout_region_t *regions,
                                  uint32_t *count,
                                  uint64_t base,
                                  uint64_t end,
                                  uint64_t flags) {
    if (count == 0 || end <= base) {
        return -1;
    }

    uint64_t merged_base = base;
    uint64_t merged_end = end;
    uint64_t merged_flags = flags;

    for (;;) {
        int merged_any = 0;
        for (uint32_t i = 0; i < *count; ++i) {
            if (!ranges_overlap(merged_base, merged_end, regions[i].base, regions[i].end)) {
                continue;
            }
            if (regions[i].base < merged_base) {
                merged_base = regions[i].base;
            }
            if (regions[i].end > merged_end) {
                merged_end = regions[i].end;
            }
            merged_flags |= regions[i].flags;
            regions[i] = regions[*count - 1];
            --(*count);
            merged_any = 1;
            break;
        }
        if (!merged_any) {
            break;
        }
    }

    if (*count >= USERIMG_MAX_LAYOUT_REGIONS) {
        return -1;
    }

    regions[*count].base = merged_base;
    regions[*count].end = merged_end;
    regions[*count].flags = merged_flags;
    ++(*count);
    return 0;
}

static void sort_regions_by_base(userimg_layout_region_t *regions, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = i + 1; j < count; ++j) {
            if (regions[j].base < regions[i].base) {
                userimg_layout_region_t tmp = regions[i];
                regions[i] = regions[j];
                regions[j] = tmp;
            }
        }
    }
}

static void unmap_previous_layout(void) {
    for (uint32_t i = 0; i < g_last_mapped_count; ++i) {
        (void)vmm_unmap_range(g_last_mapped[i].base, g_last_mapped[i].size);
    }
    g_last_mapped_count = 0;
}

static void rollback_mapped_layout(void) {
    while (g_last_mapped_count > 0) {
        --g_last_mapped_count;
        (void)vmm_unmap_range(g_last_mapped[g_last_mapped_count].base, g_last_mapped[g_last_mapped_count].size);
    }
}

const char *userimg_result_name(mvos_userimg_result_t rc) {
    switch (rc) {
        case MVOS_USERIMG_OK:
            return "ok";
        case MVOS_USERIMG_ERR_NULL_ARG:
            return "null-arg";
        case MVOS_USERIMG_ERR_ELF:
            return "elf-inspect-failed";
        case MVOS_USERIMG_ERR_LAYOUT:
            return "layout-invalid";
        case MVOS_USERIMG_ERR_MAP:
            return "vmm-map-failed";
        default:
            return "unknown";
    }
}

mvos_userimg_result_t userimg_prepare_embedded_sample(mvos_userimg_report_t *out) {
    if (out == 0) {
        return MVOS_USERIMG_ERR_NULL_ARG;
    }

    *out = (mvos_userimg_report_t){0};

    mvos_elf64_report_t elf_report;
    mvos_elf64_result_t elf_rc = elf64_inspect_image(g_linux_user_elf_sample, g_linux_user_elf_sample_len, &elf_report);
    if (elf_rc != MVOS_ELF64_OK) {
        return MVOS_USERIMG_ERR_ELF;
    }

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)g_linux_user_elf_sample;
    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3 ||
        eh->e_phnum == 0) {
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    uint64_t ph_bytes = 0;
    uint64_t ph_end = 0;
    ph_bytes = (uint64_t)eh->e_phnum * sizeof(elf64_phdr_t);
    if (checked_add_u64(eh->e_phoff, ph_bytes, &ph_end) != 0 || ph_end > g_linux_user_elf_sample_len) {
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    uint64_t src_floor = align_down_u64(elf_report.min_vaddr, MVOS_VMM_PAGE_SIZE);
    uint64_t src_ceil = 0;
    if (align_up_u64(elf_report.max_vaddr, MVOS_VMM_PAGE_SIZE, &src_ceil) != 0 || src_ceil <= src_floor) {
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    const uint64_t mapped_base = 0x0000000000800000ULL;
    uint64_t mapped_limit = 0;
    if (checked_add_u64(mapped_base, src_ceil - src_floor, &mapped_limit) != 0) {
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    userimg_layout_region_t layout[USERIMG_MAX_LAYOUT_REGIONS];
    uint32_t layout_count = 0;
    const elf64_phdr_t *ph = (const elf64_phdr_t *)(g_linux_user_elf_sample + eh->e_phoff);

    for (uint64_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != ELF_PHDR_LOAD || ph[i].p_memsz == 0) {
            continue;
        }

        uint64_t seg_end = 0;
        if (checked_add_u64(ph[i].p_vaddr, ph[i].p_memsz, &seg_end) != 0) {
            return MVOS_USERIMG_ERR_LAYOUT;
        }

        uint64_t seg_floor = align_down_u64(ph[i].p_vaddr, MVOS_VMM_PAGE_SIZE);
        uint64_t seg_ceil = 0;
        if (align_up_u64(seg_end, MVOS_VMM_PAGE_SIZE, &seg_ceil) != 0 || seg_ceil <= seg_floor) {
            return MVOS_USERIMG_ERR_LAYOUT;
        }
        if (seg_floor < src_floor || seg_ceil > src_ceil) {
            return MVOS_USERIMG_ERR_LAYOUT;
        }

        uint64_t offset_floor = seg_floor - src_floor;
        uint64_t offset_ceil = seg_ceil - src_floor;
        uint64_t reloc_floor = 0;
        uint64_t reloc_ceil = 0;
        if (checked_add_u64(mapped_base, offset_floor, &reloc_floor) != 0 ||
            checked_add_u64(mapped_base, offset_ceil, &reloc_ceil) != 0) {
            return MVOS_USERIMG_ERR_LAYOUT;
        }

        uint64_t map_flags = MVOS_VMM_FLAG_USER;
        if ((ph[i].p_flags & ELF_PHDR_FLAG_READ) != 0) {
            map_flags |= MVOS_VMM_FLAG_READ;
        }
        if ((ph[i].p_flags & ELF_PHDR_FLAG_WRITE) != 0) {
            map_flags |= MVOS_VMM_FLAG_WRITE;
        }
        if ((ph[i].p_flags & ELF_PHDR_FLAG_EXEC) != 0) {
            map_flags |= MVOS_VMM_FLAG_EXEC;
        }
        if ((map_flags & (MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_EXEC)) == 0) {
            map_flags |= MVOS_VMM_FLAG_READ;
        }

        if (append_or_merge_region(layout, &layout_count, reloc_floor, reloc_ceil, map_flags) != 0) {
            return MVOS_USERIMG_ERR_LAYOUT;
        }
    }

    if (layout_count == 0) {
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    sort_regions_by_base(layout, layout_count);
    unmap_previous_layout();

    uint64_t mapped_bytes = 0;
    for (uint32_t i = 0; i < layout_count; ++i) {
        uint64_t region_size = layout[i].end - layout[i].base;
        if (vmm_map_range(layout[i].base, region_size, layout[i].flags, "userimg-load") != 0) {
            rollback_mapped_layout();
            return MVOS_USERIMG_ERR_MAP;
        }
        if (g_last_mapped_count >= USERIMG_MAX_LAYOUT_REGIONS) {
            rollback_mapped_layout();
            return MVOS_USERIMG_ERR_LAYOUT;
        }
        g_last_mapped[g_last_mapped_count].base = layout[i].base;
        g_last_mapped[g_last_mapped_count].size = region_size;
        ++g_last_mapped_count;
        mapped_bytes += region_size;
    }

    uint64_t stack_base = 0;
    if (checked_add_u64(mapped_limit, USERIMG_STACK_GAP_BYTES, &stack_base) != 0 ||
        align_up_u64(stack_base, MVOS_VMM_PAGE_SIZE, &stack_base) != 0) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_LAYOUT;
    }
    uint64_t stack_top = 0;
    if (checked_add_u64(stack_base, USERIMG_STACK_SIZE_BYTES, &stack_top) != 0) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_LAYOUT;
    }
    if (vmm_map_range(stack_base,
                      USERIMG_STACK_SIZE_BYTES,
                      MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE | MVOS_VMM_FLAG_USER,
                      "userimg-stack") != 0) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_MAP;
    }
    if (g_last_mapped_count >= USERIMG_MAX_LAYOUT_REGIONS) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_LAYOUT;
    }
    g_last_mapped[g_last_mapped_count].base = stack_base;
    g_last_mapped[g_last_mapped_count].size = USERIMG_STACK_SIZE_BYTES;
    ++g_last_mapped_count;
    mapped_bytes += USERIMG_STACK_SIZE_BYTES;

    if (elf_report.entry < src_floor || elf_report.entry >= src_ceil) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_LAYOUT;
    }
    uint64_t entry_offset = elf_report.entry - src_floor;
    uint64_t mapped_entry = 0;
    if (checked_add_u64(mapped_base, entry_offset, &mapped_entry) != 0) {
        rollback_mapped_layout();
        return MVOS_USERIMG_ERR_LAYOUT;
    }

    out->entry = elf_report.entry;
    out->mapped_entry = mapped_entry;
    out->stack_base = stack_base;
    out->stack_top = stack_top;
    out->stack_size = USERIMG_STACK_SIZE_BYTES;
    out->image_size = elf_report.image_size;
    out->load_segments = elf_report.load_count;
    out->mapped_regions = g_last_mapped_count;
    out->mapped_bytes = mapped_bytes;
    out->min_vaddr = elf_report.min_vaddr;
    out->max_vaddr = elf_report.max_vaddr;
    out->mapped_base = mapped_base;
    out->mapped_limit = mapped_limit;
    return MVOS_USERIMG_OK;
}
