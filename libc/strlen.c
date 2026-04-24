#include <stddef.h>

size_t strlen(const char *str) {
    if (str == 0) {
        return 0;
    }
    const char *p = str;
    while (*p) {
        ++p;
    }
    return (size_t)(p - str);
}
