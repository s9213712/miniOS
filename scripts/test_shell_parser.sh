#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_shell_parser.c"
PARSER_SRC="${ROOT_DIR}/kernel/core/shell_parser.c"

if [ ! -f "${TEST_SRC}" ] || [ ! -f "${PARSER_SRC}" ]; then
    echo "[test_shell_parser] missing required source files"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_shell_parser"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${PARSER_SRC}" \
    -o "${BIN}"

"${BIN}"
