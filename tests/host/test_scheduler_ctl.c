#include <mvos/scheduler.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t g_task_a_calls;
static uint64_t g_task_b_calls;
static uint64_t g_task_extra_calls;

void klog(const char *message) {
    (void)message;
}

void klogln(const char *message) {
    (void)message;
}

void klog_u64(uint64_t value) {
    (void)value;
}

static void task_a(uint64_t tick) {
    (void)tick;
    g_task_a_calls++;
}

static void task_b(uint64_t tick) {
    (void)tick;
    g_task_b_calls++;
}

static void task_extra(uint64_t tick) {
    (void)tick;
    g_task_extra_calls++;
}

static void run_ticks(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; ++i) {
        scheduler_on_timer_tick();
        scheduler_run_once();
    }
}

int main(void) {
    scheduler_init();
    int a_idx = scheduler_add_task("task-a", task_a);
    int b_idx = scheduler_add_task("task-b", task_b);
    if (a_idx != 0 || b_idx != 1) {
        fprintf(stderr, "[test_scheduler_ctl] unexpected task indexes: a=%d b=%d\n", a_idx, b_idx);
        return 1;
    }

    for (int i = 2; i < 8; ++i) {
        char name[] = "task-x";
        name[5] = (char)('0' + i);
        int idx = scheduler_add_task(name, task_extra);
        if (idx != i) {
            fprintf(stderr, "[test_scheduler_ctl] expected extra task index=%d got=%d\n", i, idx);
            return 1;
        }
    }
    if (scheduler_add_task("task-overflow", task_extra) != -1) {
        fprintf(stderr, "[test_scheduler_ctl] expected overflow add to fail once MAX_TASKS is reached\n");
        return 1;
    }
    if (scheduler_task_count() != 8) {
        fprintf(stderr, "[test_scheduler_ctl] expected 8 tasks after capacity fill, got=%u\n",
                scheduler_task_count());
        return 1;
    }

    run_ticks(260);
    if (g_task_a_calls == 0 || g_task_b_calls == 0 || g_task_extra_calls == 0) {
        fprintf(stderr, "[test_scheduler_ctl] expected all registered tasks to run: a=%llu b=%llu extra=%llu\n",
                (unsigned long long)g_task_a_calls,
                (unsigned long long)g_task_b_calls,
                (unsigned long long)g_task_extra_calls);
        return 1;
    }

    uint64_t a_before = g_task_a_calls;
    uint64_t b_before = g_task_b_calls;
    uint64_t extra_before = g_task_extra_calls;
    if (scheduler_set_task_active((uint32_t)b_idx, 0) != 0) {
        fprintf(stderr, "[test_scheduler_ctl] failed to stop task-b\n");
        return 1;
    }
    run_ticks(900);
    if (g_task_b_calls != b_before) {
        fprintf(stderr, "[test_scheduler_ctl] stopped task-b still ran: before=%llu after=%llu\n",
                (unsigned long long)b_before, (unsigned long long)g_task_b_calls);
        return 1;
    }
    if (g_task_a_calls == a_before && g_task_extra_calls == extra_before) {
        fprintf(stderr, "[test_scheduler_ctl] no active task progressed after stopping task-b\n");
        return 1;
    }

    if (scheduler_task_active((uint32_t)b_idx) != 0) {
        fprintf(stderr, "[test_scheduler_ctl] task-b active state mismatch after stop\n");
        return 1;
    }
    if (scheduler_find_task("task-b") != b_idx) {
        fprintf(stderr, "[test_scheduler_ctl] find_task failed for task-b\n");
        return 1;
    }

    scheduler_reset_task_runs((uint32_t)a_idx);
    if (scheduler_task_runs((uint32_t)a_idx) != 0) {
        fprintf(stderr, "[test_scheduler_ctl] reset_task_runs did not clear task-a\n");
        return 1;
    }

    if (scheduler_set_task_active((uint32_t)b_idx, 1) != 0) {
        fprintf(stderr, "[test_scheduler_ctl] failed to restart task-b\n");
        return 1;
    }
    run_ticks(900);
    if (g_task_b_calls == b_before) {
        fprintf(stderr, "[test_scheduler_ctl] restarted task-b did not run\n");
        return 1;
    }

    scheduler_reset_all_task_runs();
    if (scheduler_task_runs((uint32_t)a_idx) != 0 || scheduler_task_runs((uint32_t)b_idx) != 0) {
        fprintf(stderr, "[test_scheduler_ctl] reset_all_task_runs did not clear counters\n");
        return 1;
    }

    printf("[test_scheduler_ctl] PASS\n");
    return 0;
}
