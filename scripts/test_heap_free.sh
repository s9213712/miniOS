#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_heap_free.c"
HEAP_SRC="${ROOT_DIR}/kernel/mm/heap.c"
PMM_SRC="${ROOT_DIR}/kernel/mm/pmm.c"

if [ ! -f "${TEST_SRC}" ] || [ ! -f "${HEAP_SRC}" ] || [ ! -f "${PMM_SRC}" ]; then
    echo "[test_heap_free] missing source files"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_heap_free"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${HEAP_SRC}" \
    "${PMM_SRC}" \
    -o "${BIN}"

"${BIN}"
