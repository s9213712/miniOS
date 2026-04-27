#include <mvos/elf.h>
#include <mvos/elf64_internal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t g_linux_user_elf_sample[];
extern const uint64_t g_linux_user_elf_sample_len;

/* Host test stubs for elf_sample_diagnostic link-time references. */
void console_write_string(const char *str) {
    (void)str;
}

void console_write_u64(uint64_t value) {
    (void)value;
}

static int expect_result(const uint8_t *image,
                         uint64_t size,
                         mvos_elf64_result_t expect,
                         const char *label) {
    mvos_elf64_report_t report;
    mvos_elf64_result_t rc = elf64_inspect_image(image, size, &report);
    if (rc != expect) {
        fprintf(stderr,
                "[test_elf_inspect] %s expected %s (%d), got %s (%d)\n",
                label,
                elf64_result_name(expect),
                (int)expect,
                elf64_result_name(rc),
                (int)rc);
        return 1;
    }
    return 0;
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

    uint8_t *scratch = malloc((size_t)g_linux_user_elf_sample_len);
    if (scratch == NULL) {
        fprintf(stderr, "[test_elf_inspect] malloc failed for scratch buffer\n");
        return 1;
    }
    memcpy(scratch, g_linux_user_elf_sample, (size_t)g_linux_user_elf_sample_len);

    mvos_elf64_ehdr_t *eh = (mvos_elf64_ehdr_t *)scratch;
    eh->e_phentsize = 0;
    if (expect_result(scratch, g_linux_user_elf_sample_len, MVOS_ELF64_ERR_HEADER, "bad e_phentsize") != 0) {
        free(scratch);
        return 1;
    }

    memcpy(scratch, g_linux_user_elf_sample, (size_t)g_linux_user_elf_sample_len);
    eh = (mvos_elf64_ehdr_t *)scratch;
    eh->e_phoff = g_linux_user_elf_sample_len - sizeof(mvos_elf64_phdr_t) + 1;
    if (expect_result(scratch, g_linux_user_elf_sample_len, MVOS_ELF64_ERR_PHTAB, "truncated program header table") != 0) {
        free(scratch);
        return 1;
    }

    free(scratch);
    return 0;
}
