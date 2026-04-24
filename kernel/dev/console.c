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

    uint64_t bytes_per_pixel = (uint64_t)graphics_bpp >> 3u;
    if (bytes_per_pixel == 0u ||
        (graphics_pitch != 0u && y > UINT64_MAX / graphics_pitch)) {
        return;
    }
    uint64_t row_offset = y * graphics_pitch;
    if (x > (UINT64_MAX - row_offset) / bytes_per_pixel) {
        return;
    }
    uint64_t offset = row_offset + (x * bytes_per_pixel);
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

static int graphics_clip_rect(uint64_t *x, uint64_t *y, uint64_t *width, uint64_t *height) {
    if (!graphics_enabled || x == NULL || y == NULL || width == NULL || height == NULL ||
        *width == 0u || *height == 0u) {
        return 0;
    }
    if (*x >= graphics_width || *y >= graphics_height) {
        return 0;
    }

    uint64_t max_width = graphics_width - *x;
    uint64_t max_height = graphics_height - *y;
    if (*width > max_width) {
        *width = max_width;
    }
    if (*height > max_height) {
        *height = max_height;
    }
    return *width != 0u && *height != 0u;
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
    if (!graphics_clip_rect(&x, &y, &width, &height)) {
        return;
    }

    for (uint64_t row = 0; row < height; ++row) {
        uint64_t yy = y + row;
        for (uint64_t col = 0; col < width; ++col) {
            uint64_t xx = x + col;
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
    if (!graphics_clip_rect(&x, &y, &width, &height) || width < 2u || height < 2u) {
        return;
    }

    uint64_t right = x + width - 1u;
    uint64_t bottom = y + height - 1u;
    for (uint64_t col = x; col <= right; ++col) {
        graphics_plot(col, y, red, green, blue);
        graphics_plot(col, bottom, red, green, blue);
    }

    for (uint64_t row = y + 1u; row < bottom; ++row) {
        graphics_plot(x, row, red, green, blue);
        graphics_plot(right, row, red, green, blue);
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

static inline uint64_t resolve_graphics_fb_address(uint64_t framebuffer_address, uint64_t hhdm_offset) {
    if (framebuffer_address < 0x100000000ULL && hhdm_offset != 0u) {
        return framebuffer_address + hhdm_offset;
    }
    return framebuffer_address;
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
        klogln(" (expected RGB memory model=1)");
        return;
    }

    graphics_fb = (volatile uint8_t *)resolve_graphics_fb_address(framebuffer_address, hhdm_offset);
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

void console_launch_demo_gui_app(void) {
    if (!graphics_enabled) {
        return;
    }

    uint64_t app_width = (graphics_width > 260u) ? (graphics_width * 3u) / 4u : graphics_width;
    uint64_t app_height = (graphics_height > 160u) ? (graphics_height * 2u) / 3u : graphics_height;

    if (app_width < 120u) {
        app_width = graphics_width;
    }
    if (app_height < 90u) {
        app_height = graphics_height;
    }

    if (app_width > graphics_width) {
        app_width = graphics_width;
    }
    if (app_height > graphics_height) {
        app_height = graphics_height;
    }

    uint64_t app_x = (graphics_width > app_width) ? ((graphics_width - app_width) / 2u) : 0u;
    uint64_t app_y = (graphics_height > app_height) ? ((graphics_height - app_height) / 2u) : 0u;

    graphics_fill_rect(0u, 0u, graphics_width, graphics_height, 0x04u, 0x08u, 0x18u);
    graphics_fill_rect(app_x, app_y, app_width, app_height, 0x12u, 0x16u, 0x22u);
    graphics_draw_rect_border(app_x, app_y, app_width, app_height, 0xd0u, 0xd0u, 0xffu);

    uint64_t title_h = (app_height >= 22u) ? 18u : (app_height / 4u);
    if (title_h >= 1u) {
        graphics_fill_rect(app_x + 1u, app_y + 1u, app_width > 1u ? app_width - 1u : app_width, title_h, 0x44u, 0x98u, 0xd8u);
        if (app_width > 54u) {
            graphics_fill_rect(app_x + app_width - 18u, app_y + 2u, 12u, 10u, 0xe2u, 0x30u, 0x20u);
        }
    }

    uint64_t content_y = app_y + ((title_h > 0u) ? (title_h + 1u) : 2u);
    uint64_t content_x = app_x + 8u;
    uint64_t content_h = (app_height > (content_y - app_y + 4u)) ? (app_height - (content_y - app_y + 2u)) : 0u;
    uint64_t content_w = (app_width > 16u) ? (app_width - 16u) : 0u;

    graphics_fill_rect(
        content_x,
        content_y,
        content_w,
        content_h,
        0x08u,
        0x20u,
        0x50u
    );

    if (content_w >= 40u && content_h >= 24u) {
        for (uint64_t i = 0u; i < content_w; i += 20u) {
            uint64_t x = content_x + (i % (content_w - 8u + 1u));
            uint64_t y = content_y + ((i / 20u) % 3u) * 12u + 4u;
            if (y + 6u < content_y + content_h) {
                uint64_t bar_w = (content_w - (x - content_x) >= 16u) ? 12u : (content_w - (x - content_x) - 1u);
                graphics_fill_rect(x, y, bar_w, 6u, 0x94u, 0xbau, 0xd6u);
            }
        }
    }

    klogln("[graphics] launched demo GUI app");
}

void console_launch_demo_gui_alt_app(void) {
    if (!graphics_enabled) {
        return;
    }

    uint64_t app_width = (graphics_width > 360u) ? 360u : graphics_width;
    uint64_t app_height = (graphics_height > 220u) ? 220u : graphics_height;
    if (app_width < 180u) {
        app_width = graphics_width;
    }
    if (app_height < 140u) {
        app_height = graphics_height;
    }

    if (graphics_width > app_width) {
        app_width = (graphics_width + app_width) / 2u;
    }
    if (app_width > graphics_width) {
        app_width = graphics_width;
    }
    if (graphics_height > app_height) {
        app_height = (graphics_height + app_height) / 2u;
    }
    if (app_height > graphics_height) {
        app_height = graphics_height;
    }

    uint64_t app_x = (graphics_width > app_width) ? ((graphics_width - app_width) / 4u) : 0u;
    uint64_t app_y = (graphics_height > app_height) ? ((graphics_height - app_height) / 4u) : 0u;

    graphics_fill_rect(0u, 0u, graphics_width, graphics_height, 0x06u, 0x18u, 0x2au);
    graphics_draw_rect_border(0u, 0u, graphics_width, graphics_height, 0xaau, 0xaau, 0xaau);

    graphics_fill_rect(app_x, app_y, app_width, app_height, 0x18u, 0x18u, 0x10u);
    graphics_draw_rect_border(app_x, app_y, app_width, app_height, 0xfau, 0xe8u, 0x90u);

    uint64_t title_h = (app_height >= 24u) ? 18u : (app_height > 8u ? app_height / 3u : app_height);
    if (title_h > 0u) {
        graphics_fill_rect(app_x + 2u, app_y + 2u, app_width > 4u ? app_width - 4u : app_width, title_h, 0x1au, 0x1au, 0x35u);
        uint64_t icon = (app_width > 32u) ? 22u : (app_width > 12u ? app_width - 4u : 0u);
        if (icon > 8u && title_h > 10u) {
            graphics_fill_rect(app_x + app_width - icon, app_y + 4u, icon - 4u, title_h - 4u, 0x0eu, 0xb6u, 0x26u);
        }
    }

    uint64_t list_x = app_x + 10u;
    uint64_t list_y = app_y + (title_h + 2u);
    uint64_t list_w = app_width > 20u ? app_width - 20u : app_width;
    uint64_t list_h = app_height > (title_h + 30u) ? app_height - (title_h + 30u) : 0u;
    graphics_fill_rect(list_x, list_y, list_w, list_h, 0x0eu, 0x12u, 0x20u);

    for (uint64_t row = 0u; row < 3u; ++row) {
        uint64_t bar_y = list_y + 14u + (row * 16u);
        if (bar_y + 8u >= list_y + list_h || list_h < 2u || list_w < 1u) {
            break;
        }
        uint64_t bar_w = (list_w > 34u) ? (list_w * 2u / 3u) : (list_w > 2u ? list_w - 2u : list_w);
        uint64_t bar_h = 6u;
        graphics_fill_rect(list_x + 6u, bar_y, bar_w, bar_h, 0x72u, 0x94u, 0xbau);
        if (bar_w >= 12u) {
            graphics_fill_rect(list_x + 6u, bar_y, bar_w / 2u, bar_h, 0x0eu, 0x2eu, 0x6au);
        }
    }

    klogln("[graphics] launched alt demo GUI app");
}

void console_write_graphics_status(void) {
    if (!graphics_enabled) {
        console_write_string("[graphics] backend disabled\n");
        return;
    }

    console_write_string("[graphics] framebuffer status\n");
    console_write_string("width=");
    console_write_u64(graphics_width);
    console_write_string(" height=");
    console_write_u64(graphics_height);
    console_write_string(" pitch=");
    console_write_u64(graphics_pitch);
    console_write_string(" bpp=");
    console_write_u64((uint64_t)graphics_bpp);
    console_write_string("\n[graphics] memory model=");
    console_write_u64((uint64_t)graphics_memory_model);
    console_write_string(" masks R/G/B=");
    console_write_u64((uint64_t)graphics_red_mask_size);
    console_write_string("/");
    console_write_u64((uint64_t)graphics_green_mask_size);
    console_write_string("/");
    console_write_u64((uint64_t)graphics_blue_mask_size);
    console_write_string(" shifts ");
    console_write_u64((uint64_t)graphics_red_mask_shift);
    console_write_string("/");
    console_write_u64((uint64_t)graphics_green_mask_shift);
    console_write_string("/");
    console_write_u64((uint64_t)graphics_blue_mask_shift);
    console_write_string("\n");
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
    if (framebuffer_text_enabled && current_target != MVOS_CONSOLE_TARGET_FRAMEBUFFER) {
        framebuffer_write_char(c);
    }
}

void console_write_string(const char *str) {
    if (console_ops && console_ops->write_string) {
        console_ops->write_string(str);
    }
    if (framebuffer_text_enabled && current_target != MVOS_CONSOLE_TARGET_FRAMEBUFFER) {
        framebuffer_write_string(str);
    }
}

void console_write_u64(uint64_t value) {
    if (console_ops && console_ops->write_u64) {
        console_ops->write_u64(value);
    }
    if (framebuffer_text_enabled && current_target != MVOS_CONSOLE_TARGET_FRAMEBUFFER) {
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
