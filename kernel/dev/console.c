#include <mvos/console.h>
#include <mvos/serial.h>
#include <mvos/log.h>
#include <stdbool.h>

typedef struct {
    void (*write_char)(char c);
    void (*write_string)(const char *str);
    void (*write_u64)(uint64_t value);
} mvos_console_ops_t;

static volatile uint16_t *framebuffer_text;
static bool framebuffer_text_enabled;
static bool graphics_enabled;
static uint16_t fb_columns = 80;
static uint16_t fb_rows = 25;
static uint16_t fb_cursor_col;
static uint16_t fb_cursor_row;
static const uint16_t fb_attrib = 0x0f;

static volatile uint8_t *graphics_fb;
static uint64_t graphics_width;
static uint64_t graphics_height;
static uint64_t graphics_pitch;
static uint16_t graphics_bpp;
static uint8_t graphics_memory_model;
static uint8_t graphics_red_mask_size;
static uint8_t graphics_red_mask_shift;
static uint8_t graphics_green_mask_size;
static uint8_t graphics_green_mask_shift;
static uint8_t graphics_blue_mask_size;
static uint8_t graphics_blue_mask_shift;

static uint32_t color_channel_to_masked_value(uint8_t channel, uint8_t mask_size, uint8_t mask_shift) {
    if (mask_size == 0 || mask_shift >= 32) {
        return 0u;
    }
    if (mask_size >= 32) {
        return ((uint32_t)channel) << mask_shift;
    }
    if (mask_size >= 8u) {
        return (uint32_t)channel << mask_shift;
    }
    if (mask_size == 1u) {
        return ((channel >= 128u ? 1u : 0u) << mask_shift);
    }
    return (uint32_t)(channel >> (8u - mask_size)) << mask_shift;
}

static void graphics_plot(uint64_t x, uint64_t y, uint8_t red, uint8_t green, uint8_t blue) {
    if (!graphics_enabled || graphics_fb == NULL) {
        return;
    }
    if (x >= graphics_width || y >= graphics_height) {
        return;
    }

    uint64_t offset = y * graphics_pitch + (x * ((uint64_t)graphics_bpp >> 3u));
    volatile uint8_t *px = graphics_fb + offset;
    uint32_t value =
        color_channel_to_masked_value(red, graphics_red_mask_size, graphics_red_mask_shift) |
        color_channel_to_masked_value(green, graphics_green_mask_size, graphics_green_mask_shift) |
        color_channel_to_masked_value(blue, graphics_blue_mask_size, graphics_blue_mask_shift);

    if (graphics_bpp == 32u) {
        px[0] = (uint8_t)(value & 0xffu);
        px[1] = (uint8_t)((value >> 8u) & 0xffu);
        px[2] = (uint8_t)((value >> 16u) & 0xffu);
        px[3] = 0x00u;
        return;
    }
    if (graphics_bpp == 24u) {
        px[0] = (uint8_t)(value & 0xffu);
        px[1] = (uint8_t)((value >> 8u) & 0xffu);
        px[2] = (uint8_t)((value >> 16u) & 0xffu);
        return;
    }
}

static void graphics_fill_rect(
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    if (!graphics_enabled) {
        return;
    }

    for (uint64_t row = 0; row < height; ++row) {
        uint64_t yy = y + row;
        if (yy >= graphics_height) {
            break;
        }
        for (uint64_t col = 0; col < width; ++col) {
            uint64_t xx = x + col;
            if (xx >= graphics_width) {
                break;
            }
            graphics_plot(xx, yy, red, green, blue);
        }
    }
}

static void graphics_draw_rect_border(
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    if (width < 2u || height < 2u) {
        return;
    }

    for (uint64_t col = x; col < x + width; ++col) {
        if (col < graphics_width) {
            graphics_plot(col, y, red, green, blue);
            if (height > 1u && y + (height - 1u) < graphics_height) {
                graphics_plot(col, y + (height - 1u), red, green, blue);
            }
        }
    }

    for (uint64_t row = y + 1u; row < y + height - 1u; ++row) {
        if (row >= graphics_height) {
            break;
        }
        if (x < graphics_width) {
            graphics_plot(x, row, red, green, blue);
        }
        if (x + (width - 1u) < graphics_width) {
            graphics_plot(x + (width - 1u), row, red, green, blue);
        }
    }
}

