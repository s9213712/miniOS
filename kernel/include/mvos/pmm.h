#pragma once

#include <stddef.h>
#include <stdint.h>

struct limine_memmap_response;

void pmm_init(const struct limine_memmap_response *response, uint64_t hhdm_offset);
uint64_t pmm_free_pages(void);

void *pmm_allocate_pages(uint64_t page_count);
void *pmm_alloc(size_t bytes);
