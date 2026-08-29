#!/usr/bin/env python3
"""Configure, build, and run the host test suite."""

from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parent.parent
TEST_BUILD = ROOT / "firmware" / "build" / "host-tests"


def build() -> None:
    """Create a clean host-test build and generate its coverage report."""
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


build()
