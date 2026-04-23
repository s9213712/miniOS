#pragma once

#include <stdint.h>

typedef void (*mvos_task_fn)(uint64_t tick);

void scheduler_init(void);
int scheduler_add_task(const char *name, mvos_task_fn entry);
void scheduler_on_timer_tick(void);
void scheduler_run_once(void);
uint64_t scheduler_ticks(void);
