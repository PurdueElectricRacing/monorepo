#!/usr/bin/env python3
"""Run the generator and firmware host test suites."""

from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
TEST_BUILD = ROOT / "firmware" / "build" / "host-tests"


def print_suite(name: str) -> None:
    """Print a visible test-suite heading before subprocess output."""
    print(f"\n{'=' * 80}\n{name}\n{'=' * 80}", flush=True)


def run() -> None:
    """Run generator tests, then host tests with a fresh coverage build."""
    print_suite("Generator unit tests")
    subprocess.run(
        [sys.executable, "-m", "pytest", "generators", "-q"],
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
        ["cmake", "--build", str(TEST_BUILD)],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(TEST_BUILD), "--target", "coverage"],
        cwd=ROOT,
        check=True,
    )


run()
