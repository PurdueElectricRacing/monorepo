#!/usr/bin/env python3
"""Build every project and the host test suite in the PER monorepo."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent


def build() -> None:
    """Run the complete repository build in a fixed order."""
    subprocess.run(
        [sys.executable, "build_firmware.py"],
        cwd=ROOT / "firmware",
        check=True,
    )
    subprocess.run(["cargo", "build"], cwd=ROOT / "daqapp", check=True)
    subprocess.run(
        [sys.executable, "tests/build_tests.py"],
        cwd=ROOT,
        check=True,
    )


build()
