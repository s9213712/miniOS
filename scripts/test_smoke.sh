#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SMOKE_LOG_DIR="${TEST_SMOKE_LOG_DIR:-/tmp}"
SMOKE_BASENAME="${TEST_SMOKE_BASENAME:-${SMOKE_BASENAME:-miniOS-smoke}}"
RUN_LOG="$SMOKE_LOG_DIR/${SMOKE_BASENAME}_qemu.log"
SERIAL_LOG="${SMOKE_LOG_DIR}/${SMOKE_BASENAME}_serial.log"
MAKE_ISO_LOG="$SMOKE_LOG_DIR/${SMOKE_BASENAME}_make_iso.log"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-8}"
SMOKE_OFFLINE="${SMOKE_OFFLINE:-0}"
LIMINE_CONF="$ROOT_DIR/boot/limine.conf"
KERNEL_ELF="$ROOT_DIR/build/mvos.elf"
KERNEL_BIN="$ROOT_DIR/build/mvos.bin"

mkdir -p "$SMOKE_LOG_DIR"
cleanup() {
  if [ "${SMOKE_KEEP_LOGS:-0}" != "1" ]; then
    rm -f "$RUN_LOG" "$MAKE_ISO_LOG" "$SERIAL_LOG"
  fi
}
trap cleanup EXIT

EXPECTED_BOOT="hello from kernel"
EXPECTED_FAULT="\\[fault\\]"
EXPECTED_PMM="\\[pmm\\] memory map"
EXPECTED_EXECVE_START="\\[execve-demo\\] running linux-abi probe"
EXPECTED_EXECVE_HELLO="hello from linux user elf"
EXPECTED_EXECVE_INITFS="read from initfs: miniOS init filesystem"
EXPECTED_EXECVE_RETURN="userapp execve returned"
EXPECTED_EXECVE_DONE="\\[execve-demo\\] linux-abi probe done"
EXPECTED_EXECVE_UNAME="\\[linux-abi\\] uname\\.sysname=miniOS release=0\\.47"
EXPECTED_PHASE20_HELLO="\\[ring3\\] hello from direct userapp"
EXPECTED_PHASE20_TICKS="\\[ring3\\] ticks="
EXPECTED_PHASE20_DONE="\\[phase20\\] demo user app ticks done"
EXPECTED_PROTO_LOWER="^protocol[[:space:]]*:[[:space:]]*limine$"
EXPECTED_PROTO_UPPER="^PROTOCOL[[:space:]]*=[[:space:]]*limine$"
EXPECTED_PATH_UPPER="^KERNEL_PATH[[:space:]]*=[[:space:]]*boot:///boot/mvos\\.bin$"
EXPECTED_PATH_LOWER="^path[[:space:]]*:[[:space:]]*boot\\(\\):/boot/mvos\\.bin$"

check_limine_conf() {
  local conf_path="$1"
  if [ ! -f "$conf_path" ]; then
    echo "[test_smoke] [phase-0] Limine config missing: $conf_path" >&2
    return 1
  fi
  if ! (grep -Eq "$EXPECTED_PROTO_LOWER" "$conf_path" || grep -Eq "$EXPECTED_PROTO_UPPER" "$conf_path"); then
    echo "[test_smoke] [phase-0] Limine protocol mismatch in $conf_path. Expected PROTOCOL/ protocol = limine." >&2
    return 1
  fi
  if ! (grep -Eq "$EXPECTED_PATH_UPPER" "$conf_path" || grep -Eq "$EXPECTED_PATH_LOWER" "$conf_path"); then
    echo "[test_smoke] [phase-0] Limine kernel path mismatch in $conf_path. Expected KERNEL_PATH or path = boot()/boot:///boot/mvos.bin." >&2
    return 1
  fi
  return 0
}

if [ -n "${PANIC_TEST:-}" ]; then
  make PANIC_TEST=1 -B
elif [ -n "${FAULT_TEST:-}" ]; then
  make FAULT_TEST="${FAULT_TEST}" -B
else
  make
fi

check_limine_conf "$LIMINE_CONF"

if [ ! -f "$KERNEL_ELF" ] || [ ! -f "$KERNEL_BIN" ]; then
  echo "[test_smoke] Missing build outputs: $KERNEL_ELF or $KERNEL_BIN" >&2
  exit 1
