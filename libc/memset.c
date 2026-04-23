#include <stddef.h>

void *memset(void *dest, int value, size_t num) {
    unsigned char *d = (unsigned char *)dest;
    while (num--) {
        *d++ = (unsigned char)value;
    }
    return dest;
}