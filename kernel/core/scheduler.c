#include <mvos/scheduler.h>
#include <mvos/log.h>
#include <stdbool.h>
#include <stddef.h>

/* Tiny teaching scheduler:
 * no preemption or context save, only cooperative hooks driven by timer cadence.
 */
#define MAX_TASKS 8
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
static volatile uint64_t g_ticks;
static volatile uint64_t g_ticks_until_switch;

static int streq(const char *a, const char *b) {
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int scheduler_any_active_task(void) {
    for (uint32_t i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].active && g_tasks[i].entry != NULL) {
            return 1;
        }
    }
    return 0;
}

static int scheduler_find_next_active_from(uint32_t start_index, uint32_t *out_index) {
    if (g_task_count == 0) {
        return -1;
    }
    for (uint32_t i = 0; i < g_task_count; ++i) {
        uint32_t cand = (start_index + i) % g_task_count;
        if (g_tasks[cand].active && g_tasks[cand].entry != NULL) {
            *out_index = cand;
            return 0;
        }
    }
    return -1;
}

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
    if (entry == NULL) {
        klogln("[scheduler] rejected task with null entry");
        return -1;
    }
    if (name == NULL || name[0] == '\0') {
        klogln("[scheduler] rejected task with empty name");
        return -1;
    }
    if (g_task_count >= MAX_TASKS) {
        klog("[scheduler] MAX_TASKS reached, dropping task: ");
        klog(name);
        klogln("");
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
    if (g_task_count == 0 || !scheduler_any_active_task()) {
        return;
    }

    if (g_current_task >= g_task_count || !g_tasks[g_current_task].active || g_tasks[g_current_task].entry == NULL) {
        uint32_t recovered = 0;
        if (scheduler_find_next_active_from(0, &recovered) != 0) {
            return;
        }
        g_current_task = recovered;
        g_ticks_until_switch = TASK_SWITCH_INTERVAL;
    }

    if (g_ticks_until_switch == 0 && g_task_count > 1) {
        g_ticks_until_switch = TASK_SWITCH_INTERVAL;
        uint32_t next = 0;
        if (scheduler_find_next_active_from((g_current_task + 1) % g_task_count, &next) == 0) {
            g_current_task = next;
        }
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

uint32_t scheduler_task_count(void) {
    return g_task_count;
}

const char *scheduler_task_name(uint32_t index) {
    if (index >= g_task_count) {
        return "(invalid)";
    }
    if (g_tasks[index].name == NULL) {
        return "(unnamed)";
    }
    return g_tasks[index].name;
}

uint64_t scheduler_task_runs(uint32_t index) {
    if (index >= g_task_count) {
        return 0;
    }
    return g_tasks[index].runs;
}

int scheduler_task_active(uint32_t index) {
    if (index >= g_task_count) {
        return -1;
    }
    return g_tasks[index].active ? 1 : 0;
}

int scheduler_set_task_active(uint32_t index, int active) {
    if (index >= g_task_count) {
        return -1;
    }
    int had_active_before = scheduler_any_active_task();
    g_tasks[index].active = active != 0;
    if (g_tasks[index].active) {
        if (!had_active_before) {
            g_current_task = index;
            g_ticks_until_switch = TASK_SWITCH_INTERVAL;
        }
        return 0;
    }
    if (!scheduler_any_active_task()) {
        return 0;
    }
    if (index == g_current_task) {
        uint32_t next = 0;
        if (scheduler_find_next_active_from((index + 1) % g_task_count, &next) == 0) {
            g_current_task = next;
        }
    }
    return 0;
}

int scheduler_find_task(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (uint32_t i = 0; i < g_task_count; ++i) {
        const char *task_name = g_tasks[i].name;
        if (task_name != NULL && streq(task_name, name)) {
            return (int)i;
        }
    }
    return -1;
}

void scheduler_reset_task_runs(uint32_t index) {
    if (index >= g_task_count) {
        return;
    }
    g_tasks[index].runs = 0;
}

void scheduler_reset_all_task_runs(void) {
    for (uint32_t i = 0; i < g_task_count; ++i) {
        g_tasks[i].runs = 0;
    }
}
