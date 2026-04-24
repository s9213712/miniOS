#include <mvos/elf.h>
#include <stdint.h>
#include <stdio.h>

extern const uint8_t g_linux_user_elf_sample[];
extern const uint64_t g_linux_user_elf_sample_len;

/* Host test stubs for elf_sample_diagnostic link-time references. */
void console_write_string(const char *str) {
    (void)str;
}

void console_write_u64(uint64_t value) {
    (void)value;
}

int main(void) {
    mvos_elf64_report_t report;
    mvos_elf64_result_t rc = elf64_inspect_image(g_linux_user_elf_sample, g_linux_user_elf_sample_len, &report);
    if (rc != MVOS_ELF64_OK) {
        fprintf(stderr, "[test_elf_inspect] inspect failed: %s (%d)\n", elf64_result_name(rc), (int)rc);
        return 1;
    }

    if (report.entry == 0) {
        fprintf(stderr, "[test_elf_inspect] invalid entry=0\n");
        return 1;
    }
    if (report.load_count < 1) {
        fprintf(stderr, "[test_elf_inspect] expected load_count >= 1, got %llu\n",
                (unsigned long long)report.load_count);
        return 1;
    }
    if (report.min_vaddr >= report.max_vaddr) {
        fprintf(stderr, "[test_elf_inspect] invalid vaddr range [%llu,%llu)\n",
                (unsigned long long)report.min_vaddr,
                (unsigned long long)report.max_vaddr);
        return 1;
    }
    if (report.min_offset >= report.max_offset) {
        fprintf(stderr, "[test_elf_inspect] invalid file range [%llu,%llu)\n",
                (unsigned long long)report.min_offset,
                (unsigned long long)report.max_offset);
        return 1;
    }

    printf("[test_elf_inspect] PASS entry=%llu ph=%llu load=%llu\n",
           (unsigned long long)report.entry,
           (unsigned long long)report.ph_count,
           (unsigned long long)report.load_count);
    return 0;
}