static void framebuffer_clear_text(void) {
    if (!framebuffer_text) {
        return;
    }

    for (uint16_t row = 0; row < fb_rows; ++row) {
        for (uint16_t col = 0; col < fb_columns; ++col) {
            framebuffer_text[(uint32_t)row * fb_columns + col] = (uint16_t)(fb_attrib << 8) | ' ';
        }
    }
}

static void framebuffer_write_char(char c);

static void framebuffer_scroll_text(void) {
    if (!framebuffer_text || fb_rows < 2) {
        return;
    }

    for (uint16_t row = 1; row < fb_rows; ++row) {
        for (uint16_t col = 0; col < fb_columns; ++col) {
            framebuffer_text[(uint32_t)(row - 1) * fb_columns + col] =
                framebuffer_text[(uint32_t)row * fb_columns + col];
        }
    }

    for (uint16_t col = 0; col < fb_columns; ++col) {
        framebuffer_text[(uint32_t)(fb_rows - 1) * fb_columns + col] = (uint16_t)(fb_attrib << 8) | ' ';
    }
}

static void framebuffer_newline_text(void) {
    fb_cursor_col = 0;
    if (fb_cursor_row + 1 >= fb_rows) {
        framebuffer_scroll_text();
        return;
    }
    ++fb_cursor_row;
}

static void framebuffer_write_hex_u64(uint64_t value) {
    const char *hex = "0123456789abcdef";
    framebuffer_write_char('0');
    framebuffer_write_char('x');
    for (int i = 60; i >= 0; i -= 4) {
        framebuffer_write_char(hex[(value >> i) & 0xf]);
    }
}

static void framebuffer_write_char(char c) {
    if (!framebuffer_text_enabled || framebuffer_text == NULL) {
        return;
    }

    if (c == '\n') {
        framebuffer_newline_text();
        return;
    }
    if (c == '\r') {
        fb_cursor_col = 0;
        return;
    }
    if (c == '\b') {
        if (fb_cursor_col > 0) {
            --fb_cursor_col;
        }
        framebuffer_text[(uint32_t)fb_cursor_row * fb_columns + fb_cursor_col] = (uint16_t)(fb_attrib << 8) | ' ';
        return;
    }

    framebuffer_text[(uint32_t)fb_cursor_row * fb_columns + fb_cursor_col] = (uint16_t)(fb_attrib << 8) | (uint8_t)c;
    ++fb_cursor_col;
    if (fb_cursor_col >= fb_columns) {
        framebuffer_newline_text();
    }
}

static void framebuffer_write_string(const char *str) {
    if (!str) {
        return;
    }

    for (const char *p = str; *p; ++p) {
        framebuffer_write_char(*p);
    }
}

static void framebuffer_write_u64(uint64_t value) {
    framebuffer_write_hex_u64(value);
}

static const mvos_console_ops_t serial_ops = {
    .write_char = serial_write_char,
    .write_string = serial_write_string,
    .write_u64 = serial_write_u64,
};

static const mvos_console_ops_t framebuffer_ops = {
    .write_char = framebuffer_write_char,
    .write_string = framebuffer_write_string,
    .write_u64 = framebuffer_write_u64,
};

static const mvos_console_ops_t *console_ops = &serial_ops;
static mvos_console_target_t current_target = MVOS_CONSOLE_TARGET_SERIAL;

void console_init(void) {
    serial_init();
    console_ops = &serial_ops;
    current_target = MVOS_CONSOLE_TARGET_SERIAL;
    klogln("[console] serial backend active");
}

