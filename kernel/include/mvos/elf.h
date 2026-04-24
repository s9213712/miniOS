#pragma once

#include <stdint.h>

typedef struct {
    uint64_t entry;
    uint64_t image_size;
    uint64_t ph_count;
    uint64_t load_count;
    uint64_t min_vaddr;
    uint64_t max_vaddr;
    uint64_t min_offset;
    uint64_t max_offset;
} mvos_elf64_report_t;

int elf64_inspect_image(const uint8_t *image, uint64_t size, mvos_elf64_report_t *report);
void elf_sample_diagnostic(void);
