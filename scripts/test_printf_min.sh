#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_printf_min.c"
PRINTF_SRC="${ROOT_DIR}/libc/printf_min.c"

if [ ! -f "${TEST_SRC}" ] || [ ! -f "${PRINTF_SRC}" ]; then
    echo "[test_printf_min] missing source files"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_printf_min"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${PRINTF_SRC}" \
    -o "${BIN}"

"${BIN}"
