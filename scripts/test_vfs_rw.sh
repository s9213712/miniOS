#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_vfs_rw.c"
VFS_SRC="${ROOT_DIR}/kernel/core/vfs.c"

if [ ! -f "${TEST_SRC}" ]; then
    echo "[test_vfs_rw] missing test source: ${TEST_SRC}"
    exit 1
fi
if [ ! -f "${VFS_SRC}" ]; then
    echo "[test_vfs_rw] missing vfs source: ${VFS_SRC}"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_vfs_rw"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${VFS_SRC}" \
    -o "${BIN}"

"${BIN}"
