"""Small data-alignment and output helpers used by the renderers."""

from collections.abc import Callable, Iterable
from datetime import timedelta
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import polars as pl
from matplotlib.figure import Figure

from ..preprocessing.telemetry_dataset import TelemetryDataset


PlotCreator = Callable[[TelemetryDataset, dict[str, Any]], Figure]


def require_columns(dataset: TelemetryDataset, columns: Iterable[str]) -> list[str]:
    """Validate and return required columns in their first-occurrence order."""

    required = list(dict.fromkeys(columns))
    missing = [column for column in required if column not in dataset.frame.columns]
    if missing:
        raise ValueError(f"Missing required columns: {missing}")

    return required


def signal_samples(
    dataset: TelemetryDataset,
    time_column: str,
    value_column: str,
    alias: str = "value",
) -> pl.DataFrame:
    """Select timestamped, finite signal values without changing their order."""
    return dataset.frame.select(
        pl.col(time_column).alias("_time"),
        pl.col(value_column).alias(alias),
    ).filter(pl.col("_time").is_not_null() & pl.col(alias).is_finite())


def match_nearest(
    left: pl.DataFrame,
    right: pl.DataFrame,
    tolerance_seconds: float,
    value_column: str,
) -> pl.DataFrame:
    """Match prefiltered frames on `_time`, dropping unmatched right values."""
    return (
        left.sort("_time")
        .join_asof(
            right.sort("_time"),
            on="_time",
            strategy="nearest",
            tolerance=timedelta(seconds=tolerance_seconds),
        )
        .drop_nulls(value_column)
    )


def render_figure(
    creator: PlotCreator,
    dataset: TelemetryDataset,
    specification: dict[str, Any],
    output_directory: str | Path,
    *,
    dpi: int = 300,
) -> Path:
    """Create, save, and close a figure, including when writing fails."""

    figure = creator(dataset, specification)
    try:
        directory = Path(output_directory)
        directory.mkdir(parents=True, exist_ok=True)

        output_path = directory / specification["filename"].strip()
        figure.savefig(output_path, dpi=dpi)
        return output_path
    finally:
        plt.close(figure)
