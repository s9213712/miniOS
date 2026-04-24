#include <mvos/userimg.h>
#include <mvos/userproc.h>
#include <mvos/vmm.h>
#include <stdlib.h>
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

extern uint64_t g_userproc_running;
extern uint64_t g_userproc_current_app_id;

enum {
    TEST_LINUX_SYSCALL_EXECVE = 59,
};

static uint64_t read_u64(const uint8_t *stack_mem, uint64_t stack_base, uint64_t stack_top, uint64_t addr) {
    if (addr < stack_base || addr + sizeof(uint64_t) > stack_top) {
        return UINT64_MAX;
    }
    uint64_t value = 0;
    memcpy(&value, stack_mem + (addr - stack_base), sizeof(value));
    return value;
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

    const char *argv_demo[] = {"hello_linux_tiny", "--demo"};
    const char *envp_demo[] = {"TERM=minios", "PATH=/usr/bin"};
    uint8_t *stack_mem = calloc(1u, (size_t)report.stack_size);
    if (stack_mem == NULL) {
        fprintf(stderr, "[test_userimg_loader] failed to allocate stack scratch\n");
        return 1;
    }
    mvos_user_stack_layout_t layout;
    int stack_rc = userproc_prepare_exec_stack(
        stack_mem,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        argv_demo,
        2,
        envp_demo,
        2,
        &layout);
    if (stack_rc != 0) {
        fprintf(stderr, "[test_userimg_loader] exec stack prep failed: %s (%d)\n",
                userproc_stack_result_name(stack_rc),
                stack_rc);
        free(stack_mem);
        return 1;
    }
    if (layout.argc != 2 || layout.initial_rsp < report.stack_base || layout.initial_rsp >= report.stack_top) {
        fprintf(stderr, "[test_userimg_loader] invalid stack layout header\n");
        free(stack_mem);
        return 1;
    }
    if (read_u64(stack_mem, report.stack_base, report.stack_top, layout.initial_rsp) != 2ULL) {
        fprintf(stderr, "[test_userimg_loader] expected argc=2 at initial_rsp\n");
        free(stack_mem);
        return 1;
    }

    uint64_t arg0_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user);
    uint64_t arg1_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user + 8ULL);
    uint64_t argn_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.argv_user + 16ULL);
    uint64_t env0_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user);
    uint64_t env1_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user + 8ULL);
    uint64_t envn_ptr = read_u64(stack_mem, report.stack_base, report.stack_top, layout.envp_user + 16ULL);
    if (arg0_ptr == UINT64_MAX || arg1_ptr == UINT64_MAX || env0_ptr == UINT64_MAX || env1_ptr == UINT64_MAX) {
        fprintf(stderr, "[test_userimg_loader] argv/envp pointer read out of range\n");
        free(stack_mem);
        return 1;
    }
    if (argn_ptr != 0 || envn_ptr != 0) {
        fprintf(stderr, "[test_userimg_loader] argv/envp terminator mismatch\n");
        free(stack_mem);
        return 1;
    }
    if (strcmp((const char *)(stack_mem + (arg0_ptr - report.stack_base)), argv_demo[0]) != 0 ||
        strcmp((const char *)(stack_mem + (arg1_ptr - report.stack_base)), argv_demo[1]) != 0 ||
        strcmp((const char *)(stack_mem + (env0_ptr - report.stack_base)), envp_demo[0]) != 0 ||
        strcmp((const char *)(stack_mem + (env1_ptr - report.stack_base)), envp_demo[1]) != 0) {
        fprintf(stderr, "[test_userimg_loader] argv/envp string payload mismatch\n");
        free(stack_mem);
        return 1;
    }
    if (read_u64(stack_mem, report.stack_base, report.stack_top, layout.auxv_user) != 0ULL ||
        read_u64(stack_mem, report.stack_base, report.stack_top, layout.auxv_user + 8ULL) != 0ULL) {
        fprintf(stderr, "[test_userimg_loader] expected auxv terminator pair\n");
        free(stack_mem);
        return 1;
    }
    if (layout.used_bytes == 0 || layout.strings_bytes == 0 || layout.used_bytes < layout.strings_bytes) {
        fprintf(stderr, "[test_userimg_loader] invalid used/strings byte counters\n");
        free(stack_mem);
        return 1;
    }

    uint8_t tiny_stack[64];
    stack_rc = userproc_prepare_exec_stack(
        tiny_stack,
        sizeof(tiny_stack),
        0x1000ULL,
        0x1040ULL,
        argv_demo,
        2,
        envp_demo,
        2,
        &layout);
    if (stack_rc != -5) {
        fprintf(stderr, "[test_userimg_loader] expected tiny stack overflow (-5), got %d\n", stack_rc);
        free(stack_mem);
        return 1;
    }

    stack_rc = userproc_prepare_exec_stack(
        stack_mem,
        report.stack_size,
        report.stack_base,
        report.stack_top,
        NULL,
        1,
        envp_demo,
        2,
        &layout);
    if (stack_rc != -4) {
        fprintf(stderr, "[test_userimg_loader] expected null argv error (-4), got %d\n", stack_rc);
        free(stack_mem);
        return 1;
    }

    const char exec_path[] = "/bin/hello_linux_tiny";
    const char *exec_argv[] = {"hello_linux_tiny", "--host-test", NULL};
    const char *exec_envp[] = {"TERM=minios", NULL};
    g_userproc_running = 1;
    g_userproc_current_app_id = 41;
    uint64_t exec_rc = userproc_dispatch(
        TEST_LINUX_SYSCALL_EXECVE,
        (uint64_t)(uintptr_t)exec_path,
        (uint64_t)(uintptr_t)exec_argv,
        (uint64_t)(uintptr_t)exec_envp);
    if (exec_rc != 1ULL) {
        fprintf(stderr, "[test_userimg_loader] expected execve scaffold success signal (1), got %lld\n",
                (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (g_userproc_running != 0) {
        fprintf(stderr, "[test_userimg_loader] expected execve scaffold to stop current app context\n");
        free(stack_mem);
        return 1;
    }

    g_userproc_running = 1;
    exec_rc = userproc_dispatch(
        TEST_LINUX_SYSCALL_EXECVE,
        (uint64_t)(uintptr_t)"/bad/path",
        (uint64_t)(uintptr_t)exec_argv,
        (uint64_t)(uintptr_t)exec_envp);
    if ((int64_t)exec_rc != -2) {
        fprintf(stderr, "[test_userimg_loader] expected execve ENOENT (-2), got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    if (g_userproc_running != 1) {
        fprintf(stderr, "[test_userimg_loader] expected failed execve to keep running state\n");
        free(stack_mem);
        return 1;
    }

    exec_rc = userproc_dispatch(TEST_LINUX_SYSCALL_EXECVE, 0, 0, 0);
    if ((int64_t)exec_rc != -14) {
        fprintf(stderr, "[test_userimg_loader] expected execve EFAULT (-14), got %lld\n", (long long)exec_rc);
        free(stack_mem);
        return 1;
    }
    free(stack_mem);

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
