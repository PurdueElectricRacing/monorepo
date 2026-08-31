#!/usr/bin/env python3
"""Run static analysis on all PER firmware targets."""

import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / ".cache" / "cppcheck-build"
CPPCHECK_DIR = ROOT / ".cache" / "cppcheck"


def check() -> None:
    """Configure every firmware target and analyze its compilation database."""
    subprocess.run(
        ["cmake", "-E", "rm", "-rf", str(BUILD_DIR)],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(BUILD_DIR),
            "-G",
            "Ninja",
            "-DBOOTLOADER_BUILD=OFF",
        ],
        cwd=ROOT,
        check=True,
    )
    CPPCHECK_DIR.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "cppcheck",
            f"--project={BUILD_DIR / 'compile_commands.json'}",
            "--enable=warning,style,performance,portability",
            "--check-level=exhaustive",
            "--max-ctu-depth=4",
            "--safety",
            f"--cppcheck-build-dir={CPPCHECK_DIR}",
            "-D__GNUC__",
            "--platform=unix32",
            "--funsigned-char",
            "--file-filter=*/firmware/source/*",
            "--file-filter=*/firmware/common/*",
            "--file-filter=*/firmware/can_library/*",
            "--suppress=*:*/external/*",
            "--suppress=*:*/source/torque_vector/vcu/*",
            "--suppress=*:*/common/phal_F4/*",
            "--suppress=preprocessorErrorDirective",
            "--inline-suppr",
            "--quiet",
            f"-j{os.cpu_count() or 1}",
            "--error-exitcode=1",
        ],
        cwd=ROOT,
        check=True,
    )


check()
