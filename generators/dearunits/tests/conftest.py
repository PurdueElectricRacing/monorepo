import json
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest

from core.config import BASE_TYPES_CONFIG_PATH, COMPOUND_TYPES_CONFIG_PATH, DEARUNITS_LIBRARY_DIR
from dearunits.api import DearUnits
from dearunits.config_loader import load_unit_config_bundle

STRICT_C_FLAGS = [
    "-std=c23", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Wshadow", "-Wconversion",
    "-Wdouble-promotion", "-Wsign-compare", "-Wformat=2", "-Wcast-qual", "-Wundef",
]
C_TEST_DIR = Path(__file__).resolve().parent / "c"
REPO_ROOT = Path(__file__).resolve().parents[3]


@dataclass
class CompileResult:
    ok: bool
    output: str


class CCompiler:
    def __init__(self, executable: str, workdir: Path) -> None:
        self.executable = executable
        self.workdir = workdir

    def compile(self, source: Path, include_dirs: Path | list[Path], output: str, flags: list[str] | None = None) -> CompileResult:
        binary = self.workdir / output
        dirs = include_dirs if isinstance(include_dirs, list) else [include_dirs]
        command = [self.executable, *(flags if flags is not None else STRICT_C_FLAGS), "-O1",
                   *(f"-I{directory}" for directory in dirs), str(source), "-o", str(binary), "-lm"]
        result = subprocess.run(command, capture_output=True, text=True)
        return CompileResult(result.returncode == 0, result.stdout + result.stderr)

    def run(self, output: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([str(self.workdir / output)], capture_output=True, text=True)


@pytest.fixture(scope="session")
def config_paths() -> tuple[Path, Path]:
    return BASE_TYPES_CONFIG_PATH, COMPOUND_TYPES_CONFIG_PATH


@pytest.fixture(scope="session")
def bundle():
    return load_unit_config_bundle()


@pytest.fixture(scope="session")
def artifacts(bundle) -> dict[str, str]:
    generator = DearUnits()
    return {
        str(artifact.relative_path): artifact.content
        for artifact in generator.generate(generator.parse(bundle))
    }


@pytest.fixture(scope="session")
def header(artifacts) -> str:
    return artifacts["dear_units.h"]


@pytest.fixture(scope="session")
def generated_dir(artifacts, tmp_path_factory) -> Path:
    root = tmp_path_factory.mktemp("dearunits_library")
    shutil.copy(DEARUNITS_LIBRARY_DIR / "dear_units_internal.h", root / "dear_units_internal.h")
    directory = root / "generated"
    directory.mkdir()
    for name, content in artifacts.items():
        (directory / name).write_text(content)
    return directory


@pytest.fixture(scope="session")
def cc(tmp_path_factory) -> CCompiler:
    executable = shutil.which("gcc") or shutil.which("cc")
    if executable is None:
        pytest.fail("a C23 host compiler (gcc) is required for the DearUnits generated-code tests")
    return CCompiler(executable, tmp_path_factory.mktemp("dearunits_build"))


@pytest.fixture
def load_config(tmp_path, capsys):
    def load(base=None, compound=None):
        base_path, compound_path = tmp_path / "base_types.json", tmp_path / "compound_types.json"
        base_path.write_text(json.dumps(base if base is not None else json.loads(BASE_TYPES_CONFIG_PATH.read_text())))
        compound_path.write_text(json.dumps(compound if compound is not None else json.loads(COMPOUND_TYPES_CONFIG_PATH.read_text())))
        capsys.readouterr()
        try:
            return load_unit_config_bundle(base_path, compound_path), ""
        except ValueError:
            return None, capsys.readouterr().out
    return load


@pytest.fixture
def real_configs() -> tuple[dict, dict]:
    return (
        json.loads(BASE_TYPES_CONFIG_PATH.read_text()),
        json.loads(COMPOUND_TYPES_CONFIG_PATH.read_text()),
    )
