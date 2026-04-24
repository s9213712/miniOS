#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLOB="${ROOT_DIR}/kernel/core/elf_sample_blob.c"
SOURCE="${ROOT_DIR}/samples/linux-user/hello_linux_tiny.S"

if [ ! -f "${SOURCE}" ]; then
    echo "[test_elf_sample] missing source: ${SOURCE}"
    exit 1
fi

echo "[test_elf_sample] regenerating blob"
make -C "${ROOT_DIR}" refresh-elf-sample >/dev/null

if [ ! -f "${BLOB}" ]; then
    echo "[test_elf_sample] missing blob after refresh: ${BLOB}"
    exit 1
fi

if ! grep -q 'const uint8_t g_linux_user_elf_sample\[\] = {' "${BLOB}"; then
    echo "[test_elf_sample] missing byte array symbol in blob"
    exit 1
fi

if ! grep -q 'const uint64_t g_linux_user_elf_sample_len = sizeof(g_linux_user_elf_sample);' "${BLOB}"; then
    echo "[test_elf_sample] missing length symbol in blob"
    exit 1
fi

if ! grep -q '0x7f, 0x45, 0x4c, 0x46' "${BLOB}"; then
    echo "[test_elf_sample] ELF magic bytes not found in blob"
    exit 1
fi

byte_count="$(grep -o '0x[0-9a-f][0-9a-f],' "${BLOB}" | wc -l | tr -d ' ')"
if [ "${byte_count}" -lt 128 ]; then
    echo "[test_elf_sample] blob too small: ${byte_count} bytes"
    exit 1
fi

echo "[test_elf_sample] blob bytes=${byte_count}"
echo "[test_elf_sample] PASS"
