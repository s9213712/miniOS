#include <mvos/keyboard.h>
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    __asm__ volatile("pause");
}

static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e',
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f',
    'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b',
    'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

void keyboard_init(void) {
    /* No additional setup is required for a simple polling-based driver. */
    (void)0;
}

int keyboard_read_char_nonblocking(void) {
    static int extended = 0;

    if (!(inb(0x64) & 0x01)) {
        return -1;
    }

    uint8_t scancode = inb(0x60);
    if (scancode == 0xe0) {
        extended = 1;
        return -1;
    }

    if (extended) {
        extended = 0;
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
                break;
        }
        return -1;
    }

    if (scancode & 0x80) {
        return -1;
    }
    if (scancode >= sizeof(scancode_to_ascii)) {
        return -1;
    }

    return (int)scancode_to_ascii[scancode];
}

int keyboard_read_char_timeout(uint64_t max_polls) {
    for (uint64_t i = 0; i < max_polls; ++i) {
        int c = keyboard_read_char_nonblocking();
        if (c >= 0) {
            return c;
        }
        io_wait();
    }
    return -1;
}

int keyboard_read_char(void) {
    int c;
    do {
        c = keyboard_read_char_nonblocking();
        io_wait();
    } while (c < 0);
    return c;
}
