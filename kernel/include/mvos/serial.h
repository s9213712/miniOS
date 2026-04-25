#pragma once

#include <stdint.h>

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *str);
void serial_write_u64(uint64_t value);
int serial_read_char_nonblocking(void);
int serial_read_char(void);
