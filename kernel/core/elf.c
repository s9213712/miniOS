#include <mvos/elf.h>
#include <mvos/console.h>
#include <stdint.h>

enum {
    ELF_MAGIC0 = 0x7f,
    ELF_MAGIC1 = 'E',
    ELF_MAGIC2 = 'L',
    ELF_MAGIC3 = 'F',
    ELF_CLASS_64 = 2,
    ELF_DATA_LSB = 1,
    ELF_VERSION_CURRENT = 1,
    ELF_MACHINE_X86_64 = 62,
    ELF_TYPE_EXEC = 2,
    ELF_TYPE_DYN = 3,
    ELF_PHDR_LOAD = 1,
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

extern const uint8_t g_linux_user_elf_sample[];
extern const uint64_t g_linux_user_elf_sample_len;

static int checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) {
    uint64_t v = a + b;
    if (v < a) {
        return -1;
    }
    *out = v;
    return 0;
}

const char *elf64_result_name(mvos_elf64_result_t rc) {
    switch (rc) {
        case MVOS_ELF64_OK:
            return "ok";
        case MVOS_ELF64_ERR_NULL_ARG:
            return "null-arg-or-image-too-small";
        case MVOS_ELF64_ERR_MAGIC:
            return "bad-magic";
        case MVOS_ELF64_ERR_CLASS:
            return "bad-class-data-version";
        case MVOS_ELF64_ERR_MACHINE:
            return "unsupported-machine";
        case MVOS_ELF64_ERR_TYPE:
            return "unsupported-type";
        case MVOS_ELF64_ERR_HEADER:
            return "invalid-elf-header";
        case MVOS_ELF64_ERR_PHTAB:
            return "invalid-program-header-table";
        case MVOS_ELF64_ERR_SEG_SIZE:
            return "segment-filesz-gt-memsz";
        case MVOS_ELF64_ERR_SEG_FILE:
            return "segment-file-range-overflow";
        case MVOS_ELF64_ERR_SEG_MEM:
            return "segment-memory-range-overflow";
        case MVOS_ELF64_ERR_NO_LOAD:
            return "no-load-segments";
        default:
            return "unknown";
    }
}

mvos_elf64_result_t elf64_inspect_image(const uint8_t *image, uint64_t size, mvos_elf64_report_t *report) {
    if (image == 0 || report == 0 || size < sizeof(elf64_ehdr_t)) {
        return MVOS_ELF64_ERR_NULL_ARG;
    }

    *report = (mvos_elf64_report_t){0};
    report->image_size = size;

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)image;
    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3) {
        return MVOS_ELF64_ERR_MAGIC;
    }
    if (eh->e_ident[4] != ELF_CLASS_64 || eh->e_ident[5] != ELF_DATA_LSB || eh->e_ident[6] != ELF_VERSION_CURRENT) {
        return MVOS_ELF64_ERR_CLASS;
    }
    if (eh->e_machine != ELF_MACHINE_X86_64) {
        return MVOS_ELF64_ERR_MACHINE;
    }
    if (eh->e_type != ELF_TYPE_EXEC && eh->e_type != ELF_TYPE_DYN) {
        return MVOS_ELF64_ERR_TYPE;
    }
    if (eh->e_ehsize != sizeof(elf64_ehdr_t) || eh->e_phentsize != sizeof(elf64_phdr_t) || eh->e_phnum == 0) {
        return MVOS_ELF64_ERR_HEADER;
    }

    uint64_t ph_bytes = (uint64_t)eh->e_phnum * sizeof(elf64_phdr_t);
    uint64_t ph_end = 0;
    if (checked_add_u64(eh->e_phoff, ph_bytes, &ph_end) != 0 || ph_end > size) {
        return MVOS_ELF64_ERR_PHTAB;
    }

    report->entry = eh->e_entry;
    report->ph_count = eh->e_phnum;
    report->min_vaddr = UINT64_MAX;
    report->min_offset = UINT64_MAX;

    const elf64_phdr_t *ph = (const elf64_phdr_t *)(image + eh->e_phoff);
    for (uint64_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != ELF_PHDR_LOAD) {
            continue;
        }
        if (ph[i].p_filesz > ph[i].p_memsz) {
            return MVOS_ELF64_ERR_SEG_SIZE;
        }

        uint64_t seg_file_end = 0;
        if (checked_add_u64(ph[i].p_offset, ph[i].p_filesz, &seg_file_end) != 0 || seg_file_end > size) {
            return MVOS_ELF64_ERR_SEG_FILE;
        }

        uint64_t seg_mem_end = 0;
        if (checked_add_u64(ph[i].p_vaddr, ph[i].p_memsz, &seg_mem_end) != 0) {
            return MVOS_ELF64_ERR_SEG_MEM;
        }

        if (ph[i].p_vaddr < report->min_vaddr) {
            report->min_vaddr = ph[i].p_vaddr;
        }
        if (seg_mem_end > report->max_vaddr) {
            report->max_vaddr = seg_mem_end;
        }
        if (ph[i].p_offset < report->min_offset) {
            report->min_offset = ph[i].p_offset;
        }
        if (seg_file_end > report->max_offset) {
            report->max_offset = seg_file_end;
        }
        report->load_count++;
    }

    if (report->load_count == 0) {
        return MVOS_ELF64_ERR_NO_LOAD;
    }

    return MVOS_ELF64_OK;
}

void elf_sample_diagnostic(void) {
    mvos_elf64_report_t report;
    mvos_elf64_result_t rc = elf64_inspect_image(g_linux_user_elf_sample, g_linux_user_elf_sample_len, &report);
    if (rc != MVOS_ELF64_OK) {
        console_write_string("[elf] sample inspect failed: ");
        console_write_string(elf64_result_name(rc));
        console_write_string("\n");
        return;
    }

    console_write_string("[elf] sample inspect ok\n");
    console_write_string("[elf] image_size=");
    console_write_u64(report.image_size);
    console_write_string(" entry=");
    console_write_u64(report.entry);
    console_write_string("\n");
    console_write_string("[elf] ph=");
    console_write_u64(report.ph_count);
    console_write_string(" load=");
    console_write_u64(report.load_count);
    console_write_string(" vaddr=[");
    console_write_u64(report.min_vaddr);
    console_write_string(",");
    console_write_u64(report.max_vaddr);
    console_write_string(")\n");
    console_write_string("[elf] file_range=[");
    console_write_u64(report.min_offset);
    console_write_string(",");
    console_write_u64(report.max_offset);
    console_write_string(")\n");
}
