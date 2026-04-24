#!/usr/bin/env python3
"""miniOS local development helper."""

from __future__ import annotations

import argparse
import shutil
import subprocess


def _check_tool(name: str) -> bool:
    return shutil.which(name) is not None


def show_tools() -> int:
    tools = [
        "make",
        "python3",
        "gcc",
        "g++",
        "qemu-system-x86_64",
        "nasm",
        "xorriso",
    ]
    all_ok = True
    print("[dev-status] tool check:")
    for t in tools:
        ok = _check_tool(t)
        all_ok &= ok
        print(f" - {t:20}: {'ok' if ok else 'missing'}")
    return 0 if all_ok else 2


def run(cmd: list[str], env: dict[str, str] | None = None) -> int:
    try:
        return subprocess.call(cmd, env=env)
    except FileNotFoundError:
        print(f"[dev-status] command not found: {cmd[0]}")
        return 127


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", action="store_true", help="run make")
    parser.add_argument(
        "--build-programs",
        action="store_true",
        help="run make build-host-programs",
    )
    args = parser.parse_args()

    status = show_tools()
    if args.build:
        print("[dev-status] running: make -B")
        status = run(["make", "-B"])
    if args.build_programs:
        print("[dev-status] running: make build-host-programs")
        programs_status = run(["make", "host-programs"])
        if programs_status != 0:
            status = programs_status
    return status


if __name__ == "__main__":
    raise SystemExit(main())