void console_enable_framebuffer_text_mode(uint64_t hhdm_offset) {
    framebuffer_text = (volatile uint16_t *)(0xB8000u + hhdm_offset);
    if (framebuffer_text == NULL) {
        return;
    }

    fb_columns = 80;
    fb_rows = 25;
    fb_cursor_col = 0;
    fb_cursor_row = 0;
    framebuffer_clear_text();
    framebuffer_text_enabled = true;
}

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
) {
    if (framebuffer_address == 0u || width == 0u || height == 0u || pitch == 0u) {
        klogln("[graphics] ignoring invalid framebuffer descriptor");
        return;
    }
    if (bpp != 24u && bpp != 32u) {
        klog("[graphics] unsupported bpp=");
        klog_u64((uint64_t)bpp);
        klogln(" (need 24 or 32)");
        return;
    }
    if (memory_model != 1u) {
        klog("[graphics] unsupported memory model=");
        klog_u64((uint64_t)memory_model);
        klogln(" (expected indexed color model=1)");
        return;
    }

    graphics_fb = (volatile uint8_t *)(framebuffer_address + hhdm_offset);
    if (graphics_fb == NULL) {
        return;
    }
    graphics_width = width;
    graphics_height = height;
    graphics_pitch = pitch;
    graphics_bpp = bpp;
    graphics_memory_model = memory_model;
    graphics_red_mask_size = red_mask_size;
    graphics_red_mask_shift = red_mask_shift;
    graphics_green_mask_size = green_mask_size;
    graphics_green_mask_shift = green_mask_shift;
    graphics_blue_mask_size = blue_mask_size;
    graphics_blue_mask_shift = blue_mask_shift;
    graphics_enabled = true;

    graphics_fill_rect(0, 0, graphics_width, graphics_height, 0x06u, 0x06u, 0x10u);
    klogln("[graphics] framebuffer backend enabled");
}

int console_graphics_enabled(void) {
    return graphics_enabled ? 1 : 0;
}

void console_draw_gui_boot_window(void) {
    if (!graphics_enabled) {
        return;
    }

    uint64_t window_width = graphics_width >= 40u ? graphics_width / 2u : graphics_width;
    uint64_t window_height = graphics_height >= 20u ? graphics_height / 2u : graphics_height;
    uint64_t window_x = (graphics_width > window_width) ? ((graphics_width - window_width) / 3u) : 0u;
    uint64_t window_y = (graphics_height > window_height) ? ((graphics_height - window_height) / 3u) : 0u;

    graphics_fill_rect(window_x, window_y, window_width, window_height, 0x18u, 0x18u, 0x2cu);
    graphics_draw_rect_border(window_x, window_y, window_width, window_height, 0xf4u, 0xf4u, 0xf0u);

    uint64_t title_height = (window_height > 18u) ? 14u : 0u;
    if (title_height > 0u) {
        graphics_fill_rect(window_x + 1u, window_y + 1u, window_width - 2u, title_height, 0x20u, 0x40u, 0x78u);
        uint64_t close_size = 10u;
        if (window_width >= 3u + close_size) {
            graphics_fill_rect(window_x + window_width - close_size - 2u, window_y + 3u, close_size - 2u, close_size - 2u, 0x98u, 0x10u, 0x10u);
        }
    }

    klogln("[graphics] drew boot window demo");
}

void console_set_target(mvos_console_target_t target) {
    current_target = target;
    switch (target) {
    case MVOS_CONSOLE_TARGET_SERIAL:
        console_ops = &serial_ops;
        serial_init();
        break;
    case MVOS_CONSOLE_TARGET_FRAMEBUFFER:
        console_ops = &framebuffer_ops;
        break;
    default:
        console_ops = &serial_ops;
        current_target = MVOS_CONSOLE_TARGET_SERIAL;
        break;
    }
}

mvos_console_target_t console_target(void) {
    return current_target;
}

void console_write_char(char c) {
    if (console_ops && console_ops->write_char) {
        console_ops->write_char(c);
    }
    if (framebuffer_text_enabled) {
        framebuffer_write_char(c);
    }
}

void console_write_string(const char *str) {
    if (console_ops && console_ops->write_string) {
        console_ops->write_string(str);
    }
    if (framebuffer_text_enabled) {
        framebuffer_write_string(str);
    }
}

void console_write_u64(uint64_t value) {
    if (console_ops && console_ops->write_u64) {
        console_ops->write_u64(value);
    }
    if (framebuffer_text_enabled) {
        framebuffer_write_u64(value);
    }
}

void console_write_fmtln(const char *label, const char *message) {
    console_write_string(label);
    if (message) {
        console_write_string(message);
    }
    console_write_string("\n");
}
