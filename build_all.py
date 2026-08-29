#!/usr/bin/env python3
"""Build every project and the host test suite in the PER monorepo."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent

def build_project(projct_name: str, command: list[str], cwd: Path) -> None:
    print("\nBuilding project:", projct_name)
    print("=" * 80)
    print(f"$ (cd {cwd} && {' '.join(command)})", flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def build() -> None:
    """Run the complete repository build in a fixed order."""

    build_project(
        "firmware",
        [sys.executable, "build_firmware.py"],
        ROOT / "firmware",
    )
    build_project(
        "daqapp",
        ["cargo", "build"],
        ROOT / "daqapp",
    )
    build_project(
        "host tests",
        [sys.executable, "tests/build_tests.py"],
        ROOT,
    )


build()
