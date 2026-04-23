#include <stddef.h>

size_t strlen(const char *str) {
    const char *p = str;
    while (*p) {
        ++p;
    }
    return (size_t)(p - str);
}