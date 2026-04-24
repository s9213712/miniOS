#pragma once

#include <stdint.h>

typedef enum {
    MVOS_USERIMG_OK = 0,
    MVOS_USERIMG_ERR_NULL_ARG = -1,
    MVOS_USERIMG_ERR_ELF = -2,
    MVOS_USERIMG_ERR_LAYOUT = -3,
    MVOS_USERIMG_ERR_MAP = -4,
} mvos_userimg_result_t;

typedef struct {
    uint64_t entry;
    uint64_t mapped_entry;
    uint64_t stack_base;
    uint64_t stack_top;
    uint64_t stack_size;
    uint64_t image_size;
    uint64_t load_segments;
    uint64_t mapped_regions;
    uint64_t mapped_bytes;
    uint64_t min_vaddr;
    uint64_t max_vaddr;
    uint64_t mapped_base;
    uint64_t mapped_limit;
} mvos_userimg_report_t;

mvos_userimg_result_t userimg_prepare_image(const uint8_t *image, uint64_t image_len, mvos_userimg_report_t *out);
mvos_userimg_result_t userimg_prepare_embedded_sample(mvos_userimg_report_t *out);
const char *userimg_result_name(mvos_userimg_result_t rc);
