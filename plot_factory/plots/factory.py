from pathlib import Path
from typing import Any

from matplotlib.figure import Figure

from ..preprocessing.telemetry_dataset import TelemetryDataset
from .common import PlotCreator, render_figure
from .gps_scatter import create_gps_scatter_figure
from .scatter import create_scatter_figure
from .time_series import create_time_series_figure


PLOT_CREATORS: dict[str, PlotCreator] = {
    "gps_scatter": create_gps_scatter_figure,
    "scatter": create_scatter_figure,
    "time_series": create_time_series_figure,
}


def _plot_type(specification: dict[str, Any]) -> str:
    plot_type = specification.get("type")
    if not isinstance(plot_type, str) or not plot_type:
        raise ValueError("Plot configuration must define a non-empty 'type'")

    if plot_type not in PLOT_CREATORS:
        available_types = ", ".join(sorted(PLOT_CREATORS))
        raise ValueError(
            f"Unknown plot type '{plot_type}'. Available types: {available_types}"
        )

    return plot_type


def create_plot(dataset: TelemetryDataset, specification: dict[str, Any]) -> Figure:
    """Create a figure for customization; the caller must close it after use."""
    return PLOT_CREATORS[_plot_type(specification)](dataset, specification)


def render_plot(
    dataset: TelemetryDataset,
    specification: dict[str, Any],
    output_directory: str | Path,
) -> Path:
    """Create, save, and close a plot using its configured creator."""
    return render_figure(create_plot, dataset, specification, output_directory)
