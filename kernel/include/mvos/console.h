#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MVOS_CONSOLE_TARGET_SERIAL = 0,
    MVOS_CONSOLE_TARGET_FRAMEBUFFER = 1,
} mvos_console_target_t;

struct limine_framebuffer;

void console_init(void);
void console_enable_framebuffer_text_mode(uint64_t hhdm_offset);
void console_enable_graphics_framebuffer(
    uint64_t hhdm_offset,
    uint64_t framebuffer_address,
    uint64_t width,
    uint64_t height,
    uint64_t pitch,
    uint16_t bpp,
    uint8_t memory_model,
    uint8_t red_mask_size,
    uint8_t red_mask_shift,
    uint8_t green_mask_size,
    uint8_t green_mask_shift,
    uint8_t blue_mask_size,
    uint8_t blue_mask_shift
);
void console_draw_gui_boot_window(void);
void console_launch_demo_gui_app(void);
void console_launch_demo_gui_alt_app(void);
void console_write_graphics_status(void);
int console_graphics_enabled(void);
void console_set_target(mvos_console_target_t target);
mvos_console_target_t console_target(void);
void console_write_char(char c);
void console_write_string(const char *str);
void console_write_u64(uint64_t value);
void console_write_fmtln(const char *label, const char *message);
