#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO_PATH="${ROOT_DIR}/build/mvos.iso"
QEMU_BIN="qemu-system-x86_64"

if [ ! -f "$ISO_PATH" ]; then
  echo "[run_qemu] ISO missing, building now."
  make iso
fi

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
  echo "[run_qemu] qemu-system-x86_64 is required." >&2
  exit 1
fi

set -x
"$QEMU_BIN" -machine q35 -cpu qemu64 -m 1G -serial stdio -nographic -vga none -cdrom "$ISO_PATH" -no-reboot