#!/usr/bin/env python3
"""Run the generator and firmware host test suites."""

import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
TEST_BUILD = ROOT / "firmware" / "build" / "host-tests"
BUILD_JOBS = os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or str(os.cpu_count() or 1)


def print_suite(name: str) -> None:
    """Print a visible test-suite heading before subprocess output."""
    print(f"\n{'=' * 80}\n{name}\n{'=' * 80}", flush=True)


def run() -> None:
    """Generate firmware headers, then run Python and host tests."""
    print_suite("Generate firmware headers")
    subprocess.run(
        [sys.executable, "generators/generate.py"],
        cwd=ROOT,
        check=True,
    )

    print_suite("Python unit tests")
    subprocess.run(
        [sys.executable, "-m", "pytest"],
        cwd=ROOT,
        check=True,
    )

    print_suite("Firmware host unit tests and coverage")
    if TEST_BUILD.exists():
        shutil.rmtree(TEST_BUILD)

    subprocess.run(
        [
            "cmake",
            "-S",
            "tests",
            "-B",
            str(TEST_BUILD),
            "-DPER_TEST_SANITIZERS=ON",
            "-DPER_TEST_COVERAGE=ON",
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(TEST_BUILD), "--parallel", BUILD_JOBS],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        [
            "cmake",
            "--build",
            str(TEST_BUILD),
            "--parallel",
            BUILD_JOBS,
            "--target",
            "coverage",
        ],
        cwd=ROOT,
        check=True,
    )


run()
