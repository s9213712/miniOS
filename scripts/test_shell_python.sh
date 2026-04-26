#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_shell_python.c"
VFS_SRC="${ROOT_DIR}/kernel/core/vfs.c"
SHELL_SRC="${ROOT_DIR}/kernel/core/shell_vfs_commands.c"
PYTHON_RUNTIME_SRC="${ROOT_DIR}/kernel/core/shell_py_runtime.c"
ELF_BLOB_SRC="${ROOT_DIR}/kernel/core/elf_sample_blob.c"

if [ ! -f "${TEST_SRC}" ]; then
    echo "[test_shell_python] missing test source: ${TEST_SRC}"
    exit 1
fi
if [ ! -f "${VFS_SRC}" ]; then
    echo "[test_shell_python] missing vfs source: ${VFS_SRC}"
    exit 1
fi
if [ ! -f "${SHELL_SRC}" ]; then
    echo "[test_shell_python] missing shell source: ${SHELL_SRC}"
    exit 1
fi
if [ ! -f "${ELF_BLOB_SRC}" ]; then
    echo "[test_shell_python] missing ELF blob source: ${ELF_BLOB_SRC}"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_shell_python"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${SHELL_SRC}" \
    "${PYTHON_RUNTIME_SRC}" \
    "${VFS_SRC}" \
    "${ELF_BLOB_SRC}" \
    -o "${BIN}"

"${BIN}"
