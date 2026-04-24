#include <mvos/vmm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    vmm_init(0);

    if (vmm_map_range(0x1000ULL, MVOS_VMM_PAGE_SIZE * 2ULL, MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE, "kheap") != 0) {
        fprintf(stderr, "[test_vmm_basic] initial map failed\n");
        return 1;
    }
    if (vmm_user_range_check(0x1000ULL, 16, MVOS_VMM_FLAG_READ) == 0) {
        fprintf(stderr, "[test_vmm_basic] kernel-only region passed user range check\n");
        return 1;
    }
    if (vmm_region_count() != 1) {
        fprintf(stderr, "[test_vmm_basic] expected 1 region after first map\n");
        return 1;
    }

    if (vmm_map_range(0x1800ULL, MVOS_VMM_PAGE_SIZE, MVOS_VMM_FLAG_READ, "bad-align") != -2) {
        fprintf(stderr, "[test_vmm_basic] expected alignment error for unaligned vaddr\n");
        return 1;
    }
    if (vmm_map_range(0x2000ULL, MVOS_VMM_PAGE_SIZE, MVOS_VMM_FLAG_READ, "overlap") != -4) {
        fprintf(stderr, "[test_vmm_basic] expected overlap rejection\n");
        return 1;
    }

    if (vmm_unmap_range(0x1000ULL, MVOS_VMM_PAGE_SIZE * 2ULL) != 0) {
        fprintf(stderr, "[test_vmm_basic] unmap failed\n");
        return 1;
    }
    if (vmm_region_count() != 0) {
        fprintf(stderr, "[test_vmm_basic] expected 0 regions after unmap\n");
        return 1;
    }

    vmm_user_heap_init(0x400000ULL, 0x1000ULL, 0x5000ULL);
    if (vmm_user_brk_get() != 0x400000ULL) {
        fprintf(stderr, "[test_vmm_basic] unexpected initial brk=%llu\n", (unsigned long long)vmm_user_brk_get());
        return 1;
    }
    if (vmm_user_brk_limit() != 0x405000ULL) {
        fprintf(stderr, "[test_vmm_basic] unexpected brk limit=%llu\n", (unsigned long long)vmm_user_brk_limit());
        return 1;
    }

    if (vmm_user_brk_set(0x403000ULL) != 0x403000ULL) {
        fprintf(stderr, "[test_vmm_basic] failed to advance brk inside range\n");
        return 1;
    }
    if (vmm_user_brk_set(0x406000ULL) != 0x403000ULL) {
        fprintf(stderr, "[test_vmm_basic] expected brk to remain unchanged when over limit\n");
        return 1;
    }

    if (vmm_region_count() < 1) {
        fprintf(stderr, "[test_vmm_basic] expected mapped user-brk region(s)\n");
        return 1;
    }

    int saw_user_brk = 0;
    uint32_t count = vmm_region_count();
    for (uint32_t i = 0; i < count; ++i) {
        mvos_vmm_region_info_t info;
        if (vmm_region_at(i, &info) != 0) {
            continue;
        }
        if (strcmp(info.tag, "user-brk") == 0) {
            saw_user_brk = 1;
        }
    }
    if (!saw_user_brk) {
        fprintf(stderr, "[test_vmm_basic] expected user-brk tagged region\n");
        return 1;
    }
    if (vmm_user_range_check(0x400000ULL, 0x1000ULL, MVOS_VMM_FLAG_READ | MVOS_VMM_FLAG_WRITE) != 0) {
        fprintf(stderr, "[test_vmm_basic] expected user-brk range check success\n");
        return 1;
    }
    if (vmm_user_range_check(0x400000ULL, 0x6000ULL, MVOS_VMM_FLAG_READ) == 0) {
        fprintf(stderr, "[test_vmm_basic] over-limit user range unexpectedly passed\n");
        return 1;
    }

    printf("[test_vmm_basic] PASS\n");
    return 0;
}
