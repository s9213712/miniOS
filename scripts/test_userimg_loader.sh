#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_userimg_loader.c"
USERIMG_SRC="${ROOT_DIR}/kernel/core/userimg.c"
ELF_SRC="${ROOT_DIR}/kernel/core/elf.c"
ELF_BLOB="${ROOT_DIR}/kernel/core/elf_sample_blob.c"
VMM_SRC="${ROOT_DIR}/kernel/mm/vmm.c"
USERPROC_SRC="${ROOT_DIR}/kernel/core/userproc.c"

if [ ! -f "${TEST_SRC}" ]; then
    echo "[test_userimg_loader] missing test source: ${TEST_SRC}"
    exit 1
fi
if [ ! -f "${USERIMG_SRC}" ] || [ ! -f "${ELF_SRC}" ] || [ ! -f "${ELF_BLOB}" ] || [ ! -f "${VMM_SRC}" ] || [ ! -f "${USERPROC_SRC}" ]; then
    echo "[test_userimg_loader] missing required source files"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_userimg_loader"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${USERIMG_SRC}" \
    "${ELF_SRC}" \
    "${ELF_BLOB}" \
    "${VMM_SRC}" \
    "${USERPROC_SRC}" \
    -o "${BIN}"

"${BIN}"
