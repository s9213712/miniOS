#include <mvos/console.h>
#include <mvos/serial.h>
#include <mvos/log.h>

typedef struct {
    void (*write_char)(char c);
    void (*write_string)(const char *str);
    void (*write_u64)(uint64_t value);
} mvos_console_ops_t;

static void framebuffer_write_char(char c) {
    (void)c;
}

static void framebuffer_write_string(const char *str) {
    (void)str;
}

static void framebuffer_write_u64(uint64_t value) {
    (void)value;
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
}

void console_write_string(const char *str) {
    if (console_ops && console_ops->write_string) {
        console_ops->write_string(str);
    }
}

void console_write_u64(uint64_t value) {
    if (console_ops && console_ops->write_u64) {
        console_ops->write_u64(value);
    }
}

void console_write_fmtln(const char *label, const char *message) {
    console_write_string(label);
    if (message) {
        console_write_string(message);
    }
    console_write_string("\n");
}
