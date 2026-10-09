from itertools import cycle
from typing import Any, Literal

import matplotlib.pyplot as plt
import polars as pl
from matplotlib.figure import Figure
from pydantic import Field

from ..config import NonEmptyString, PlotConfig, PositiveFiniteFloat, StrictConfig
from ..preprocessing.telemetry_dataset import TelemetryDataset
from .common import match_nearest, require_columns, signal_samples


PLOT_STYLE = {
    "marker_size": 8,
    "marker_alpha": 0.5,
    "grid_alpha": 0.25,
    "legend_location": "best",
}

# matplotlib's default color cycle — used when no explicit colors are set
DEFAULT_COLORS = [
    "tab:blue",
    "tab:orange",
    "tab:green",
    "tab:red",
    "tab:purple",
    "tab:brown",
    "tab:pink",
    "tab:gray",
    "tab:olive",
    "tab:cyan",
]


class ScatterTraceConfig(StrictConfig):
    x_column: NonEmptyString
    y_column: NonEmptyString
    label: NonEmptyString
    color: NonEmptyString | None = None


class ScatterConfig(PlotConfig):
    type: Literal["scatter"]
    match_tolerance_seconds: PositiveFiniteFloat
    traces: list[ScatterTraceConfig] = Field(min_length=1)
    x_label: NonEmptyString | None = None
    y_label: NonEmptyString | None = None


def create_scatter_figure(
    dataset: TelemetryDataset,
    specification: dict[str, Any],
) -> Figure:
    """Create a caller-owned figure with overlaid Y-vs-X scatter traces."""
    config = ScatterConfig.model_validate(specification)

    require_columns(
        dataset,
        [
            config.time_column,
            *(column for trace in config.traces
              for column in (trace.x_column, trace.y_column)),
        ],
    )

    figure, axis = plt.subplots(figsize=(config.width, config.height))

    try:
        color_cycle = cycle(DEFAULT_COLORS)

        for trace in config.traces:
            color = trace.color if trace.color is not None else next(color_cycle)
            trace_data = _match_trace(
                dataset,
                config.time_column,
                config.match_tolerance_seconds,
                trace.x_column,
                trace.y_column,
            )

            if trace_data.is_empty():
                continue

            axis.scatter(
                trace_data["x"].to_numpy(),
                trace_data["y"].to_numpy(),
                label=trace.label,
                s=PLOT_STYLE["marker_size"],
                alpha=PLOT_STYLE["marker_alpha"],
                color=color,
                edgecolors="none",
            )

        axis.set_xlabel(_axis_label(config.x_label, "x", config.traces, dataset))
        axis.set_ylabel(_axis_label(config.y_label, "y", config.traces, dataset))
        axis.legend(loc=PLOT_STYLE["legend_location"])
        axis.grid(alpha=PLOT_STYLE["grid_alpha"])
        axis.set_title(config.title)
        figure.tight_layout()

        return figure
    except BaseException:
        plt.close(figure)
        raise


def _match_trace(
    dataset: TelemetryDataset,
    time_column: str,
    tolerance_seconds: float,
    x_column: str,
    y_column: str,
) -> pl.DataFrame:
    """Time-align X and Y columns using nearest-neighbor join_asof."""
    x_data = signal_samples(dataset, time_column, x_column, "x")
    y_data = signal_samples(dataset, time_column, y_column, "y")

    return match_nearest(x_data, y_data, tolerance_seconds, "y").select("x", "y")


def _axis_label(
    override: str | None,
    axis: Literal["x", "y"],
    traces: list[ScatterTraceConfig],
    dataset: TelemetryDataset,
) -> str:
    """Determine the axis label, preferring override then auto-derived unit."""
    if override is not None:
        return override

    # Collect units from all trace columns for this axis
    columns = [trace.x_column if axis == "x" else trace.y_column for trace in traces]
    unique_columns = list(dict.fromkeys(columns))
    units = [
        dataset.unit_for(col) for col in unique_columns
        if dataset.unit_for(col) is not None
    ]

    if len(set(units)) == 1 and units:
        return f"{axis.upper()} ({units[0]})"

    return axis.upper()
