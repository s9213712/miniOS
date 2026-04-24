#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct mvos_vfs_file {
    const void *data;
    uint64_t size;
    uint64_t cursor;
    int in_use;
    const char *path;
    uint32_t checksum;
    int slot;
    int dynamic;
    uint64_t generation;
} mvos_vfs_file_t;

typedef void (*mvos_vfs_list_visitor_t)(const char *path, uint64_t size, uint32_t checksum, void *user_data);

int vfs_open(const char *path, mvos_vfs_file_t *file);
int vfs_read(mvos_vfs_file_t *file, void *buffer, uint64_t max_len, uint64_t *bytes_read);
void vfs_close(mvos_vfs_file_t *file);
uint64_t vfs_list(mvos_vfs_list_visitor_t visitor, const char *prefix, void *user_data);
int vfs_write_file(const char *path, const void *data, uint64_t len, int append);
int vfs_remove_file(const char *path);
void vfs_diagnostic_list(void);
void vfs_diagnostic_read_file(void);
void vfs_diagnostic_missing(void);
