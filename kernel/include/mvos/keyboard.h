#pragma once

#include <stdint.h>

enum {
    KEY_UP = 0x1101,
    KEY_DOWN = 0x1102,
    KEY_LEFT = 0x1103,
    KEY_RIGHT = 0x1104,
};

void keyboard_init(void);
void keyboard_handle_irq(void);
int keyboard_read_char(void);
int keyboard_read_char_nonblocking(void);
int keyboard_read_char_timeout(uint64_t max_polls);
