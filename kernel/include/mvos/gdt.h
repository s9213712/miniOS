#pragma once

#include <stdint.h>

#define MINIOS_GDT_SELECTOR_KERNEL_CODE 0x08
#define MINIOS_GDT_SELECTOR_KERNEL_DATA 0x10
#define MINIOS_GDT_SELECTOR_USER_CODE  0x1B
#define MINIOS_GDT_SELECTOR_USER_DATA  0x23
#define MINIOS_GDT_SELECTOR_TSS        0x28

void gdt_init(void);
uint64_t gdt_get_rsp0(void);
void gdt_set_rsp0(uint64_t rsp0);
