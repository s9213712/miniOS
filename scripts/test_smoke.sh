#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SMOKE_LOG_DIR="${TEST_SMOKE_LOG_DIR:-/tmp}"
SMOKE_BASENAME="${TEST_SMOKE_BASENAME:-miniOS-smoke}"
RUN_LOG="$SMOKE_LOG_DIR/${SMOKE_BASENAME}_qemu.log"
MAKE_ISO_LOG="$SMOKE_LOG_DIR/${SMOKE_BASENAME}_make_iso.log"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-8}"
SMOKE_OFFLINE="${SMOKE_OFFLINE:-0}"

mkdir -p "$SMOKE_LOG_DIR"
cleanup() {
  if [ "${SMOKE_KEEP_LOGS:-0}" != "1" ]; then
    rm -f "$RUN_LOG" "$MAKE_ISO_LOG"
  fi
}
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

if [ "${SKIP_SMOKE_RUN:-0}" = "1" ]; then
  echo "[test_smoke] SKIP_SMOKE_RUN=1, skipping make iso + qemu run."
  echo "[test_smoke] Build artifacts verified successfully."
  exit 0
fi

make iso >"$MAKE_ISO_LOG" 2>&1 || {
  cat "$MAKE_ISO_LOG"
  if grep -q "Could not resolve host" "$MAKE_ISO_LOG" || [ "$SMOKE_OFFLINE" = "1" ]; then
    echo "[test_smoke] Limine download is blocked (no DNS/network in this environment)." >&2
    echo "[test_smoke] Supply files via LIMINE_LOCAL_DIR, then run:" >&2
    echo "  LIMINE_LOCAL_DIR=/path/to/limine-bin make test-smoke" >&2
    if [ "$SMOKE_OFFLINE" = "1" ]; then
      echo "[test_smoke] Offline mode is enabled. Ensure cache exists in LIMINE_CACHE_DIR or set LIMINE_LOCAL_DIR."
    fi
  fi
  exit 1
}

set +e
if command -v timeout >/dev/null 2>&1; then
  timeout "${QEMU_TIMEOUT}s" bash scripts/run_qemu.sh >"$RUN_LOG" 2>&1
  STATUS=$?
else
  bash scripts/run_qemu.sh > "$RUN_LOG" 2>&1
  STATUS=$?
fi
set -e

if [ "$STATUS" -ne 0 ] && [ "$STATUS" -ne 124 ]; then
  echo "[test_smoke] QEMU exited with status $STATUS."
fi

if ! grep -q "$EXPECTED_BOOT" "$RUN_LOG"; then
  echo "[test_smoke] Expected boot text not found: $EXPECTED_BOOT" >&2
  echo "[test_smoke] ---- make_iso summary ----"
  tail -n 80 "$MAKE_ISO_LOG"
  echo "[test_smoke] ---- qemu log ----"
  if [ -s "$RUN_LOG" ]; then
    sed -n '1,140p' "$RUN_LOG"
  else
    echo "[test-smoke] qemu output was empty."
  fi
  if [ "$STATUS" -eq 124 ]; then
    echo "[test_smoke] QEMU timed out after ${QEMU_TIMEOUT}s. This can indicate early boot did not reach the serial log path."
  fi
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
