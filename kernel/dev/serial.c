#include <mvos/serial.h>
#include <mvos/log.h>
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline int serial_is_transmit_empty(void) {
    return inb(0x3fd) & 0x20;
}

void serial_init(void) {
    outb(0x3f9, 0x00);
    outb(0x3fb, 0x00);
    outb(0x3fb, 0x80);
    outb(0x3f8, 0x03);
    outb(0x3f9, 0x00);
    outb(0x3fb, 0x03);
    outb(0x3fa, 0xC7);
    outb(0x3fc, 0x0b);
    serial_write_string("[serial] ready\n");
}

void serial_write_char(char c) {
    while (!serial_is_transmit_empty()) {
        asm volatile("pause");
    }
    if (c == '\n') {
        outb(0x3f8, '\r');
        while (!serial_is_transmit_empty()) {
            asm volatile("pause");
        }
    }
    outb(0x3f8, (uint8_t)c);
}

void serial_write_string(const char *str) {
    while (*str) {
        serial_write_char(*str++);
    }
}

void serial_write_u64(uint64_t value) {
    const char hex[] = "0123456789abcdef";
    klog("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_write_char(hex[(value >> i) & 0xf]);
    }
}
