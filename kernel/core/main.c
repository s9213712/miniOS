#include <mvos/serial.h>
#include <mvos/log.h>
#include <mvos/panic.h>
#include <mvos/gdt.h>
#include <mvos/idt.h>
#include <mvos/pmm.h>
#include <mvos/heap.h>
#include <mvos/limine.h>
#include <mvos/interrupt.h>
#include <mvos/console.h>
#include <mvos/scheduler.h>
#include <mvos/vfs.h>
#include <mvos/vmm.h>
#include <mvos/shell.h>
#include <mvos/keyboard.h>
#include <mvos/userapp.h>
#include <mvos/userproc.h>
#include <stdint.h>

/* Limine request section markers are grouped as required for bootloader discovery.
 * Smoke tests also verify that request_start is ordered before request_end.
 */
volatile uint64_t request_start[4]
    __attribute__((used, section(".requests.start"))) = LIMINE_REQUESTS_START_MARKER;

static volatile struct limine_bootloader_info_request bootloader_info_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile struct limine_stack_size_request stack_size_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .response = NULL,
    .stack_size = 1024 * 16
};

static volatile struct limine_hhdm_request hhdm_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile struct limine_memmap_request memmap_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

static volatile struct limine_framebuffer_request framebuffer_request
    __attribute__((used, section(".requests"))) = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

volatile uint64_t request_end[2]
    __attribute__((used, section(".requests.end"))) = LIMINE_REQUESTS_END_MARKER;

/* Fault injection is compile-time only so normal boot remains deterministic.
 * When enabled, each helper intentionally executes only the requested architecture trap.
 */
#ifdef MINIOS_FAULT_TEST_DIVIDE_BY_ZERO
static void trigger_divide_by_zero_fault(void) {
    __asm__ volatile(
        "mov $1, %%rax\n\t"
        "xor %%rdx, %%rdx\n\t"
        "mov $0, %%rbx\n\t"
        "div %%rbx\n\t"
        :
        :
        : "rax", "rbx", "rdx", "cc");
}
#endif

#ifdef MINIOS_FAULT_TEST_INVALID_OPCODE
static void trigger_invalid_opcode_fault(void) {
    __asm__ volatile(".byte 0x0f, 0xff");
}
#endif

#ifdef MINIOS_FAULT_TEST_GP_FAULT
static void trigger_general_protection_fault(void) {
    __asm__ volatile(
        "mov $0x28, %ax\n\t"
        "mov %ax, %gs\n\t"
        :
        :
        : "ax");
}
#endif

#ifdef MINIOS_FAULT_TEST_PAGE_FAULT
static void trigger_page_fault(void) {
    volatile uint64_t *bad = (volatile uint64_t *)0;
    *bad = 0x12345678;
}
#endif

/* Phase-5 cooperative example tasks.
 * Output cadence is intentionally sparse to reduce QEMU serial noise.
 */
static void task_a(uint64_t tick) {
    if ((tick & 0xff) != 0) {
        return;
    }
    static uint64_t last_seen_tick = UINT64_MAX;
    if (tick == last_seen_tick) {
        return;
    }
    last_seen_tick = tick;

    klog("[sched] task-a tick=");
    klog_u64(tick);
    klog(" run=");
    klog_u64((uint64_t)scheduler_ticks());
    klogln("");
}

static void task_b(uint64_t tick) {
    if ((tick & 0xfff) != 0) {
        return;
    }
    klog("[sched] task-b tick=");
    klog_u64(tick);
    klogln("");
}

