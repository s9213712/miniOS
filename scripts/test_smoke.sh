#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_LOG="$(mktemp)"
cleanup() { rm -f "$RUN_LOG"; }
trap cleanup EXIT

EXPECTED_BOOT="hello from kernel"
EXPECTED_FAULT="\\[fault\\]"
EXPECTED_PMM="\\[pmm\\] memory map"

if [ -n "${PANIC_TEST:-}" ]; then
  make PANIC_TEST=1 -B
elif [ -n "${FAULT_TEST:-}" ]; then
  make FAULT_TEST="${FAULT_TEST}" -B
else
  make
fi

make iso >/tmp/make_iso.log 2>&1 || cat /tmp/make_iso.log

set +e
if command -v timeout >/dev/null 2>&1; then
  timeout 8s bash scripts/run_qemu.sh | tee "$RUN_LOG"
  STATUS=${PIPESTATUS[0]}
else
  bash scripts/run_qemu.sh > "$RUN_LOG"
  STATUS=$?
fi
set -e

if ! grep -q "$EXPECTED_BOOT" "$RUN_LOG"; then
  echo "[test_smoke] Expected boot text not found: $EXPECTED_BOOT" >&2
  exit 1
fi

echo "[test_smoke] Boot text found."

if ! grep -q "$EXPECTED_PMM" "$RUN_LOG"; then
  echo "[test_smoke] Expected phase 3 memory map text not found." >&2
  exit 1
fi
echo "[test_smoke] Phase 3 memory map verified."

if [ -n "${PANIC_TEST:-}" ]; then
  if ! grep -q "\[panic\]" "$RUN_LOG"; then
    echo "[test_smoke] Expected panic marker not found." >&2
    exit 1
  fi
  echo "[test_smoke] Panic path exercised."
fi

if [ -n "${FAULT_TEST:-}" ]; then
  if ! grep -q "$EXPECTED_FAULT" "$RUN_LOG"; then
    echo "[test_smoke] Expected fault marker not found for FAULT_TEST=$FAULT_TEST" >&2
    exit 1
  fi
  echo "[test_smoke] Fault path exercised."
fi

echo "[test_smoke] PASS"
