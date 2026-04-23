#include <mvos/vfs.h>
#include <mvos/console.h>
#include <mvos/log.h>
#include <stdint.h>

typedef struct vfs_node {
    const char *path;
    const uint8_t *data;
    uint64_t size;
    uint32_t checksum;
} vfs_node_t;

typedef struct vfs_open_state {
    const vfs_node_t *node;
    uint64_t cursor;
    int in_use;
} vfs_open_state_t;

static const uint8_t init_file_boot_txt[] =
    "miniOS initfs sample\n"
    "ready=1\n";

static const uint8_t init_file_readme_txt[] =
    "miniOS init filesystem\n"
    "used for phase 6 smoke diagnostics.\n";

static vfs_node_t g_nodes[] = {
    {"/boot/init/boot.txt", init_file_boot_txt, sizeof(init_file_boot_txt) - 1, 0},
    {"/boot/init/readme.txt", init_file_readme_txt, sizeof(init_file_readme_txt) - 1, 0},
};

#define MAX_OPEN_FILES 4

static vfs_open_state_t g_open_files[MAX_OPEN_FILES];

static uint32_t fnv1a_hash32(const void *ptr, uint64_t len) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 0x811c9dc5u;
    for (uint64_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

static void init_file_checksums(void) {
    for (uint64_t i = 0; i < sizeof(g_nodes) / sizeof(g_nodes[0]); ++i) {
        vfs_node_t *node = &g_nodes[i];
        if (node->checksum == 0) {
            node->checksum = fnv1a_hash32(node->data, node->size);
        }
    }
}

static int path_eq(const char *a, const char *b) {
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int has_prefix(const char *str, const char *prefix) {
    if (prefix == NULL || prefix[0] == '\0') {
        return 1;
    }
    for (uint64_t i = 0; ; ++i) {
        if (prefix[i] == '\0') {
            return 1;
        }
        if (str[i] == '\0' || str[i] != prefix[i]) {
            return 0;
        }
    }
}

int vfs_open(const char *path, mvos_vfs_file_t *file) {
    init_file_checksums();

    if (path == NULL || file == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!g_open_files[i].in_use) {
            const vfs_node_t *node = NULL;
            for (uint64_t j = 0; j < sizeof(g_nodes) / sizeof(g_nodes[0]); ++j) {
                if (path_eq(path, g_nodes[j].path)) {
                    node = &g_nodes[j];
                    break;
                }
            }
            if (!node) {
                return -2;
            }
            g_open_files[i] = (vfs_open_state_t){ .node = node, .cursor = 0, .in_use = 1 };
            file->data = node->data;
            file->size = node->size;
            file->cursor = 0;
            file->in_use = 1;
            file->path = node->path;
            file->checksum = node->checksum;
            file->slot = (int)i;
            return 0;
        }
    }
    return -3;
}

int vfs_read(mvos_vfs_file_t *file, void *buffer, uint64_t max_len, uint64_t *bytes_read) {
    if (!file || !file->in_use || !buffer || max_len == 0) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return -1;
    }

    uint64_t remaining = file->size - file->cursor;
    uint64_t to_copy = (max_len < remaining) ? max_len : remaining;
    const uint8_t *src = ((const uint8_t *)file->data) + file->cursor;
    for (uint64_t i = 0; i < to_copy; ++i) {
        ((uint8_t *)buffer)[i] = src[i];
    }

    file->cursor += to_copy;
    if (bytes_read) {
        *bytes_read = to_copy;
    }
    return 0;
}

void vfs_close(mvos_vfs_file_t *file) {
    if (!file || !file->in_use) {
        return;
    }
    for (uint64_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (g_open_files[i].in_use && i == (uint64_t)file->slot) {
            g_open_files[i].in_use = 0;
            break;
        }
    }
    file->in_use = 0;
}

uint64_t vfs_list(mvos_vfs_list_visitor_t visitor, const char *prefix, void *user_data) {
    uint64_t listed = 0;
    for (uint64_t i = 0; i < sizeof(g_nodes) / sizeof(g_nodes[0]); ++i) {
        if (has_prefix(g_nodes[i].path, prefix)) {
            ++listed;
            if (visitor) {
                visitor(g_nodes[i].path, g_nodes[i].size, g_nodes[i].checksum, user_data);
            }
        }
    }
    return listed;
}

static void vfs_log_entry(const char *path, uint64_t size, uint32_t checksum, void *unused) {
    (void)unused;
    console_write_string(path);
    console_write_string(" size=");
    console_write_u64(size);
    console_write_string(" checksum=0x");
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (checksum >> (28 - i * 4)) & 0xf;
        const char hex[] = "0123456789abcdef";
        console_write_char(hex[nibble]);
    }
    console_write_string("\n");
}

void vfs_diagnostic_list(void) {
    console_write_string("[vfs] listing /boot/init/\n");
    uint64_t count = vfs_list(vfs_log_entry, "/boot/init", NULL);
    console_write_string("[vfs] files=");
    console_write_u64(count);
    console_write_string("\n");
}

void vfs_diagnostic_read_file(void) {
    mvos_vfs_file_t file;
    if (vfs_open("/boot/init/readme.txt", &file) != 0) {
        klog("[vfs] open /boot/init/readme.txt failed\n");
        return;
    }

    uint64_t read_total = 0;
    uint8_t buffer[64];
    while (read_total < file.size) {
        uint64_t bytes;
        if (vfs_read(&file, buffer, sizeof(buffer), &bytes) != 0 || bytes == 0) {
            break;
        }
        read_total += bytes;
        for (uint64_t i = 0; i < bytes; ++i) {
            console_write_char((char)buffer[i]);
        }
    }
    klog("[vfs] read=/boot/init/readme.txt bytes=");
    klog_u64(read_total);
    klog(" expected=");
    klog_u64(file.size);
    klog(" checksum=0x");
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (file.checksum >> (28 - i * 4)) & 0xf;
        const char hex[] = "0123456789abcdef";
        console_write_char(hex[nibble]);
    }
    console_write_string("\n");
    vfs_close(&file);
}

void vfs_diagnostic_missing(void) {
    mvos_vfs_file_t file;
    if (vfs_open("/boot/init/missing.txt", &file) != 0) {
        klog("[vfs] missing=ok /boot/init/missing.txt not found\n");
        return;
    }
    vfs_close(&file);
    klog("[vfs] unexpected open success for missing file\n");
}
