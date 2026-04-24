#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void (*mvos_userapp_entry_t)(void);

void userapp_init(void);
uint64_t userapp_count(void);
const char *userapp_name(uint64_t index);
const char *userapp_desc(uint64_t index);
int userapp_run(const char *name);
int userapp_is_user_mode(uint64_t index);
