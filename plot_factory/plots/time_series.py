from typing import Any, Literal

import matplotlib.pyplot as plt
import polars as pl
from matplotlib.figure import Figure
from pydantic import Field, model_validator

from ..config import NonEmptyString, PlotConfig, StrictConfig
from ..preprocessing.telemetry_dataset import TelemetryDataset
from .common import require_columns


PLOT_STYLE = {
    "line_width": 0.8,
    "grid_alpha": 0.25,
    "legend_location": "upper right",
}


class TraceConfig(StrictConfig):
    column: NonEmptyString
    label: NonEmptyString
    color: NonEmptyString | None = None


class PanelConfig(StrictConfig):
    label: NonEmptyString | None = None
    ylabel: NonEmptyString | None = None
    traces: list[TraceConfig] = Field(min_length=1)

    @model_validator(mode="after")
    def require_axis_label(self) -> "PanelConfig":
        if self.label is None and self.ylabel is None:
            raise ValueError("a panel must define either 'label' or 'ylabel'")
        return self


class TimeSeriesConfig(PlotConfig):
    type: Literal["time_series"]
    forward_fill: bool = False
    panels: list[PanelConfig] = Field(min_length=1)


def create_time_series_figure(
    dataset: TelemetryDataset,
    specification: dict[str, Any],
) -> Figure:
    """Create a caller-owned figure with vertically stacked time-series panels."""
    config = TimeSeriesConfig.model_validate(specification)

    trace_columns = [
        trace.column for panel in config.panels for trace in panel.traces
    ]

    required_columns = require_columns(dataset, [config.time_column, *trace_columns])

    plot_data = dataset.frame.select(required_columns)
    if config.forward_fill:
        plot_data = plot_data.with_columns(
            pl.col(list(dict.fromkeys(trace_columns))).forward_fill()
        )

    figure, axes = plt.subplots(
        len(config.panels),
        1,
        figsize=(config.width, config.height),
        sharex=True,
        squeeze=False,
    )

    try:
        time = plot_data[config.time_column].to_numpy()

        for panel_index, (axis, panel) in enumerate(
            zip(axes[:, 0], config.panels)
        ):
            for trace in panel.traces:
                plot_options = {
                    "label": trace.label,
                    "linewidth": PLOT_STYLE["line_width"],
                }
                if trace.color is not None:
                    plot_options["color"] = trace.color

                axis.plot(
                    time,
                    plot_data[trace.column].to_numpy(),
                    **plot_options,
                )

            axis.set_ylabel(_panel_ylabel(panel, dataset, panel_index))
            axis.legend(loc=PLOT_STYLE["legend_location"])
            axis.grid(alpha=PLOT_STYLE["grid_alpha"])

        axes[-1, 0].set_xlabel(config.time_column)
        figure.suptitle(config.title)
        figure.tight_layout()

        return figure
    except BaseException:
        plt.close(figure)
        raise


def _panel_ylabel(
    panel: PanelConfig,
    dataset: TelemetryDataset,
    panel_index: int,
) -> str:
    if panel.ylabel is not None:
        return panel.ylabel

    units = [dataset.unit_for(trace.column) for trace in panel.traces]
    known_units = {unit for unit in units if unit}

    if len(known_units) > 1:
        raise ValueError(
            f"Panel {panel_index + 1} ({panel.label!r}) contains traces with "
            f"conflicting units: {sorted(known_units)}"
        )

    if len(known_units) == 1 and all(units):
        return f"{panel.label} ({known_units.pop()})"

    # PanelConfig guarantees label is present when ylabel is absent.
    return panel.label or ""
