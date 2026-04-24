#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/build/host-programs/manifest.json"

validate_manifest() {
  local expected_mode="$1"
  local expected_static_flag="$2"
  python3 - "$MANIFEST" "$expected_mode" "$expected_static_flag" <<'PY'
import json
import sys
from pathlib import Path

manifest_path = Path(sys.argv[1])
expected_mode = sys.argv[2]
expected_static_flag = sys.argv[3] == "1"

if not manifest_path.exists():
    raise SystemExit(f"[test_host_programs] missing manifest: {manifest_path}")

data = json.loads(manifest_path.read_text())
programs = data.get("programs", [])

if data.get("default_link_mode") != expected_mode:
    raise SystemExit(
        "[test_host_programs] default_link_mode mismatch: "
        f"expected={expected_mode} got={data.get('default_link_mode')}"
    )
if not programs:
    raise SystemExit("[test_host_programs] manifest has no program entries")

for entry in programs:
    if entry.get("status") != "ok":
        raise SystemExit(
            f"[test_host_programs] build entry not ok: {entry.get('name')} status={entry.get('status')}"
        )
    if entry.get("link_mode") != expected_mode:
        raise SystemExit(
            "[test_host_programs] per-program link_mode mismatch: "
            f"name={entry.get('name')} expected={expected_mode} got={entry.get('link_mode')}"
        )
    flags = entry.get("flags") or []
    has_static = "-static" in flags
    if expected_static_flag and not has_static:
        raise SystemExit(f"[test_host_programs] expected -static in flags for {entry.get('name')}")
    if (not expected_static_flag) and has_static:
        raise SystemExit(f"[test_host_programs] unexpected -static in dynamic mode for {entry.get('name')}")

print(f"[test_host_programs] manifest ok: mode={expected_mode} entries={len(programs)}")
PY
}

echo "[test_host_programs] dynamic mode build"
make -C "$ROOT_DIR" clean-host-programs >/dev/null
make -C "$ROOT_DIR" host-programs >/dev/null
validate_manifest "dynamic" "0"

echo "[test_host_programs] static mode build"
make -C "$ROOT_DIR" clean-host-programs >/dev/null
MHOST_STATIC=1 make -C "$ROOT_DIR" host-programs >/dev/null
validate_manifest "static" "1"

echo "[test_host_programs] PASS"
