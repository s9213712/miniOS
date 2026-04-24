#pragma once

#include <stdint.h>

enum {
    MVOS_ELF64_MAGIC0 = 0x7f,
    MVOS_ELF64_MAGIC1 = 'E',
    MVOS_ELF64_MAGIC2 = 'L',
    MVOS_ELF64_MAGIC3 = 'F',
    MVOS_ELF64_CLASS_64 = 2,
    MVOS_ELF64_DATA_LSB = 1,
    MVOS_ELF64_VERSION_CURRENT = 1,
    MVOS_ELF64_MACHINE_X86_64 = 62,
    MVOS_ELF64_TYPE_EXEC = 2,
    MVOS_ELF64_TYPE_DYN = 3,
    MVOS_ELF64_PHDR_LOAD = 1,
    MVOS_ELF64_PHDR_FLAG_EXEC = 1,
    MVOS_ELF64_PHDR_FLAG_WRITE = 2,
    MVOS_ELF64_PHDR_FLAG_READ = 4,
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
} mvos_elf64_ehdr_t;

typedef struct __attribute__((packed)) {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} mvos_elf64_phdr_t;
