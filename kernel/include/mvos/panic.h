#pragma once

void panic(const char *message);
void kassert_fail(const char *expr, const char *file, int line, const char *func);

#define kassert(expr) ((expr) ? (void)0 : kassert_fail(#expr, __FILE__, __LINE__, __func__))