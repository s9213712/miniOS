#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ISO_DIR="$ROOT_DIR/boot/iso_root"
LIMINE_DIR="$ROOT_DIR/boot/limine"
SCRIPT_DIR="$ROOT_DIR/scripts"
ISO_PATH="$BUILD_DIR/mvos.iso"
ISO_ENTRY_BIOS="boot/limine-bios-cd.bin"
ISO_ENTRY_UEFI="boot/limine-uefi-cd.bin"
KERNEL_NAME="boot/mvos.bin"

mkdir -p "$BUILD_DIR"
mkdir -p "$ISO_DIR"

if [ ! -f "$BUILD_DIR/mvos.bin" ]; then
  echo "[make_iso] kernel binary missing: $BUILD_DIR/mvos.bin" >&2
  echo "Run: make" >&2
  exit 1
fi

need_download=0
for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI limine.conf; do
  if [ ! -f "$LIMINE_DIR/$f" ]; then
    need_download=1
    break
  fi
done

if [ "$need_download" -eq 1 ]; then
  echo "[make_iso] Limine artifacts missing in $LIMINE_DIR."
  echo "[make_iso] Attempting to download official v11.x-binary release tree into a temporary directory."
  TEMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TEMP_DIR"' EXIT
    git clone --depth 1 --branch v11.x-binary https://github.com/Limine-bootloader/Limine.git "$TEMP_DIR/limine"
    cp "$TEMP_DIR/limine/limine-bios.sys" "$TEMP_DIR/limine/limine-bios-cd.bin" "$TEMP_DIR/limine/limine-uefi-cd.bin" "$TEMP_DIR/limine/BOOTX64.EFI" "$LIMINE_DIR/"
    cp "$ROOT_DIR/boot/limine.conf" "$LIMINE_DIR/"
  echo "[make_iso] Limine artifacts copied into $LIMINE_DIR"
fi

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/EFI/BOOT"

cp "$BUILD_DIR/mvos.bin" "$ISO_DIR/$KERNEL_NAME"
cp "$LIMINE_DIR/limine-bios-cd.bin" "$ISO_DIR/boot/limine-bios-cd.bin"
cp "$LIMINE_DIR/limine-bios.sys" "$ISO_DIR/boot/limine-bios.sys"
cp "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_DIR/boot/limine-uefi-cd.bin"
cp "$LIMINE_DIR/BOOTX64.EFI" "$ISO_DIR/EFI/BOOT/BOOTX64.EFI"
cp "$LIMINE_DIR/limine.conf" "$ISO_DIR/boot/limine.conf"

mkdir -p "$ISO_DIR/boot"

if ! command -v xorriso >/dev/null 2>&1; then
  echo "[make_iso] xorriso is required but missing." >&2
  echo "Install xorriso and rerun make iso." >&2
  exit 1
fi

xorriso -as mkisofs -R -r -J -b $ISO_ENTRY_BIOS -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot $ISO_ENTRY_UEFI -efi-boot-part --efi-boot-image --protective-msdos-label "$ISO_DIR" -o "$ISO_PATH"

LIMINE_EXE="$LIMINE_DIR/limine"
if [ -x "$LIMINE_EXE" ]; then
  "$LIMINE_EXE" bios-install "$ISO_PATH"
else
  if [ -f "$LIMINE_DIR/limine.exe" ]; then
    echo "[make_iso] Warning: found limine.exe but no native limine executable."
    echo "[make_iso] BIOS patching not performed; image may be BIOS-boot incomplete."
    echo "[make_iso] Optional: run limine bios-install manually in a Windows environment."
  else
    echo "[make_iso] Warning: limine boot patch utility not present in boot/limine."
    echo "[make_iso] BIOS boot patching skipped; make debug/run may still work in BIOS/QEMU for UEFI tests only."
  fi
fi

echo "[make_iso] Created $ISO_PATH"
