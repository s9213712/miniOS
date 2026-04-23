/* SPDX-License-Identifier: 0BSD */

#ifndef LIMINE_H
#define LIMINE_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal Limine protocol header subset for build-time compatibility. */

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_PTR(TYPE) uint64_t
#else
#  define LIMINE_PTR(TYPE) TYPE
#endif

#define LIMINE_REQUESTS_START_MARKER { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
                                       0x785c6ed015d3e316, 0x181e920a7852b9d9 }
#define LIMINE_REQUESTS_END_MARKER { 0xadc0e0531bb10d03, 0x9572709f31764c62 }

#define LIMINE_BASE_REVISION(N) { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }

#define LIMINE_BASE_REVISION_SUPPORTED(VAR) ((VAR)[2] == 0)

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

#define LIMINE_BOOTLOADER_INFO_REQUEST_ID { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }
#define LIMINE_STACK_SIZE_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x224ef0460a8e8926, 0xe1cb0fc25f46ea3d }
#define LIMINE_HHDM_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }
#define LIMINE_MEMMAP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

struct limine_bootloader_info_response {
    uint64_t revision;
    LIMINE_PTR(char *) name;
    LIMINE_PTR(char *) version;
};

struct limine_bootloader_info_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_bootloader_info_response *) response;
};

struct limine_stack_size_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(void *) response;
    uint64_t stack_size;
};

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_hhdm_response *) response;
};

#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_MEMMAP_RESERVED 1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE 2
#define LIMINE_MEMMAP_ACPI_NVS 3
#define LIMINE_MEMMAP_BAD_MEMORY 4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER 7
#define LIMINE_MEMMAP_RESERVED_MAPPED 8

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    LIMINE_PTR(struct limine_memmap_entry **) entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_memmap_response *) response;
};

#ifdef __cplusplus
}
#endif

#endif
