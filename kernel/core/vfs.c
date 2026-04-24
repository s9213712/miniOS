#include <mvos/vfs.h>
#include <mvos/console.h>
#include <mvos/log.h>
#include <stdint.h>

extern const uint8_t g_linux_user_elf_sample[];
extern const uint64_t g_linux_user_elf_sample_len;

/* Minimal initramfs + writable /tmp overlay:
 * - readonly bootstrap nodes for deterministic smoke behavior
 * - writable runtime nodes under /tmp for teaching filesystem mutations
 */
typedef struct vfs_node {
    const char *path;
    const uint8_t *data;
    uint64_t size;
    uint32_t checksum;
} vfs_node_t;

typedef struct vfs_dynamic_node {
    int in_use;
    char path[64];
    uint8_t data[512];
    uint64_t size;
    uint32_t checksum;
    uint64_t generation;
} vfs_dynamic_node_t;

typedef struct vfs_open_state {
    int in_use;
} vfs_open_state_t;

static const uint8_t init_file_boot_txt[] =
    "miniOS initfs sample\n"
    "ready=1\n";

static const uint8_t init_file_readme_txt[] =
    "miniOS init filesystem\n"
    "used for phase 6 smoke diagnostics.\n";

/* In-memory manifest is intentionally tiny and deterministic for smoke testing.
 * Node metadata is computed once lazily during first open.
 */
static vfs_node_t g_nodes[] = {
    {"/boot/init/boot.txt", init_file_boot_txt, sizeof(init_file_boot_txt) - 1, 0},
    {"/boot/init/readme.txt", init_file_readme_txt, sizeof(init_file_readme_txt) - 1, 0},
    {"/boot/init/hello_linux_tiny", g_linux_user_elf_sample, 0, 0},
};

#define MAX_OPEN_FILES 4
#define MAX_DYNAMIC_FILES 8

static vfs_open_state_t g_open_files[MAX_OPEN_FILES];
static vfs_dynamic_node_t g_dynamic_nodes[MAX_DYNAMIC_FILES];

