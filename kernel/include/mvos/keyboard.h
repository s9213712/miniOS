#pragma once

#include <stdint.h>

void keyboard_init(void);
int keyboard_read_char(void);
int keyboard_read_char_nonblocking(void);
