#include <mvos/scheduler.h>
#include <mvos/log.h>
#include <stdbool.h>
#include <stddef.h>

/* Tiny teaching scheduler:
 * no preemption or context save, only cooperative hooks driven by timer cadence.
 */
#define MAX_TASKS 4
#define TASK_SWITCH_INTERVAL 100

struct task {
    /* Human-readable name for smoke/diagnostic output. */
    const char *name;
    /* Task entry point executed on each scheduler slice. */
    mvos_task_fn entry;
    /* Number of times the task function has been called. */
    uint64_t runs;
    /* Whether this slot participates in round-robin selection. */
    bool active;
};

/* Global scheduler state.
 * Kept in a single translation unit for educational clarity and minimal complexity.
 */
static struct task g_tasks[MAX_TASKS];
static uint32_t g_task_count;
static uint32_t g_current_task;
static uint64_t g_ticks;
static uint64_t g_ticks_until_switch;

void scheduler_init(void) {
    /* Clear state on each boot so tests can depend on deterministic task ids and counters. */
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        g_tasks[i] = (struct task){0};
    }
    g_task_count = 0;
    g_current_task = 0;
    g_ticks = 0;
    g_ticks_until_switch = TASK_SWITCH_INTERVAL;
}

int scheduler_add_task(const char *name, mvos_task_fn entry) {
    /* Guard compile-time test paths from undefined behavior:
     * cannot register null entry, nameless tasks, or overflow past MAX_TASKS.
     */
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
    /* Called from timer ISR only for coarse preemption scheduling intent.
     * Actual dispatch still happens from main loop in scheduler_run_once().
     */
    g_ticks += 1;
    if (g_task_count <= 1) {
        return;
    }
    if (g_ticks_until_switch > 0) {
        --g_ticks_until_switch;
    }
}

void scheduler_run_once(void) {
    /* One scheduler slice:
     * pick a task only when tick budget reaches zero; otherwise continue current task.
     */
    if (g_task_count == 0 || g_tasks[g_current_task].entry == NULL) {
        return;
    }

    if (g_ticks_until_switch == 0 && g_task_count > 1) {
        g_ticks_until_switch = TASK_SWITCH_INTERVAL;
        uint32_t next = (g_current_task + 1) % g_task_count;
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
    /* Exported for teaching tasks to print timebase correlations. */
    return g_ticks;
}
