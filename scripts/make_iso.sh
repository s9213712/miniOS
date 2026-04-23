#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ISO_DIR="$ROOT_DIR/boot/iso_root"
LIMINE_DIR="$ROOT_DIR/boot/limine"
LOCAL_LIMINE_DIR="${LIMINE_LOCAL_DIR:-}"
ISO_PATH="$BUILD_DIR/mvos.iso"
ISO_ENTRY_BIOS="boot/limine-bios-cd.bin"
ISO_ENTRY_UEFI="boot/limine-uefi-cd.bin"
KERNEL_NAME="boot/mvos.bin"
LIMINE_FILES="limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI limine.conf"

log_file_info() {
  local file="$1"
  local name
  name="$(basename "$file")"
  if [ -f "$file" ]; then
    echo "[make_iso] artifact: $name size=$(stat -c '%s' "$file") bytes"
    if command -v sha256sum >/dev/null 2>&1; then
      echo "[make_iso]   sha256=$("sha256sum" "$file" | awk '{print $1}')"
    fi
  else
    echo "[make_iso] artifact missing: $file"
  fi
}

mkdir -p "$BUILD_DIR"
mkdir -p "$ISO_DIR"

if [ ! -f "$BUILD_DIR/mvos.bin" ]; then
  echo "[make_iso] kernel binary missing: $BUILD_DIR/mvos.bin" >&2
  echo "Run: make" >&2
  exit 1
fi

need_download=0
for f in $LIMINE_FILES; do
  if [ ! -f "$LIMINE_DIR/$f" ]; then
    need_download=1
    break
  fi
done

if [ "$need_download" -eq 0 ] && [ -n "$LOCAL_LIMINE_DIR" ] && [ -x "$LOCAL_LIMINE_DIR/limine" ] && [ ! -x "$LIMINE_DIR/limine" ]; then
  need_download=1
fi

if [ "$need_download" -eq 1 ]; then
  if [ -n "$LOCAL_LIMINE_DIR" ]; then
    if [ ! -d "$LOCAL_LIMINE_DIR" ]; then
      echo "[make_iso] LIMINE_LOCAL_DIR is set but not a directory: $LOCAL_LIMINE_DIR" >&2
      exit 1
    fi

    for f in $LIMINE_FILES; do
      if [ "$f" = "limine.conf" ]; then
        continue
      fi
      if [ ! -f "$LOCAL_LIMINE_DIR/$f" ]; then
        echo "[make_iso] Missing $f in LIMINE_LOCAL_DIR=$LOCAL_LIMINE_DIR" >&2
        echo "Set LIMINE_LOCAL_DIR to a directory containing all required Limine files." >&2
        exit 1
      fi
    done

    echo "[make_iso] Copying Limine artifacts from LIMINE_LOCAL_DIR=$LOCAL_LIMINE_DIR."
    cp "$LOCAL_LIMINE_DIR/limine-bios.sys" \
       "$LOCAL_LIMINE_DIR/limine-bios-cd.bin" \
       "$LOCAL_LIMINE_DIR/limine-uefi-cd.bin" \
       "$LOCAL_LIMINE_DIR/BOOTX64.EFI" \
       "$LIMINE_DIR/"

    if [ -x "$LOCAL_LIMINE_DIR/limine" ]; then
      cp "$LOCAL_LIMINE_DIR/limine" "$LIMINE_DIR/"
    fi
    cp "$ROOT_DIR/boot/limine.conf" "$LIMINE_DIR/"
    need_download=0
  else
    echo "[make_iso] Limine artifacts missing in $LIMINE_DIR."
    echo "[make_iso] Attempting to download official v11.x-binary release tree into a temporary directory."
    TEMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$TEMP_DIR"' EXIT
    git clone --depth 1 --branch v11.x-binary https://github.com/Limine-bootloader/Limine.git "$TEMP_DIR/limine"
    cp "$TEMP_DIR/limine/limine-bios.sys" "$TEMP_DIR/limine/limine-bios-cd.bin" "$TEMP_DIR/limine/limine-uefi-cd.bin" "$TEMP_DIR/limine/BOOTX64.EFI" "$LIMINE_DIR/"
    cp "$ROOT_DIR/boot/limine.conf" "$LIMINE_DIR/"
    echo "[make_iso] Limine artifacts copied into $LIMINE_DIR"
  fi
fi

if [ "$need_download" -ne 0 ]; then
  echo "[make_iso] Limine bootstrap failed; required artifacts are missing after copy attempt."
  exit 1
fi

for artifact in $LIMINE_FILES; do
  log_file_info "$LIMINE_DIR/$artifact"
done

for required in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI; do
  if [ ! -f "$LIMINE_DIR/$required" ]; then
    echo "[make_iso] Missing required Limine artifact after staging: $required" >&2
    exit 1
  fi
done

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/EFI/BOOT"

cp "$BUILD_DIR/mvos.bin" "$ISO_DIR/$KERNEL_NAME"
cp "$LIMINE_DIR/limine-bios-cd.bin" "$ISO_DIR/boot/limine-bios-cd.bin"
cp "$LIMINE_DIR/limine-bios.sys" "$ISO_DIR/boot/limine-bios.sys"
cp "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_DIR/boot/limine-uefi-cd.bin"
cp "$LIMINE_DIR/BOOTX64.EFI" "$ISO_DIR/boot/EFI/BOOT/BOOTX64.EFI"
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
echo "[make_iso] ISO layout:"
if command -v xorriso >/dev/null 2>&1; then
  if ! xorriso -indev "$ISO_PATH" -report_el_torito plain >/tmp/make_iso_eltorito_report.txt 2>&1; then
    echo "[make_iso] xorriso report_el_torito failed; captured below:"
    sed -n '1,80p' /tmp/make_iso_eltorito_report.txt
  else
    echo "[make_iso] El Torito entries:"
    sed -n '1,80p' /tmp/make_iso_eltorito_report.txt
  fi

  if ! xorriso -indev "$ISO_PATH" -ls /boot >/tmp/make_iso_boot_ls.txt 2>&1; then
    echo "[make_iso] xorriso boot listing failed; captured below:"
    sed -n '1,80p' /tmp/make_iso_boot_ls.txt
  else
    echo "[make_iso] /boot files:"
    sed -n '1,80p' /tmp/make_iso_boot_ls.txt
  fi
else
  echo "[make_iso] Skipping ISO layout checks (xorriso unavailable)."
fi
