#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PRE_PUSH_LOG_DIR:-}"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"

run_stage() {
  local label="$1"
  shift
  echo "[pre-push] ${label}"
  (cd "${ROOT_DIR}" && "$@")
}

log_if_needed() {
  if [ -z "${LOG_DIR}" ]; then
    return
  fi
  mkdir -p "${LOG_DIR}"
  local log_file="${LOG_DIR}/pre-push-${TIMESTAMP}.log"
  echo "[pre-push] logging to ${log_file}"
  exec >>"${log_file}" 2>&1
}

usage() {
  cat <<'EOF'
Usage: scripts/pre_push_check.sh

Run the repo pre-push verification pipeline.

Environment options:
  PRE_PUSH_LOG_DIR=PATH   If set, append all output to ${PATH}/pre-push-<timestamp>.log
EOF
}

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  usage
  exit 0
fi

if [ -n "${LOG_DIR}" ]; then
  log_if_needed
fi

echo "[pre-push] started at ${TIMESTAMP}"
run_stage "make all" make all
run_stage "make host-regressions" make host-regressions
echo "[pre-push] finished successfully"
