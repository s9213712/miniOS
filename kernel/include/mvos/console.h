#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MVOS_CONSOLE_TARGET_SERIAL = 0,
    MVOS_CONSOLE_TARGET_FRAMEBUFFER = 1,
} mvos_console_target_t;

void console_init(void);
void console_enable_framebuffer_text_mode(uint64_t hhdm_offset);
void console_set_target(mvos_console_target_t target);
mvos_console_target_t console_target(void);
void console_write_char(char c);
void console_write_string(const char *str);
void console_write_u64(uint64_t value);
void console_write_fmtln(const char *label, const char *message);
