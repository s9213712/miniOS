#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO_PATH="${ROOT_DIR}/build/mvos.iso"
QEMU_MACHINE="${QEMU_MACHINE:-q35}"
QEMU_CPU="${QEMU_CPU:-qemu64}"
QEMU_MEM="${QEMU_MEM:-1G}"
QEMU_GUI="${QEMU_GUI:-0}"

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
  QEMU_BIN="qemu-system-x86_64"
elif command -v qemu-system-x86 >/dev/null 2>&1; then
  QEMU_BIN="qemu-system-x86"
elif command -v qemu-system-x86_64-static >/dev/null 2>&1; then
  QEMU_BIN="qemu-system-x86_64-static"
else
  QEMU_BIN=""
fi

if [ ! -f "$ISO_PATH" ]; then
  echo "[run_qemu] ISO missing, building now."
  make iso
fi

if [ -z "$QEMU_BIN" ]; then
  echo "[run_qemu] qemu-system-x86_64/qemu-system-x86 is required." >&2
  exit 1
fi

QEMU_DISPLAY=(-nographic -vga none -display none)
if [ "$QEMU_GUI" = "1" ]; then
  QEMU_DISPLAY=(-serial stdio -monitor none -vga std)
else
  QEMU_DISPLAY=(-serial stdio -monitor none -nographic -vga none -display none)
fi

set -x
"$QEMU_BIN" -machine "$QEMU_MACHINE" -cpu "$QEMU_CPU" -m "$QEMU_MEM" "${QEMU_DISPLAY[@]}" -cdrom "$ISO_PATH" -boot d -no-reboot
