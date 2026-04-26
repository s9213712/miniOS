#!/usr/bin/env python3
"""Build host-side sample programs for miniOS learning workflow."""

from __future__ import annotations

import argparse
import json
import hashlib
import os
import shlex
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def detect_compiler(candidates: tuple[str, ...], fallback: str) -> str:
    for candidate in candidates:
        if shutil.which(candidate):
            return candidate
    return fallback


def run_compile(
    source: Path,
    output: Path,
    compiler: str,
    extra_flags: list[str],
    verbose: bool,
) -> tuple[bool, str]:
    cmd = [compiler, str(source), "-o", str(output)] + extra_flags
    invoke = " ".join(cmd)
    try:
        subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if verbose:
            print(f"[build_user_programs] compile cmd: {invoke}")
        return True, ""
    except FileNotFoundError as exc:
        return False, f"compiler not found: {compiler} ({exc})"
    except subprocess.CalledProcessError as exc:
        return False, exc.stderr.strip() or exc.stdout.strip() or f"compile failed: {source.name}"


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source-dir",
        default="samples/user-programs",
        help="directory containing .c/.cpp source files",
    )
    parser.add_argument(
        "--out-dir",
        default="build/host-programs",
        help="output directory for built binaries",
    )
    parser.add_argument(
        "--manifest",
        default="build/host-programs/manifest.json",
        help="manifest output path",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="print each compiler invocation",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="disable manifest-based incremental build cache",
    )
    return parser.parse_args()


def collect_sources(source_dir: Path) -> list[Path]:
    sources = sorted(source_dir.glob("*.c")) + sorted(source_dir.glob("*.cpp"))
    return sources


def _split_flags(value: str | None) -> list[str]:
    if not value:
        return []
    return shlex.split(value)


def _load_existing_manifest(manifest_path: Path) -> dict[str, dict]:
    if not manifest_path.exists():
        return {}
    try:
        data = json.loads(manifest_path.read_text())
        programs = data.get("programs")
        if not isinstance(programs, list):
            return {}
    except Exception:
        return {}

    index: dict[str, dict] = {}
    for entry in programs:
        if not isinstance(entry, dict):
            continue
        source = entry.get("source")
        status = entry.get("status")
        if isinstance(source, str) and isinstance(status, str):
            index[source] = entry
    return index


def _flags_unchanged(old: object, new: list[str]) -> bool:
    return old == new


def _can_reuse_entry(
    entry: dict,
    source_path: str,
    source_sha: str,
    compiler: str,
    flags: list[str],
    link_mode: str,
    output: Path,
) -> bool:
    if not isinstance(entry, dict):
        return False
    if entry.get("status") != "ok":
        return False
    if entry.get("source") != source_path:
        return False
    if entry.get("source_sha") != source_sha:
        return False
    if entry.get("compiler") != compiler:
        return False
    if not _flags_unchanged(entry.get("flags"), flags):
        return False
    if entry.get("link_mode") != link_mode:
        return False
    return output.exists()


