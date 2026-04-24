#pragma once

#include <stdint.h>

typedef enum {
    MVOS_ELF64_OK = 0,
    MVOS_ELF64_ERR_NULL_ARG = -1,
    MVOS_ELF64_ERR_MAGIC = -2,
    MVOS_ELF64_ERR_CLASS = -3,
    MVOS_ELF64_ERR_MACHINE = -4,
    MVOS_ELF64_ERR_TYPE = -5,
    MVOS_ELF64_ERR_HEADER = -6,
    MVOS_ELF64_ERR_PHTAB = -7,
    MVOS_ELF64_ERR_SEG_SIZE = -8,
    MVOS_ELF64_ERR_SEG_FILE = -9,
    MVOS_ELF64_ERR_SEG_MEM = -10,
    MVOS_ELF64_ERR_NO_LOAD = -11,
} mvos_elf64_result_t;

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

mvos_elf64_result_t elf64_inspect_image(const uint8_t *image, uint64_t size, mvos_elf64_report_t *report);
const char *elf64_result_name(mvos_elf64_result_t rc);
void elf_sample_diagnostic(void);
