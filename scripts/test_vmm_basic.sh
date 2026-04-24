#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_vmm_basic.c"
VMM_SRC="${ROOT_DIR}/kernel/mm/vmm.c"

if [ ! -f "${TEST_SRC}" ]; then
    echo "[test_vmm_basic] missing test source: ${TEST_SRC}"
    exit 1
fi
if [ ! -f "${VMM_SRC}" ]; then
    echo "[test_vmm_basic] missing vmm source: ${VMM_SRC}"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_vmm_basic"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${VMM_SRC}" \
    -o "${BIN}"

"${BIN}"
