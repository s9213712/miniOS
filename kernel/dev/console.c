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
static bool framebuffer_enabled;
static uint16_t fb_columns = 80;
static uint16_t fb_rows = 25;
static uint16_t fb_cursor_col;
static uint16_t fb_cursor_row;
static const uint16_t fb_attrib = 0x0f;

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
    if (!framebuffer_enabled || framebuffer_text == NULL) {
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
    framebuffer_enabled = true;
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
    if (framebuffer_enabled) {
        framebuffer_write_char(c);
    }
}

void console_write_string(const char *str) {
    if (console_ops && console_ops->write_string) {
        console_ops->write_string(str);
    }
    if (framebuffer_enabled) {
        framebuffer_write_string(str);
    }
}

void console_write_u64(uint64_t value) {
    if (console_ops && console_ops->write_u64) {
        console_ops->write_u64(value);
    }
    if (framebuffer_enabled) {
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
