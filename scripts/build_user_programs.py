#!/usr/bin/env python3
"""Build host-side sample programs for miniOS learning workflow."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path
import os


def detect_compiler(candidates: tuple[str, ...], fallback: str) -> str:
    for candidate in candidates:
        if shutil.which(candidate):
            return candidate
    return fallback


def run_compile(source: Path, output: Path, compiler: str, extra_flags: list[str]) -> tuple[bool, str]:
    cmd = [compiler, str(source), "-o", str(output)] + extra_flags
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return True, ""
    except FileNotFoundError as exc:
        return False, f"compiler not found: {compiler} ({exc})"
    except subprocess.CalledProcessError as exc:
        return False, exc.stderr.strip() or exc.stdout.strip() or f"compile failed: {source.name}"


def collect_sources(source_dir: Path) -> list[Path]:
    sources = sorted(source_dir.glob("*.c")) + sorted(source_dir.glob("*.cpp"))
    return sources


def build_programs(source_dir: Path, out_dir: Path, manifest_path: Path) -> int:
    c_compiler = os.environ.get("CC") or detect_compiler(
        ("x86_64-none-elf-gcc", "x86_64-elf-gcc", "gcc"),
        "gcc",
    )
    cpp_compiler = os.environ.get("CXX") or detect_compiler(
        ("x86_64-none-elf-g++", "x86_64-elf-g++", "g++"),
        "g++",
    )

    common_flags = ["-Wall", "-Wextra", "-O2"]
    c_sources = sorted(source_dir.glob("*.c"))
    cpp_sources = sorted(source_dir.glob("*.cpp"))

    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    status_code = 0

    for src in c_sources:
        out = out_dir / f"{src.stem}_c"
        ok, msg = run_compile(src, out, c_compiler, common_flags + ["-std=c11"])
        if ok:
            entries.append(
                {
                    "name": src.name,
                    "language": "c",
                    "source": str(src),
                    "output": str(out),
                    "compiler": c_compiler,
                    "size": out.stat().st_size,
                }
            )
        else:
            status_code = 1
            print(f"[build_user_programs] ERROR compiling {src}: {msg}")

    for src in cpp_sources:
        out = out_dir / f"{src.stem}_cpp"
        ok, msg = run_compile(src, out, cpp_compiler, common_flags + ["-std=c++17"])
        if ok:
            entries.append(
                {
                    "name": src.name,
                    "language": "cpp",
                    "source": str(src),
                    "output": str(out),
                    "compiler": cpp_compiler,
                    "size": out.stat().st_size,
                }
            )
        else:
            status_code = 1
            print(f"[build_user_programs] ERROR compiling {src}: {msg}")

    manifest = {
        "source_dir": str(source_dir),
        "output_dir": str(out_dir),
        "c_compiler": c_compiler,
        "cpp_compiler": cpp_compiler,
        "programs": entries,
        "count": len(entries),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True))
    print(f"[build_user_programs] built {len(entries)} programs into {out_dir}")
    return status_code


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
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source_dir = Path(args.source_dir)
    out_dir = Path(args.out_dir)
    manifest_path = Path(args.manifest)

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
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 0

    return build_programs(source_dir, out_dir, manifest_path)


if __name__ == "__main__":
    raise SystemExit(main())
