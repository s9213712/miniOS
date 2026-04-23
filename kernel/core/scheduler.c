#include <mvos/scheduler.h>
#include <mvos/log.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_TASKS 4
#define TASK_SWITCH_INTERVAL 100

struct task {
    const char *name;
    mvos_task_fn entry;
    uint64_t runs;
    bool active;
};

static struct task g_tasks[MAX_TASKS];
static uint32_t g_task_count;
static uint32_t g_current_task;
static uint64_t g_ticks;
static uint64_t g_ticks_until_switch;

void scheduler_init(void) {
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        g_tasks[i] = (struct task){0};
    }
    g_task_count = 0;
    g_current_task = 0;
    g_ticks = 0;
    g_ticks_until_switch = TASK_SWITCH_INTERVAL;
}

int scheduler_add_task(const char *name, mvos_task_fn entry) {
    if (entry == NULL || name == NULL || g_task_count >= MAX_TASKS) {
        return -1;
    }
    g_tasks[g_task_count] = (struct task){
        .name = name,
        .entry = entry,
        .runs = 0,
        .active = true,
    };
    return (int)g_task_count++;
}

void scheduler_on_timer_tick(void) {
    g_ticks += 1;
    if (g_task_count <= 1) {
        return;
    }
    if (g_ticks_until_switch > 0) {
        --g_ticks_until_switch;
    }
}

void scheduler_run_once(void) {
    if (g_task_count == 0 || g_tasks[g_current_task].entry == NULL) {
        return;
    }

    if (g_ticks_until_switch == 0 && g_task_count > 1) {
        g_ticks_until_switch = TASK_SWITCH_INTERVAL;
        uint32_t next = (g_current_task + 1) % g_task_count;
        g_tasks[next].runs += 0;
        if (!g_tasks[next].active) {
            for (uint32_t i = 0; i < g_task_count; ++i) {
                uint32_t cand = (g_current_task + 1 + i) % g_task_count;
                if (g_tasks[cand].active) {
                    next = cand;
                    break;
                }
            }
        }
        g_current_task = next;
    }

    g_tasks[g_current_task].runs += 1;
    if (g_tasks[g_current_task].entry != NULL) {
        g_tasks[g_current_task].entry(g_ticks);
    }
}

uint64_t scheduler_ticks(void) {
    return g_ticks;
}
