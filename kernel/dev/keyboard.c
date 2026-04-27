#include <mvos/keyboard.h>
#include <mvos/interrupt.h>
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e',
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f',
    'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b',
    'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

#define KEYBOARD_BUFFER_SIZE 32

static volatile uint8_t g_keyboard_head;
static volatile uint8_t g_keyboard_tail;
static uint16_t g_keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint8_t g_keyboard_extended;

static int keyboard_buffer_push(uint16_t key) {
    uint8_t head = g_keyboard_head;
    uint8_t next = (uint8_t)((head + 1u) % KEYBOARD_BUFFER_SIZE);
    if (next == g_keyboard_tail) {
        return -1;
    }
    g_keyboard_buffer[head] = key;
    g_keyboard_head = next;
    return 0;
}

static int keyboard_buffer_pop(void) {
    uint8_t tail = g_keyboard_tail;
    if (tail == g_keyboard_head) {
        return -1;
    }
    uint16_t key = g_keyboard_buffer[tail];
    g_keyboard_tail = (uint8_t)((tail + 1u) % KEYBOARD_BUFFER_SIZE);
    return (int)key;
}

static int keyboard_decode_scancode(uint8_t scancode) {
    if (scancode == 0xe0) {
        g_keyboard_extended = 1;
        return -1;
    }

    if (g_keyboard_extended) {
        g_keyboard_extended = 0;
        if (scancode & 0x80u) {
            return -1;
        }
        switch (scancode) {
            case 0x48:
                return KEY_UP;
            case 0x50:
                return KEY_DOWN;
            case 0x4b:
                return KEY_LEFT;
            case 0x4d:
                return KEY_RIGHT;
            default:
                return -1;
        }
    }

    if (scancode & 0x80u) {
        return -1;
    }
    if (scancode >= sizeof(scancode_to_ascii)) {
        return -1;
    }

    return (int)scancode_to_ascii[scancode];
}

void keyboard_init(void) {
    g_keyboard_head = 0;
    g_keyboard_tail = 0;
    g_keyboard_extended = 0;
}

void keyboard_handle_irq(void) {
    while ((inb(0x64) & 0x01u) != 0u) {
        int decoded = keyboard_decode_scancode(inb(0x60));
        if (decoded >= 0) {
            (void)keyboard_buffer_push((uint16_t)decoded);
        }
    }
}

int keyboard_read_char_nonblocking(void) {
    return keyboard_buffer_pop();
}

int keyboard_read_char_timeout(uint64_t max_polls) {
    for (uint64_t i = 0; i < max_polls; ++i) {
        int c = keyboard_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        mvos_idle_wait();
    }
    return -1;
}

int keyboard_read_char(void) {
    int c;
    do {
        c = keyboard_read_char_nonblocking();
        if (c < 0) {
            mvos_idle_wait();
        }
    } while (c < 0);
    return c;
}
