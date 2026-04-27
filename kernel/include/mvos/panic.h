#pragma once

#include <stdint.h>

enum {
    MVOS_PANIC_CTX_HAS_VECTOR = 1u << 0,
    MVOS_PANIC_CTX_HAS_ERROR = 1u << 1,
    MVOS_PANIC_CTX_HAS_CR2 = 1u << 2,
    MVOS_PANIC_CTX_HAS_FRAME = 1u << 3,
};

typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} mvos_panic_frame_t;

typedef struct {
    uint32_t valid_mask;
    uint64_t vector;
    uint64_t error_code;
    uint64_t cr2;
    mvos_panic_frame_t frame;
} mvos_panic_context_t;

void panic(const char *message);
void panic_with_context(const char *message, const mvos_panic_context_t *context);
void panic_dump_backtrace(void);
void kassert_fail(const char *expr, const char *file, int line, const char *func);

#define kassert(expr) ((expr) ? (void)0 : kassert_fail(#expr, __FILE__, __LINE__, __func__))