void kmain(void) {
    /* Phase-1: bring up the serial path first so every later diagnostic is visible.
     * Framebuffer text output is enabled for mirroring once HHDM is confirmed.
     */
    serial_init();
    console_init();
    klogln("MiniOS Phase 3 bootstrap");
    klogln("boot banner: kernel entering C");
    klogln("hello from kernel");

#ifdef MINIOS_PANIC_TEST
    panic("panic test path enabled");
#endif

    /* Phase-2 / Phase-3: install CPU tables and timer, then initialize memory.
     * These are required preconditions for fault handling and all later subsystems.
     */
    gdt_init();
    idt_init();
    userproc_syscall_init();
    timer_init(100);
    userapp_init();
    klogln("gdt/idt initialized");
    klog("[phase3] timer ticks=");
    klog_u64(timer_ticks());
    klogln("[phase3] timer enabled");

    /* Fail-fast on missing bootloader responses to make protocol changes obvious
     * during early boot smoke checks.
     */
    if (hhdm_request.response == NULL) {
        panic("missing HHDM request response");
    }
    if (memmap_request.response == NULL) {
        panic("missing memmap request response");
    }
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count == 0) {
        klogln("[framebuffer] request unavailable, fallback to default VGA text mirror only");
    } else {
        klog("[framebuffer] response count=");
        klog_u64((uint64_t)framebuffer_request.response->framebuffer_count);
        klog("[framebuffer] first addr=");
        if (framebuffer_request.response->framebuffers && framebuffer_request.response->framebuffers[0]) {
            const struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
            klog_u64((uint64_t)fb->address);
            klog("[framebuffer] size=");
            klog_u64(fb->width);
            klog("x");
            klog_u64(fb->height);
            klog(" pitch=");
            klog_u64(fb->pitch);
            klog(" bpp=");
            klog_u64((uint64_t)fb->bpp);
            klogln("");
            if (fb->bpp == 24u || fb->bpp == 32u) {
                console_enable_graphics_framebuffer(
                    hhdm_request.response->offset,
                    (uint64_t)fb->address,
                    fb->width,
                    fb->height,
                    fb->pitch,
                    fb->bpp,
                    fb->memory_model,
                    fb->red_mask_size,
                    fb->red_mask_shift,
                    fb->green_mask_size,
                    fb->green_mask_shift,
                    fb->blue_mask_size,
                    fb->blue_mask_shift);
            } else {
                klog("[framebuffer] skipping graphics output, unsupported bpp=");
                klog_u64((uint64_t)fb->bpp);
                klogln("");
            }
        } else {
            klogln("[framebuffer] missing first framebuffer pointer");
        }
    }
    console_enable_framebuffer_text_mode(hhdm_request.response->offset);
    if (console_graphics_enabled()) {
        console_draw_gui_boot_window();
    }

    klogln("[phase3] collecting boot memory map");
    klog("hhdm_offset=");
    klog_u64(hhdm_request.response->offset);
    klogln("");
    pmm_init(memmap_request.response, hhdm_request.response->offset);
    klog("free pages: ");
    klog_u64((uint64_t)pmm_free_pages());
    klogln("");
    heap_init();

    void *page = pmm_allocate_pages(1);
    if (!page) {
        panic("pmm_allocate_pages(1) failed in phase 3");
    }
    klog("pmm_allocate_pages(1) = ");
    klog_u64((uint64_t)page);
    klogln("");

    void *heap_block = kmalloc(256);
    if (!heap_block) {
        panic("kmalloc(256) failed in phase 3");
    }
    klog("kmalloc(256) = ");
    klog_u64((uint64_t)heap_block);
    klogln("");
    klog("[phase2] allocator stats free=");
    klog_u64(pmm_free_pages());
    klog(" total=");
    klog_u64(pmm_total_pages());
    klog(" allocated=");
    klog_u64(pmm_allocated_pages());
    klogln("");
    kfree(heap_block);
    klog("kfree called on ");
    klog_u64((uint64_t)heap_block);
    klogln("");
    klogln("[phase3] memory allocator ready");

    /* Phase-36: initialize virtual-memory metadata layer and user brk arena.
     * This is still a teaching scaffold (metadata + bounds), but keeps
     * Linux ABI brk state consistent and testable.
     */
    vmm_init(hhdm_request.response->offset);
    vmm_user_heap_init(0x0000400000200000ULL, 0x2000ULL, 0x400000ULL);
    klog("[vmm] regions=");
    klog_u64((uint64_t)vmm_region_count());
    klog(" user_brk=");
    klog_u64(vmm_user_brk_get());
    klog(" limit=");
    klog_u64(vmm_user_brk_limit());
    klogln("");

    /* Phase-6: initramfs-style VFS smoke diagnostics.
     * This validates file-open/read/list behavior before scheduling starts.
     */
    vfs_diagnostic_list();
    vfs_diagnostic_read_file();
    vfs_diagnostic_missing();

    /* Phase-5: register teaching tasks and switch output cadence.
     * Timer interrupts drive tick advancement; scheduler does cooperative dispatch.
     */
    scheduler_init();
    if (scheduler_add_task("task-a", task_a) < 0 || scheduler_add_task("task-b", task_b) < 0) {
        panic("scheduler init failed");
    }
    klogln("[phase5] scheduler ready");

#ifdef MINIOS_PHASE20_DEMO
    klogln("[phase20] demo: running user app hello");
    if (userapp_run("hello") != 0) {
        klogln("[phase20] demo user app hello failed");
    } else {
        klogln("[phase20] demo user app hello done");
    }
    klogln("[phase20] demo: running user app ticks");
    if (userapp_run("ticks") != 0) {
        klogln("[phase20] demo user app ticks failed");
    } else {
        klogln("[phase20] demo user app ticks done");
    }
#endif

#ifdef MINIOS_EXECVE_DEMO
    klogln("[execve-demo] running linux-abi probe");
    if (userapp_run("linux-abi") != 0) {
        klogln("[execve-demo] linux-abi probe failed");
    } else {
        klogln("[execve-demo] linux-abi probe done");
    }
#endif

#ifdef MINIOS_ENABLE_SHELL
    /* Optional interactive phase for manual verification.
     * Kept off by default to avoid changing CI smoke timeline.
     */
    keyboard_init();
    klogln("[phase4] entering interactive shell (serial only)");
    shell_run();
#endif

#ifdef MINIOS_FAULT_TEST_DIVIDE_BY_ZERO
    klogln("triggering divide-by-zero fault test");
    trigger_divide_by_zero_fault();
#endif

#ifdef MINIOS_FAULT_TEST_INVALID_OPCODE
    klogln("triggering invalid opcode fault test");
    trigger_invalid_opcode_fault();
#endif

#ifdef MINIOS_FAULT_TEST_GP_FAULT
    klogln("triggering general-protection fault test");
    trigger_general_protection_fault();
#endif

#ifdef MINIOS_FAULT_TEST_PAGE_FAULT
    klogln("triggering page-fault test");
    trigger_page_fault();
#endif

    /* Final loop keeps kernel alive: wake on interrupt, run one scheduler slice.
     * Keeps deterministic order for smoke assertions.
     */
    for (;;) {
        __asm__ volatile("hlt");
        scheduler_run_once();
    }
}