fi
if ! readelf -S "$KERNEL_ELF" | grep -Eq " \\.requests(\\*| )| \\.requests\\."; then
  echo "[test_smoke] .requests section missing in $KERNEL_ELF." >&2
  readelf -S "$KERNEL_ELF" | sed -n '1,200p'
  exit 1
fi
if ! readelf -s "$KERNEL_ELF" | grep -q "request_start"; then
  echo "[test_smoke] Limine request_start symbol missing in $KERNEL_ELF." >&2
  exit 1
fi
if ! readelf -s "$KERNEL_ELF" | grep -q "request_end"; then
  echo "[test_smoke] Limine request_end symbol missing in $KERNEL_ELF." >&2
  exit 1
fi
REQUEST_START_ADDR="$(readelf -s "$KERNEL_ELF" | awk '/request_start/ {print "0x"$2; exit}')"
REQUEST_END_ADDR="$(readelf -s "$KERNEL_ELF" | awk '/request_end/ {print "0x"$2; exit}')"
if [ -z "$REQUEST_START_ADDR" ] || [ -z "$REQUEST_END_ADDR" ]; then
  echo "[test_smoke] Limine request marker symbols missing while parsing request order." >&2
  exit 1
fi
if (( REQUEST_START_ADDR >= REQUEST_END_ADDR )); then
  echo "[test_smoke] Limine request marker order is incorrect (start should be before end)." >&2
  echo "[test_smoke] request_start: $REQUEST_START_ADDR"
  echo "[test_smoke] request_end:   $REQUEST_END_ADDR"
  exit 1
fi

if [ "${SKIP_SMOKE_RUN:-0}" = "1" ]; then
  echo "[test_smoke] SKIP_SMOKE_RUN=1, skipping make iso + qemu run."
  echo "[test_smoke] Build artifacts and Limine request metadata verified successfully."
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

ISO_STAGED_CONF="$ROOT_DIR/build/iso_root/boot/limine.conf"
if [ -f "$ISO_STAGED_CONF" ]; then
  check_limine_conf "$ISO_STAGED_CONF"
else
  echo "[test_smoke] [phase-0] ISO-staged limine.conf missing: $ISO_STAGED_CONF" >&2
  exit 1
fi

set +e
if command -v timeout >/dev/null 2>&1; then
  timeout "${QEMU_TIMEOUT}s" env QEMU_SERIAL_MODE=file QEMU_SERIAL_FILE="$SERIAL_LOG" bash scripts/run_qemu.sh >"$RUN_LOG" 2>&1
  STATUS=$?
else
  env QEMU_SERIAL_MODE=file QEMU_SERIAL_FILE="$SERIAL_LOG" bash scripts/run_qemu.sh > "$RUN_LOG" 2>&1
  STATUS=$?
fi
set -e

if [ "$STATUS" -ne 0 ] && [ "$STATUS" -ne 124 ]; then
  echo "[test_smoke] QEMU exited with status $STATUS."
fi

if [ ! -f "$SERIAL_LOG" ]; then
  echo "[test_smoke] Serial capture log not created: $SERIAL_LOG" >&2
  exit 1
