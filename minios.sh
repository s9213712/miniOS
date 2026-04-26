#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SCRIPT_NAME="$(basename "$0")"
GUI_MODE="0"
GUI_SET=0
MAKE_ARGS=()

usage() {
    cat <<'EOF'
Usage: ./minios.sh [options] [extra make args...]

Options:
  -g, --gui     Launch QEMU with GUI window (equivalent to QEMU_GUI=1).
  -t, --tty     Launch QEMU in serial/TTY mode (equivalent to QEMU_GUI=0). Default.
  -h, --help    Show this help.

Environment:
  ENABLE_SHELL  Set to 1 to enter miniOS shell (default: 1).
  QEMU_GUI      Override GUI mode (1 or 0). Command-line options have precedence.

Examples:
  ./minios.sh
  ./minios.sh -g
  ./minios.sh -t
  ENABLE_SHELL=1 ./minios.sh
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -g|--gui)
            GUI_MODE="1"
            GUI_SET=1
            shift
            ;;
        -t|--tty)
            GUI_MODE="0"
            GUI_SET=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            MAKE_ARGS+=("$@")
            break
            ;;
        *)
            MAKE_ARGS+=("$1")
            shift
            ;;
    esac
done

export ENABLE_SHELL="${ENABLE_SHELL:-1}"
if [[ "$GUI_SET" == "1" ]]; then
    export QEMU_GUI="$GUI_MODE"
else
    export QEMU_GUI="${QEMU_GUI:-$GUI_MODE}"
fi

cd "$ROOT_DIR"

set -x
make run "${MAKE_ARGS[@]}"
