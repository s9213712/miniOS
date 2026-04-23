#include <mvos/userapp.h>
#include <mvos/userproc.h>
#include <mvos/console.h>
#include <mvos/log.h>
#include <mvos/scheduler.h>
#include <mvos/interrupt.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern void minios_userapp_hello(void);
extern void minios_userapp_ticks(void);
extern void minios_userapp_scheduler(void);

#define MINIOS_USERAPP_STACK_SIZE 4096

/* Phase 20 guard:
 * Current teaching environment loads the kernel as supervisor-only pages.
 * Keeping a kernel-mode fallback avoids unstable crashes while keeping the
 * user-mode entry code for future iterations.
 */
#ifndef MINIOS_PHASE20_USER_MODE
#define MINIOS_PHASE20_USER_MODE 0
#endif

static uint8_t g_userapp_user_stack[MINIOS_USERAPP_STACK_SIZE];

typedef struct {
    const char *name;
    const char *description;
    mvos_userapp_entry_t kernel_entry;
    uint64_t user_entry;
    bool user_mode;
} mvos_userapp_t;

static void userapp_scheduler(void) {
    console_write_string("[userapp] scheduler app\n");
    console_write_string("[userapp] tasks=");
    console_write_u64((uint64_t)scheduler_task_count());
    console_write_string(" current run log snapshot:\n");
    for (uint32_t i = 0; i < scheduler_task_count(); ++i) {
        console_write_string("  ");
        console_write_string(scheduler_task_name(i));
        console_write_string(" runs=");
        console_write_u64(scheduler_task_runs(i));
        console_write_string("\n");
    }
}

static void userapp_fallback_hello(void) {
    console_write_string("user app fallback: hello from kernel mode\n");
}

static void userapp_fallback_ticks(void) {
    console_write_string("user app fallback: ticks=");
    console_write_u64(timer_ticks());
    console_write_string("\n");
}

static const mvos_userapp_t g_userapps[] = {
    {"hello", "print hello from user app (user mode)", userapp_fallback_hello, (uint64_t)minios_userapp_hello, true},
    {"ticks", "print current timer ticks via user syscall", userapp_fallback_ticks, (uint64_t)minios_userapp_ticks, true},
    {"scheduler", "print scheduler snapshot (kernel mode)", userapp_scheduler, 0, false},
};

static const uint64_t g_userapp_count = sizeof(g_userapps) / sizeof(g_userapps[0]);

void userapp_init(void) {
    /* Reserved for future init needs (e.g. loading manifest from VFS or initramfs). */
}

uint64_t userapp_count(void) {
    return g_userapp_count;
}

const char *userapp_name(uint64_t index) {
    if (index >= g_userapp_count) {
        return NULL;
    }
    return g_userapps[index].name;
}

const char *userapp_desc(uint64_t index) {
    if (index >= g_userapp_count) {
        return NULL;
    }
    return g_userapps[index].description;
}

int userapp_run(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }

    for (uint64_t i = 0; i < g_userapp_count; ++i) {
        const mvos_userapp_t *app = &g_userapps[i];
        if (app->name[0] == name[0]) {
            const char *a = app->name;
            const char *b = name;
            while (*a != '\0' && *b != '\0' && *a == *b) {
                ++a;
                ++b;
            }
            if (*a == '\0' && *b == '\0') {
                if (app->user_mode) {
                    if (!MINIOS_PHASE20_USER_MODE) {
                        if (app->kernel_entry == NULL) {
                            return -3;
                        }
                        app->kernel_entry();
                        return 0;
                    }
                    uint64_t return_stack = 0;
                    __asm__ volatile("mov %%rsp, %0" : "=r"(return_stack));
                    userproc_set_return_context((uint64_t)&&user_mode_return, return_stack);
                    userproc_set_current_app_id(i + 1);
                    userproc_enter(app->user_entry,
                                   (uint64_t)(g_userapp_user_stack + MINIOS_USERAPP_STACK_SIZE),
                                   (uint64_t)&&user_mode_return,
                                   return_stack);
user_mode_return:
                    return 0;
                }
                if (app->kernel_entry == NULL) {
                    return -3;
                }
                app->kernel_entry();
                return 0;
            }
        }
    }
    return -2;
}