static uint32_t fnv1a_hash32(const void *ptr, uint64_t len) {
    /* Simple 32-bit FNV-1a checksum, no security claims, only integrity smoke checks. */
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 0x811c9dc5u;
    for (uint64_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

static uint64_t static_node_size(const vfs_node_t *node) {
    if (node == NULL) {
        return 0;
    }
    if (node->data == g_linux_user_elf_sample) {
        return g_linux_user_elf_sample_len;
    }
    return node->size;
}

static void init_file_checksums(void) {
    /* Lazy checksum initialization keeps startup code small and avoids extra init path.
     * If checksums are already present (non-zero), this function is a no-op.
     */
    for (uint64_t i = 0; i < sizeof(g_nodes) / sizeof(g_nodes[0]); ++i) {
        vfs_node_t *node = &g_nodes[i];
        if (node->checksum == 0) {
            node->checksum = fnv1a_hash32(node->data, static_node_size(node));
        }
    }
}

static int path_eq(const char *a, const char *b) {
    /* Exact path matcher keeps behavior explicit and allocation-free. */
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int copy_cstr(char *dst, uint64_t dst_cap, const char *src) {
    if (!dst || !src || dst_cap == 0) {
        return -1;
    }
    uint64_t i = 0;
    while (src[i] != '\0') {
        if (i + 1 >= dst_cap) {
            return -1;
        }
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return 0;
}

static int has_prefix(const char *str, const char *prefix) {
    /* Prefix matcher for list() filtering, e.g. "/boot/init" root. */
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

static int is_writable_tmp_path(const char *path) {
    if (path == NULL) {
        return 0;
    }
    if (!has_prefix(path, "/tmp/")) {
        return 0;
    }
    return path[5] != '\0';
}

static const vfs_node_t *find_static_node(const char *path) {
    for (uint64_t i = 0; i < sizeof(g_nodes) / sizeof(g_nodes[0]); ++i) {
        if (path_eq(path, g_nodes[i].path)) {
            return &g_nodes[i];
        }
    }
    return NULL;
}

static vfs_dynamic_node_t *find_dynamic_node(const char *path) {
    for (uint64_t i = 0; i < MAX_DYNAMIC_FILES; ++i) {
        if (g_dynamic_nodes[i].in_use && path_eq(path, g_dynamic_nodes[i].path)) {
            return &g_dynamic_nodes[i];
        }
    }
    return NULL;
}

static vfs_dynamic_node_t *alloc_dynamic_node(const char *path) {
    for (uint64_t i = 0; i < MAX_DYNAMIC_FILES; ++i) {
        if (!g_dynamic_nodes[i].in_use) {
            vfs_dynamic_node_t *node = &g_dynamic_nodes[i];
            if (copy_cstr(node->path, sizeof(node->path), path) != 0) {
                return NULL;
            }
            node->in_use = 1;
            node->size = 0;
            node->checksum = fnv1a_hash32("", 0);
            ++node->generation;
            if (node->generation == 0) {
                node->generation = 1;
            }
            return node;
        }
    }
    return NULL;
}

int vfs_open(const char *path, mvos_vfs_file_t *file) {
    /* Phase-6 entry point: allocate one slot from fixed open table, bind node data.
     * Return code policy:
     *  -1: invalid arguments
     *  -2: file not found
     *  -3: no free handle slot
     */
    init_file_checksums();

    if (path == NULL || file == NULL) {
        return -1;
    }

    const vfs_node_t *static_node = find_static_node(path);
    vfs_dynamic_node_t *dynamic_node = (static_node == NULL) ? find_dynamic_node(path) : NULL;
    if (static_node == NULL && dynamic_node == NULL) {
        return -2;
    }

    for (uint64_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!g_open_files[i].in_use) {
            g_open_files[i] = (vfs_open_state_t){ .in_use = 1 };
            if (static_node != NULL) {
                file->data = static_node->data;
                file->size = static_node_size(static_node);
                file->path = static_node->path;
                file->checksum = static_node->checksum;
                file->dynamic = 0;
                file->generation = 0;
            } else {
                file->data = dynamic_node->data;
                file->size = dynamic_node->size;
                file->path = dynamic_node->path;
                file->checksum = dynamic_node->checksum;
                file->dynamic = 1;
                file->generation = dynamic_node->generation;
            }
            file->cursor = 0;
            file->in_use = 1;
            file->slot = (int)i;
            return 0;
        }
    }
    return -3;
}

int vfs_read(mvos_vfs_file_t *file, void *buffer, uint64_t max_len, uint64_t *bytes_read) {
    /* Read is streaming-friendly: cursor advances and caller owns bytes buffer.
     * End-of-file returns bytes_read=0, return 0.
     */
    if (!file || !file->in_use || !buffer || max_len == 0) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return -1;
    }
    if (file->dynamic) {
        vfs_dynamic_node_t *node = find_dynamic_node(file->path);
        if (node == NULL || node->generation != file->generation) {
            if (bytes_read) {
                *bytes_read = 0;
            }
            return -1;
        }
        file->data = node->data;
        file->size = node->size;
        file->checksum = node->checksum;
    }

    if (file->cursor >= file->size) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return 0;
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
    /* Close validates handle ownership, then releases matching internal slot.
     * Double-close is tolerated by early return.
     */
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
    /* Optional visitor allows callers to emit custom path listings without exposing
     * internal array details.
     */
    init_file_checksums();
    uint64_t listed = 0;
    for (uint64_t i = 0; i < sizeof(g_nodes) / sizeof(g_nodes[0]); ++i) {
        if (has_prefix(g_nodes[i].path, prefix)) {
            ++listed;
            if (visitor) {
                visitor(g_nodes[i].path, static_node_size(&g_nodes[i]), g_nodes[i].checksum, user_data);
            }
        }
    }
    for (uint64_t i = 0; i < MAX_DYNAMIC_FILES; ++i) {
        if (!g_dynamic_nodes[i].in_use) {
            continue;
        }
        if (has_prefix(g_dynamic_nodes[i].path, prefix)) {
            ++listed;
            if (visitor) {
                visitor(g_dynamic_nodes[i].path, g_dynamic_nodes[i].size, g_dynamic_nodes[i].checksum, user_data);
            }
        }
    }
    return listed;
}

int vfs_write_file(const char *path, const void *data, uint64_t len, int append) {
    /* Writable overlay is intentionally constrained:
     * - only /tmp/ prefix paths are writable
     * - fixed number of files with fixed per-file storage
     *
     * Return code policy:
     *  -1: invalid args
     *  -2: path rejected
     *  -3: no free writable slot
     *  -4: exceeds per-file size limit
     */
    if (path == NULL || (!data && len != 0)) {
        return -1;
    }
    if (!is_writable_tmp_path(path)) {
        return -2;
    }

    vfs_dynamic_node_t *node = find_dynamic_node(path);
    if (node == NULL) {
        node = alloc_dynamic_node(path);
        if (node == NULL) {
            return -3;
        }
    }

    uint64_t write_offset = append ? node->size : 0;
    if (write_offset > sizeof(node->data) || len > (sizeof(node->data) - write_offset)) {
        return -4;
    }

    for (uint64_t i = 0; i < len; ++i) {
        node->data[write_offset + i] = ((const uint8_t *)data)[i];
    }
    node->size = write_offset + len;
    node->checksum = fnv1a_hash32(node->data, node->size);
    return 0;
}

int vfs_remove_file(const char *path) {
    /* Return code policy:
     *  -1: invalid args
     *  -2: not found
     *  -3: static/readonly node
     */
    if (path == NULL) {
        return -1;
    }
    if (find_static_node(path) != NULL) {
        return -3;
    }
    vfs_dynamic_node_t *node = find_dynamic_node(path);
    if (node == NULL) {
        return -2;
    }
    node->in_use = 0;
    node->size = 0;
    node->checksum = 0;
    ++node->generation;
    if (node->generation == 0) {
        node->generation = 1;
    }
    node->path[0] = '\0';
    return 0;
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
    /* Diagnostic helper used in boot smoke path.
     * Confirms manifest discoverability before moving to task/scheduler path.
     */
    console_write_string("[vfs] listing /boot/init/\n");
    uint64_t count = vfs_list(vfs_log_entry, "/boot/init", NULL);
    console_write_string("[vfs] files=");
    console_write_u64(count);
    console_write_string("\n");
}

void vfs_diagnostic_read_file(void) {
    /* Verifies read() behavior and expected byte accounting for a real test file.
     * Keeps expected values stable so CI can compare against serial output.
     */
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
    /* Negative-path probe ensures missing files fail cleanly and predictably. */
    mvos_vfs_file_t file;
    if (vfs_open("/boot/init/missing.txt", &file) != 0) {
        klog("[vfs] missing=ok /boot/init/missing.txt not found\n");
        return;
    }
    vfs_close(&file);
    klog("[vfs] unexpected open success for missing file\n");
}
