#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_SRC="${ROOT_DIR}/tests/host/test_scheduler_ctl.c"
SCHED_SRC="${ROOT_DIR}/kernel/core/scheduler.c"

if [ ! -f "${TEST_SRC}" ]; then
    echo "[test_scheduler_ctl] missing test source: ${TEST_SRC}"
    exit 1
fi
if [ ! -f "${SCHED_SRC}" ]; then
    echo "[test_scheduler_ctl] missing scheduler source: ${SCHED_SRC}"
    exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

BIN="${TMP_DIR}/test_scheduler_ctl"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/kernel/include" \
    "${TEST_SRC}" \
    "${SCHED_SRC}" \
    -o "${BIN}"

"${BIN}"
