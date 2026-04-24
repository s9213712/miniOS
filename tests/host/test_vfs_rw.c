#include <mvos/vfs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Host test stubs for vfs diagnostics link-time references. */
void console_write_string(const char *str) {
    (void)str;
}

void console_write_u64(uint64_t value) {
    (void)value;
}

void console_write_char(char c) {
    (void)c;
}

void klog(const char *message) {
    (void)message;
}

void klog_u64(uint64_t value) {
    (void)value;
}

static int read_file_exact(const char *path, char *out, size_t cap) {
    mvos_vfs_file_t file;
    if (vfs_open(path, &file) != 0) {
        return -1;
    }

    size_t total = 0;
    while (total + 1 < cap) {
        uint64_t bytes = 0;
        if (vfs_read(&file, out + total, (uint64_t)(cap - 1 - total), &bytes) != 0) {
            vfs_close(&file);
            return -1;
        }
        if (bytes == 0) {
            break;
        }
        total += (size_t)bytes;
    }
    out[total] = '\0';
    vfs_close(&file);
    return 0;
}

static void list_find_tmp_note(const char *path, uint64_t size, uint32_t checksum, void *user_data) {
    (void)size;
    (void)checksum;
    int *found = (int *)user_data;
    if (strcmp(path, "/tmp/note.txt") == 0) {
        *found = 1;
    }
}

static void list_static_checksum_visitor(const char *path, uint64_t size, uint32_t checksum, void *user_data) {
    (void)path;
    (void)size;
    int *bad = (int *)user_data;
    if (checksum == 0) {
        *bad = 1;
    }
}

int main(void) {
    char buffer[128];

    int static_bad_checksum = 0;
    uint64_t static_listed = vfs_list(list_static_checksum_visitor, "/boot/init", &static_bad_checksum);
    if (static_listed < 2 || static_bad_checksum) {
        fprintf(stderr, "[test_vfs_rw] expected nonzero static checksums before open\n");
        return 1;
    }

    if (read_file_exact("/boot/init/readme.txt", buffer, sizeof(buffer)) != 0) {
        fprintf(stderr, "[test_vfs_rw] failed to read static initfs file\n");
        return 1;
    }
    if (strstr(buffer, "miniOS init filesystem") == NULL) {
        fprintf(stderr, "[test_vfs_rw] static initfs content mismatch\n");
        return 1;
    }

    mvos_vfs_file_t elf_file;
    if (vfs_open("/boot/init/hello_linux_tiny", &elf_file) != 0) {
        fprintf(stderr, "[test_vfs_rw] failed to open static ELF sample from initfs\n");
        return 1;
    }
    uint8_t elf_magic[4] = {0};
    uint64_t elf_bytes = 0;
    if (vfs_read(&elf_file, elf_magic, sizeof(elf_magic), &elf_bytes) != 0 ||
        elf_bytes != sizeof(elf_magic) ||
        elf_magic[0] != 0x7f ||
        elf_magic[1] != 'E' ||
        elf_magic[2] != 'L' ||
        elf_magic[3] != 'F') {
        fprintf(stderr, "[test_vfs_rw] static ELF sample magic mismatch\n");
        vfs_close(&elf_file);
        return 1;
    }
    uint64_t seek_pos = 0;
    if (vfs_seek(&elf_file, 1, 0, &seek_pos) != 0 || seek_pos != 1) {
        fprintf(stderr, "[test_vfs_rw] static ELF seek failed\n");
        vfs_close(&elf_file);
        return 1;
    }
    if (vfs_seek(&elf_file, -1, 1, &seek_pos) != 0 || seek_pos != 0) {
        fprintf(stderr, "[test_vfs_rw] static ELF relative seek failed\n");
        vfs_close(&elf_file);
        return 1;
    }
    vfs_close(&elf_file);

    if (vfs_write_file("/tmp/note.txt", "hello", 5, 0) != 0) {
        fprintf(stderr, "[test_vfs_rw] write failed\n");
        return 1;
    }
    if (vfs_write_file("/tmp/note.txt", " world", 6, 1) != 0) {
        fprintf(stderr, "[test_vfs_rw] append failed\n");
        return 1;
    }
    if (read_file_exact("/tmp/note.txt", buffer, sizeof(buffer)) != 0) {
        fprintf(stderr, "[test_vfs_rw] failed to read /tmp/note.txt\n");
        return 1;
    }
    if (strcmp(buffer, "hello world") != 0) {
        fprintf(stderr, "[test_vfs_rw] /tmp/note.txt content mismatch: %s\n", buffer);
        return 1;
    }

    if (vfs_write_file("/boot/init/boot.txt", "x", 1, 0) != -2) {
        fprintf(stderr, "[test_vfs_rw] expected readonly path rejection for /boot/init/boot.txt\n");
        return 1;
    }

    if (vfs_write_file("/tmp/empty.txt", NULL, 0, 0) != 0) {
        fprintf(stderr, "[test_vfs_rw] touch-like write failed\n");
        return 1;
    }

    int found = 0;
    uint64_t listed = vfs_list(list_find_tmp_note, "/tmp", &found);
    if (listed < 2 || found == 0) {
        fprintf(stderr, "[test_vfs_rw] list failed listed=%llu found=%d\n",
                (unsigned long long)listed, found);
        return 1;
    }

    if (vfs_remove_file("/tmp/note.txt") != 0) {
        fprintf(stderr, "[test_vfs_rw] remove failed\n");
        return 1;
    }
    mvos_vfs_file_t file;
    if (vfs_open("/tmp/note.txt", &file) == 0) {
        fprintf(stderr, "[test_vfs_rw] removed file still openable\n");
        vfs_close(&file);
        return 1;
    }

    if (vfs_write_file("/tmp/stale-a.txt", "A", 1, 0) != 0) {
        fprintf(stderr, "[test_vfs_rw] stale handle setup write failed\n");
        return 1;
    }
    mvos_vfs_file_t stale;
    if (vfs_open("/tmp/stale-a.txt", &stale) != 0) {
        fprintf(stderr, "[test_vfs_rw] stale handle setup open failed\n");
        return 1;
    }
    if (vfs_remove_file("/tmp/stale-a.txt") != 0) {
        fprintf(stderr, "[test_vfs_rw] stale handle remove failed\n");
        return 1;
    }
    if (vfs_write_file("/tmp/stale-b.txt", "B", 1, 0) != 0) {
        fprintf(stderr, "[test_vfs_rw] stale handle slot reuse write failed\n");
        return 1;
    }
    uint64_t stale_bytes = 0;
    if (vfs_read(&stale, buffer, sizeof(buffer), &stale_bytes) == 0) {
        fprintf(stderr, "[test_vfs_rw] stale dynamic handle unexpectedly remained readable\n");
        return 1;
    }
    vfs_close(&stale);

    if (vfs_remove_file("/boot/init/readme.txt") != -3) {
        fprintf(stderr, "[test_vfs_rw] expected readonly remove rejection\n");
        return 1;
    }

    printf("[test_vfs_rw] PASS\n");
    return 0;
}