def build_programs(source_dir: Path, out_dir: Path, manifest_path: Path, verbose: bool, use_cache: bool) -> int:
    c_compiler = os.environ.get("CC") or detect_compiler(
        ("gcc", "clang", "x86_64-none-elf-gcc", "x86_64-elf-gcc"),
        "gcc",
    )
    cpp_compiler = os.environ.get("CXX") or detect_compiler(
        ("g++", "clang++", "x86_64-none-elf-g++", "x86_64-elf-g++"),
        "g++",
    )

    common_flags = ["-Wall", "-Wextra", "-O2", "-fno-omit-frame-pointer"]
    if os.environ.get("MHOST_STATIC", "0") in ("1", "true", "yes", "on"):
        common_flags.append("-static")
    link_mode = "static" if "-static" in common_flags else "dynamic"
    c_flags = common_flags + ["-std=c11"]
    cpp_flags = common_flags + ["-std=c++17"]
    extra_c_flags = _split_flags(os.environ.get("MHOST_CFLAGS"))
    extra_cpp_flags = _split_flags(os.environ.get("MHOST_CXXFLAGS"))
    extra_common_flags = _split_flags(os.environ.get("MHOST_COMMON_FLAGS"))
    c_flags = extra_common_flags + c_flags + extra_c_flags
    cpp_flags = extra_common_flags + cpp_flags + extra_cpp_flags

    all_sources = collect_sources(source_dir)
    c_sources = [path for path in all_sources if path.suffix == ".c"]
    cpp_sources = [path for path in all_sources if path.suffix == ".cpp"]

    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    status_code = 0

    if verbose:
        print(f"[build_user_programs] using C compiler: {c_compiler}")
        print(f"[build_user_programs] using C++ compiler: {cpp_compiler}")

    built_at = datetime.now(timezone.utc).isoformat()
    existing_entries = _load_existing_manifest(manifest_path) if use_cache else {}

    for src in c_sources:
        out = out_dir / f"{src.stem}_c"
        source_sha = file_sha256(src)
        can_reuse = _can_reuse_entry(
            existing_entries.get(str(src)),
            str(src),
            source_sha,
            c_compiler,
            c_flags,
            link_mode,
            out,
        )
        ok = False
        msg = ""
        if can_reuse:
            ok = True
            if verbose:
                print(f"[build_user_programs] cache hit: {src.name}")
        else:
            ok, msg = run_compile(src, out, c_compiler, c_flags, verbose)
        if ok:
            if verbose:
                if can_reuse:
                    print(f"[build_user_programs] cache ok: {src.name} -> {out}")
                else:
                    print(f"[build_user_programs] compile ok: {src.name} -> {out}")
            entries.append(
                {
                    "name": src.name,
                    "language": "c",
                    "source": str(src),
                    "output": str(out),
                    "compiler": c_compiler,
                    "flags": c_flags,
                    "status": "ok",
                    "cached": can_reuse,
                    "built_at": built_at,
                    "link_mode": link_mode,
                    "source_sha": source_sha,
                    "size": out.stat().st_size,
                    "sha256": file_sha256(out),
                }
            )
        else:
            status_code = 1
            entries.append(
                {
                    "name": src.name,
                    "language": "c",
                    "source": str(src),
                    "output": str(out),
                    "compiler": c_compiler,
                    "flags": c_flags,
                    "status": "error",
                    "link_mode": link_mode,
                    "error": msg,
                }
            )
            print(f"[build_user_programs] ERROR compiling {src}: {msg}")

    for src in cpp_sources:
        out = out_dir / f"{src.stem}_cpp"
        source_sha = file_sha256(src)
        can_reuse = _can_reuse_entry(
            existing_entries.get(str(src)),
            str(src),
            source_sha,
            cpp_compiler,
            cpp_flags,
            link_mode,
            out,
        )
        ok = False
        msg = ""
        if can_reuse:
            ok = True
            if verbose:
                print(f"[build_user_programs] cache hit: {src.name}")
        else:
            ok, msg = run_compile(src, out, cpp_compiler, cpp_flags, verbose)
        if ok:
            if verbose:
                if can_reuse:
                    print(f"[build_user_programs] cache ok: {src.name} -> {out}")
                else:
                    print(f"[build_user_programs] compile ok: {src.name} -> {out}")
            entries.append(
                {
                    "name": src.name,
                    "language": "cpp",
                    "source": str(src),
                    "output": str(out),
                    "compiler": cpp_compiler,
                    "flags": cpp_flags,
                    "status": "ok",
                    "cached": can_reuse,
                    "built_at": built_at,
                    "link_mode": link_mode,
                    "source_sha": source_sha,
                    "size": out.stat().st_size,
                    "sha256": file_sha256(out),
                }
            )
        else:
            status_code = 1
            entries.append(
                {
                    "name": src.name,
                    "language": "cpp",
                    "source": str(src),
                    "output": str(out),
                    "compiler": cpp_compiler,
                    "flags": cpp_flags,
                    "status": "error",
                    "link_mode": link_mode,
                    "error": msg,
                }
            )
            print(f"[build_user_programs] ERROR compiling {src}: {msg}")

    manifest = {
        "source_dir": str(source_dir),
        "output_dir": str(out_dir),
        "c_compiler": c_compiler,
        "cpp_compiler": cpp_compiler,
        "default_link_mode": link_mode,
        "generated_at": built_at,
        "programs": entries,
        "count": len(entries),
        "status": "ok" if status_code == 0 else "error",
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True))
    print(f"[build_user_programs] built {len(entries)} programs into {out_dir}")
    return status_code

def _print_summary(manifest: Path) -> None:
    try:
        data = json.loads(manifest.read_text())
        program_count = len(data.get("programs", []))
        ok_count = sum(1 for p in data.get("programs", []) if p.get("status") == "ok")
        print(
            f"[build_user_programs] summary: total={program_count} ok={ok_count} "
            f"error={program_count - ok_count}"
        )
        cache_count = sum(1 for p in data.get("programs", []) if p.get("cached"))
        print(
            f"[build_user_programs] cache: hit={cache_count} miss={program_count - cache_count}"
        )
        print(f"[build_user_programs] manifest: {manifest}")
    except Exception as exc:  # pragma: no cover - defensive for malformed manifest
        print(f"[build_user_programs] failed to print summary: {exc}")


def main() -> int:
    args = parse_args()
    source_dir = Path(args.source_dir)
    out_dir = Path(args.out_dir)
    manifest_path = Path(args.manifest)

    verbose = args.verbose or os.environ.get("MHOST_VERBOSE", "0") == "1"

    if not source_dir.exists():
        print(f"[build_user_programs] source directory not found: {source_dir}")
        return 1

    sources = collect_sources(source_dir)
    if not sources:
        print(f"[build_user_programs] no .c/.cpp source found in: {source_dir}")
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(
            json.dumps(
                {
                    "source_dir": str(source_dir),
                    "output_dir": str(out_dir),
                    "programs": [],
                    "count": 0,
                    "generated_at": datetime.now(timezone.utc).isoformat(),
                    "status": "empty",
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 0

    use_cache = not args.no_cache and os.environ.get("MHOST_NO_CACHE", "0") not in (
        "1",
        "true",
        "yes",
        "on",
    )
    status = build_programs(source_dir, out_dir, manifest_path, verbose, use_cache)
    _print_summary(manifest_path)
    if args.verbose:
        with manifest_path.open("r", encoding="utf-8") as f:
            print("[build_user_programs] manifest preview:")
            print(f.read())

    return status


if __name__ == "__main__":
    raise SystemExit(main())
