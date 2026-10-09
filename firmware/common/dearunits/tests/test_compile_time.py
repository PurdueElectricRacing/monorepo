"""
dearunits/test_compile_time.py

Check that the C23 compiler properly rejects/accepts DearUnits during build

Author: Irving Wang (irvingw@purdue.edu)
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest

TEST_DIR = Path(__file__).resolve().parent
FIRMWARE_DIR = TEST_DIR.parents[2]
CASES_DIR = TEST_DIR / "compile_time"
CASES = [
    ("valid_operations.c", True),
    ("reject_incompatible_addition.c", False),
    ("reject_unsupported_product.c", False),
    ("reject_non_base_product.c", False),
    ("reject_wrong_conversion.c", False),
    ("reject_wrong_result_type.c", False),
    ("reject_incompatible_comparison.c", False),
]


def compile_case(compiler, include_dir, filename):
    return subprocess.run(
        [
            *compiler,
            "-std=c23",
            "-fsyntax-only",
            f"-I{include_dir}",
            str(CASES_DIR / filename),
        ],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )


@pytest.fixture(scope="module")
def compile_environment():
    compiler = shlex.split(os.environ.get("CC", "cc"))
    if not compiler or shutil.which(compiler[0]) is None:
        pytest.fail(f"C compiler unavailable: {compiler!r}")

    include_dir = FIRMWARE_DIR

    # Missing headers, unsupported C23, or a broken compiler must fail setup,
    # rather than let all rejection cases pass for the wrong reason.
    control = compile_case(compiler, include_dir, "valid_operations.c")
    assert control.returncode == 0, (
        f"Valid C23 control failed with {compiler!r}:\n"
        f"{control.stdout}{control.stderr}"
    )
    return compiler, include_dir


@pytest.mark.parametrize(
    "filename, should_compile", CASES, ids=[Path(name).stem for name, _ in CASES]
)
def test_compile(compile_environment, filename, should_compile):
    compiler, include_dir = compile_environment
    result = compile_case(compiler, include_dir, filename)

    if result.returncode < 0:
        pytest.fail(f"Compiler terminated by signal {-result.returncode}")

    compiled_successfully = result.returncode == 0
    expected_outcome = "succeed" if should_compile else "fail"
    diagnostics = result.stdout + result.stderr

    assert compiled_successfully == should_compile, (
        f"{filename}: expected compilation to {expected_outcome}\n"
        f"Compiler exit code: {result.returncode}\n"
        f"{diagnostics}"
    )
