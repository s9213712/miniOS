#include <mvos/userimg.h>
#include <mvos/userproc.h>
#include <mvos/vmm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Host test stubs for elf.c diagnostic symbols. */
void console_write_string(const char *str) {
    (void)str;
}

void console_write_u64(uint64_t value) {
    (void)value;
}

void console_write_char(char ch) {
    (void)ch;
}

uint64_t timer_ticks(void) {
    return 0;
}

void userproc_enter_asm(uint64_t entry, uint64_t user_stack) {
    (void)entry;
    (void)user_stack;
}

int main(void) {
    vmm_init(0);
    vmm_user_heap_init(0x400000ULL, 0x2000ULL, 0x400000ULL);

    uint32_t base_regions = vmm_region_count();

    mvos_userimg_report_t report;
    if (userimg_prepare_embedded_sample(&report) != MVOS_USERIMG_OK) {
        fprintf(stderr, "[test_userimg_loader] prepare_embedded_sample failed\n");
        return 1;
    }
    if (report.entry == 0 || report.mapped_entry == 0) {
        fprintf(stderr, "[test_userimg_loader] invalid entry values\n");
        return 1;
    }
    if (report.load_segments < 1 || report.mapped_regions < 1) {
        fprintf(stderr, "[test_userimg_loader] expected non-zero load/mapped region counts\n");
        return 1;
    }
    if (report.mapped_base >= report.mapped_limit) {
        fprintf(stderr, "[test_userimg_loader] invalid mapped range\n");
        return 1;
    }
    if (vmm_region_count() <= base_regions) {
        fprintf(stderr, "[test_userimg_loader] expected additional VMM regions after load prepare\n");
        return 1;
    }
    if (report.stack_base >= report.stack_top || report.stack_size == 0 || report.stack_top - report.stack_base != report.stack_size) {
        fprintf(stderr, "[test_userimg_loader] invalid stack report\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_entry, report.stack_top) != 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run success\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_entry, report.stack_top - 1ULL) == 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run to reject unaligned stack top\n");
        return 1;
    }
    if (userproc_handoff_dry_run(report.mapped_limit + 0x1000ULL, report.stack_top) == 0) {
        fprintf(stderr, "[test_userimg_loader] expected handoff dry-run to reject invalid entry\n");
        return 1;
    }

    uint32_t regions_after_first = vmm_region_count();
    int found_userimg = 0;
    int found_stack = 0;
    for (uint32_t i = 0; i < regions_after_first; ++i) {
        mvos_vmm_region_info_t info;
        if (vmm_region_at(i, &info) != 0) {
            continue;
        }
        if (strcmp(info.tag, "userimg-load") == 0) {
            found_userimg = 1;
            if ((info.flags & MVOS_VMM_FLAG_USER) == 0) {
                fprintf(stderr, "[test_userimg_loader] expected user flag on userimg region\n");
                return 1;
            }
        }
        if (strcmp(info.tag, "userimg-stack") == 0) {
            found_stack = 1;
            if ((info.flags & MVOS_VMM_FLAG_USER) == 0 || (info.flags & MVOS_VMM_FLAG_WRITE) == 0) {
                fprintf(stderr, "[test_userimg_loader] expected user writable stack region\n");
                return 1;
            }
            if (info.base != report.stack_base || info.size != report.stack_size) {
                fprintf(stderr, "[test_userimg_loader] stack region mismatch with report\n");
                return 1;
            }
        }
    }
    if (!found_userimg) {
        fprintf(stderr, "[test_userimg_loader] missing userimg-load region tag\n");
        return 1;
    }
    if (!found_stack) {
        fprintf(stderr, "[test_userimg_loader] missing userimg-stack region tag\n");
        return 1;
    }

    mvos_userimg_report_t report2;
    if (userimg_prepare_embedded_sample(&report2) != MVOS_USERIMG_OK) {
        fprintf(stderr, "[test_userimg_loader] second prepare_embedded_sample failed\n");
        return 1;
    }
    if (vmm_region_count() != regions_after_first) {
        fprintf(stderr, "[test_userimg_loader] region count changed after idempotent reload\n");
        return 1;
    }
    if (report.mapped_regions != report2.mapped_regions || report.mapped_entry != report2.mapped_entry) {
        fprintf(stderr, "[test_userimg_loader] report mismatch between repeated loads\n");
        return 1;
    }

    printf("[test_userimg_loader] PASS regions=%u mapped=%llu stack=%llu\n",
           regions_after_first,
           (unsigned long long)report.mapped_regions,
           (unsigned long long)report.stack_size);
    return 0;
}