fi
if ! grep -Fq "$EXPECTED_BOOT" "$SERIAL_LOG"; then
  echo "[test_smoke] Expected boot text not found: $EXPECTED_BOOT" >&2
  echo "[test_smoke] ---- make_iso summary ----"
  tail -n 80 "$MAKE_ISO_LOG"
  echo "[test_smoke] ---- qemu log ----"
  if [ -s "$RUN_LOG" ]; then
    sed -n '1,140p' "$RUN_LOG"
  else
    echo "[test_smoke] qemu output was empty."
    if ! command -v qemu-system-x86_64 >/dev/null 2>&1 && ! command -v qemu-system-x86 >/dev/null 2>&1 && ! command -v qemu-system-x86_64-static >/dev/null 2>&1; then
      echo "[test_smoke] qemu binary could not be resolved." >&2
    fi
  fi
  echo "[test_smoke] ---- serial log ----"
  if [ -s "$SERIAL_LOG" ]; then
    sed -n '1,140p' "$SERIAL_LOG"
  else
    echo "[test_smoke] serial output was empty."
  fi
  if [ "$STATUS" -eq 124 ]; then
    echo "[test_smoke] QEMU timed out after ${QEMU_TIMEOUT}s."
    echo "[test_smoke] This usually means Limine did not reach kernel serial path."
    if grep -q "Booting from DVD/CD" "$RUN_LOG" && grep -q "iPXE" "$RUN_LOG"; then
      echo "[test_smoke] QEMU stayed in firmware boot path (SeaBIOS -> iPXE) and never entered Limine."
      echo "[test_smoke] Likely causes:"
      echo "[test_smoke] - limine boot sectors not applied to the ISO."
      echo "[test_smoke] - the ISO boot catalog entry/path is not recognized by this QEMU."
      echo "[test_smoke] - wrong Limine artifact selection for BIOS path."
    fi
    if grep -q "BdsDxe: loading Boot0001" "$RUN_LOG"; then
      echo "[test_smoke] UEFI firmware launched DVD boot, but Limine did not hand off to kernel serial yet."
    fi
    echo "[test_smoke] Quick diagnostics:"
    echo "[test_smoke] LIMINE CONF:"
    grep -ni "protocol\\|path\\|kernel_path\\|timeout\\|serial" "$LIMINE_CONF"
    echo "[test_smoke] ELF ENTRY: $(readelf -h "$KERNEL_ELF" | awk '/Entry point address/{print $4}')"
    echo "[test_smoke] REQUEST SYMBOLS:"
    readelf -s "$KERNEL_ELF" | grep -E 'request_start|request_end|kmain|_start' || true
    echo "[test_smoke] REQUEST/BOOT SECTIONS:"
    readelf -S "$KERNEL_ELF" | awk '/\.requests|\.text|\.rodata|\.data|\.bss/{print}'
  fi
  exit 1
fi

echo "[test_smoke] Boot text found."

if ! grep -q "$EXPECTED_PMM" "$SERIAL_LOG"; then
  echo "[test_smoke] Expected phase 3 memory map text not found." >&2
  exit 1
fi
echo "[test_smoke] Phase 3 memory map verified."

if [ "${EXECVE_DEMO:-0}" = "1" ]; then
  for expected in "$EXPECTED_EXECVE_START" "$EXPECTED_EXECVE_HELLO" "$EXPECTED_EXECVE_INITFS" "$EXPECTED_EXECVE_RETURN" "$EXPECTED_EXECVE_DONE" "$EXPECTED_EXECVE_UNAME"; do
    if ! grep -q "$expected" "$SERIAL_LOG"; then
      echo "[test_smoke] Expected execve demo marker not found: $expected" >&2
      tail -n 120 "$SERIAL_LOG" >&2
      exit 1
    fi
  done
  if grep -q "\[fault\]\|\[panic\]" "$SERIAL_LOG"; then
    echo "[test_smoke] Execve demo produced fault/panic marker." >&2
    tail -n 120 "$SERIAL_LOG" >&2
    exit 1
  fi
  echo "[test_smoke] Execve demo verified."
fi

if [ "${PHASE20_DEMO:-0}" = "1" ]; then
  for expected in "$EXPECTED_PHASE20_HELLO" "$EXPECTED_PHASE20_TICKS" "$EXPECTED_PHASE20_DONE"; do
    if ! grep -q "$expected" "$SERIAL_LOG"; then
      echo "[test_smoke] Expected phase20 direct-userapp marker not found: $expected" >&2
      tail -n 120 "$SERIAL_LOG" >&2
      exit 1
    fi
  done
  if grep -q "\[fault\]\|\[panic\]" "$SERIAL_LOG"; then
    echo "[test_smoke] Phase20 direct userapp demo produced fault/panic marker." >&2
    tail -n 120 "$SERIAL_LOG" >&2
    exit 1
  fi
  echo "[test_smoke] Phase20 direct userapp demo verified."
fi

if [ -n "${PANIC_TEST:-}" ]; then
  if ! grep -q "\[panic\]" "$SERIAL_LOG"; then
    echo "[test_smoke] Expected panic marker not found." >&2
    exit 1
  fi
  echo "[test_smoke] Panic path exercised."
fi

if [ -n "${FAULT_TEST:-}" ]; then
  if ! grep -q "$EXPECTED_FAULT" "$SERIAL_LOG"; then
    echo "[test_smoke] Expected fault marker not found for FAULT_TEST=$FAULT_TEST" >&2
    exit 1
  fi
  echo "[test_smoke] Fault path exercised."
fi

echo "[test_smoke] PASS"
