#!/usr/bin/env bash
set -euo pipefail

INSTALL=0
HAS_APT=0

usage() {
  cat <<'EOF'
Usage:
  ./scripts/setup-dev.sh [--install]

Checks required and optional dependencies.
Use --install on Debian/Ubuntu to install missing APT packages automatically.
EOF
}

if command -v apt-get >/dev/null 2>&1; then
  HAS_APT=1
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      INSTALL=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[setup-dev] Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
  shift
done

if [[ "$HAS_APT" -eq 1 && "$INSTALL" -eq 1 ]]; then
  echo "[setup-dev] Install mode: missing packages will be installed with apt."
fi

MISSING=()
MISSING_PKGS=()
DETECTED_TOOLCHAIN=()

need_cmd() {
  local cmd="$1"
  local pkg_hint="${2:-$1}"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    MISSING+=("$cmd")
    MISSING_PKGS+=("$pkg_hint")
  fi
}

dedupe_pkgs() {
  local pkg
  declare -A seen
  for pkg in "$@"; do
    if [[ -n "${seen[$pkg]:-}" ]]; then
      continue
    fi
    seen["$pkg"]=1
    printf '%s\n' "$pkg"
  done
}

need_cmd git git
need_cmd make make
need_cmd nasm nasm
need_cmd gcc gcc
need_cmd ld binutils
need_cmd objcopy binutils
need_cmd xorriso xorriso
need_cmd bash bash
need_cmd sha256sum coreutils
need_cmd timeout coreutils

if ! command -v qemu-system-x86_64 >/dev/null 2>&1 && ! command -v qemu-system-x86 >/dev/null 2>&1; then
  MISSING+=("qemu-system-x86")
  MISSING_PKGS+=("qemu-system-x86")
fi

if command -v x86_64-none-elf-gcc >/dev/null 2>&1 || command -v x86_64-none-elf-ld >/dev/null 2>&1 || command -v x86_64-none-elf-objcopy >/dev/null 2>&1; then
  DETECTED_TOOLCHAIN+=("x86_64-none-elf-*")
elif command -v x86_64-elf-gcc >/dev/null 2>&1 || command -v x86_64-elf-ld >/dev/null 2>&1 || command -v x86_64-elf-objcopy >/dev/null 2>&1; then
  DETECTED_TOOLCHAIN+=("x86_64-elf-*")
else
  DETECTED_TOOLCHAIN+=("native gcc+binutils")
fi

if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
  MISSING+=("curl or wget")
  MISSING_PKGS+=("curl")
fi

if [ "${#DETECTED_TOOLCHAIN[@]}" -gt 0 ] && [[ "${DETECTED_TOOLCHAIN[0]}" == "native gcc+binutils" ]]; then
  echo "[setup-dev] Note: no cross toolchain detected. Build may still work through Makefile fallbacks."
  echo "[setup-dev] Install one of x86_64-none-elf-* or x86_64-elf-* for cleaner output."
fi

echo "[setup-dev] Check: miniOS host dependencies"
if [ "${#MISSING[@]}" -eq 0 ]; then
  echo "[ok] All required commands available."
  if [ "${#DETECTED_TOOLCHAIN[@]}" -gt 0 ]; then
    echo "[ok] Toolchain detected: ${DETECTED_TOOLCHAIN[*]}"
  fi
  exit 0
fi

echo "[warn] Missing commands:"
for item in "${MISSING[@]}"; do
  echo "  - $item"
done

if [[ "$HAS_APT" -eq 1 && "$INSTALL" -eq 1 ]]; then
  mapfile -t UNIQUE_PKGS < <(dedupe_pkgs "${MISSING_PKGS[@]}")
  echo "[setup-dev] Installing missing packages..."
  sudo apt-get update
  sudo apt-get install -y "${UNIQUE_PKGS[@]}"
  echo "[setup-dev] Re-check after install:"
  exec "$0"
else
  echo "[setup-dev] Install with:"
  mapfile -t UNIQUE_PKGS < <(dedupe_pkgs "${MISSING_PKGS[@]}")
  if [[ "$HAS_APT" -eq 1 ]]; then
    echo "  sudo apt-get install -y ${UNIQUE_PKGS[*]}"
    echo "[setup-dev] Or run:"
    echo "  ./scripts/setup-dev.sh --install"
  else
    echo "  command not available automatically on this platform: no apt-get."
    echo "  Install equivalents for: ${UNIQUE_PKGS[*]}"
  fi
fi

exit 1
