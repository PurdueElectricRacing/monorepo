"""Composable dataset preparation and batch plotting, without console output."""

import tomllib
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .plots.factory import render_plot
from .preprocessing.raw_to_polars import load_from_folder
from .preprocessing.telemetry_dataset import TelemetryDataset
from .signals.library import SignalLibrary


ANALYSIS_DIRECTORY = Path(__file__).resolve().parent
DATA_DIRECTORY = ANALYSIS_DIRECTORY / "data"
CONFIGURATION_DIRECTORY = ANALYSIS_DIRECTORY / "configuration"
SIGNALS_DIRECTORY = CONFIGURATION_DIRECTORY / "derived_signals"
OUTPUT_DIRECTORY = ANALYSIS_DIRECTORY / "output"


@dataclass(frozen=True)
class PlotFailure:
    """Failure details without retaining an exception or its traceback."""

    configuration_path: Path
    exception_type: str
    message: str


@dataclass(frozen=True)
class AnalysisResult:
    successful_plots: tuple[Path, ...]
    failed_plots: tuple[PlotFailure, ...]


def load_specification(path: str | Path) -> dict[str, Any]:
    """Read a single TOML plot specification."""

    with Path(path).open("rb") as file:
        return tomllib.load(file)


def prepare_dataset(
    data_directory: str | Path = DATA_DIRECTORY,
    signals_directory: str | Path = SIGNALS_DIRECTORY,
) -> TelemetryDataset:
    """Load telemetry and materialize all configured derived channels."""

    dataset = load_from_folder(data_directory)
    library = SignalLibrary.load_for_dataset(signals_directory, dataset)

    return library.enrich(dataset, library.names())


def render_configurations(
    dataset: TelemetryDataset,
    configuration_paths: Iterable[str | Path],
    output_directory: str | Path = OUTPUT_DIRECTORY,
) -> AnalysisResult:
    """Render configurations in supplied order, collecting per-plot failures."""

    successful: list[Path] = []
    failed: list[PlotFailure] = []

    for configuration_path in configuration_paths:
        path = Path(configuration_path)
        try:
            specification = load_specification(path)
            successful.append(render_plot(dataset, specification, output_directory))
        except Exception as error:
            failed.append(PlotFailure(path, type(error).__name__, str(error)))

    return AnalysisResult(tuple(successful), tuple(failed))


def run_analysis(
    data_directory: str | Path = DATA_DIRECTORY,
    *,
    configuration_directory: str | Path = CONFIGURATION_DIRECTORY,
    signals_directory: str | Path = SIGNALS_DIRECTORY,
    output_directory: str | Path = OUTPUT_DIRECTORY,
) -> AnalysisResult:
    """Prepare a dataset and render all top-level TOML plot configurations."""

    directory = Path(configuration_directory)
    if not directory.is_dir():
        raise FileNotFoundError(f"Plot configuration directory not found: {directory}")

    dataset = prepare_dataset(data_directory, signals_directory)
    return render_configurations(
        dataset, sorted(directory.glob("*.toml")), output_directory
    )
