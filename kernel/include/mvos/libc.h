#pragma once

#include <stddef.h>
#include <stdint.h>

void *memset(void *dest, int value, size_t num);
void *memcpy(void *dest, const void *src, size_t num);
size_t strlen(const char *str);