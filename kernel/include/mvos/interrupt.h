#pragma once

#include <stdint.h>

static inline void mvos_io_wait(void) {
    /* Keep delay semantics local to the CPU instead of touching legacy port 0x80. */
    __asm__ volatile("pause\n\tpause\n\tpause" ::: "memory");
}

static inline void mvos_idle_wait(void) {
    /* Sleep until the next interrupt instead of burning a full CPU in busy-poll loops. */
    __asm__ volatile("hlt" ::: "memory");
}

uint64_t timer_ticks(void);
void timer_init(uint16_t hz);
