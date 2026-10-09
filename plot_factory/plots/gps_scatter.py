import math
from typing import Any, Literal

import matplotlib.pyplot as plt
import polars as pl
from matplotlib.figure import Figure
from pydantic import model_validator

from ..config import FiniteFloat, NonEmptyString, PlotConfig, PositiveFiniteFloat
from ..preprocessing.telemetry_dataset import TelemetryDataset
from .common import match_nearest, require_columns, signal_samples


EARTH_RADIUS_METERS = 6_371_008.8
PLOT_STYLE = {
    "colormap": "viridis",
    "marker_size": 6,
    "marker_alpha": 0.8,
    "grid_alpha": 0.25,
}


class GPSScatterConfig(PlotConfig):
    type: Literal["gps_scatter"]
    width: PositiveFiniteFloat
    height: PositiveFiniteFloat
    longitude_column: NonEmptyString
    latitude_column: NonEmptyString
    color_column: NonEmptyString
    color_label: NonEmptyString
    match_tolerance_seconds: PositiveFiniteFloat
    max_distance_meters: PositiveFiniteFloat | None = None
    colormap: NonEmptyString = PLOT_STYLE["colormap"]
    color_min: FiniteFloat | None = None
    color_max: FiniteFloat | None = None

    @model_validator(mode="after")
    def validate_color_limits(self) -> "GPSScatterConfig":
        if (self.color_min is None) != (self.color_max is None):
            raise ValueError("color_min and color_max must be provided together")

        if (
            self.color_min is not None
            and self.color_max is not None
            and self.color_min >= self.color_max
        ):
            raise ValueError("color_min must be less than color_max")
        return self


def create_gps_scatter_figure(
    dataset: TelemetryDataset,
    specification: dict[str, Any],
) -> Figure:
    """Create a caller-owned GPS figure colored by another signal."""
    config = GPSScatterConfig.model_validate(specification)
    plot_data = _prepare_plot_data(dataset, config)

    figure, axis = plt.subplots(figsize=(config.width, config.height))

    try:
        color_options: dict[str, Any] = {"cmap": config.colormap}
        if config.color_min is not None and config.color_max is not None:
            color_options.update(vmin=config.color_min, vmax=config.color_max)

        scatter = axis.scatter(
            plot_data["east"].to_numpy(),
            plot_data["north"].to_numpy(),
            c=plot_data["color"].to_numpy(),
            s=PLOT_STYLE["marker_size"],
            alpha=PLOT_STYLE["marker_alpha"],
            edgecolors="none",
            **color_options,
        )

        axis.set_xlabel("east [m]")
        axis.set_ylabel("north [m]")
        axis.set_aspect("equal", adjustable="box")
        axis.grid(alpha=PLOT_STYLE["grid_alpha"])
        axis.set_title(config.title)

        colorbar = figure.colorbar(scatter, ax=axis)
        colorbar.set_label(_colorbar_label(dataset, config))
        figure.tight_layout()

        return figure
    except BaseException:
        plt.close(figure)
        raise


def _prepare_plot_data(
    dataset: TelemetryDataset,
    config: GPSScatterConfig,
) -> pl.DataFrame:
    require_columns(
        dataset,
        [
            config.time_column,
            config.longitude_column,
            config.latitude_column,
            config.color_column,
        ],
    )

    gps_data = dataset.frame.select(
        pl.col(config.time_column).alias("_time"),
        pl.col(config.longitude_column).alias("_longitude"),
        pl.col(config.latitude_column).alias("_latitude"),
    ).filter(
        pl.col("_time").is_not_null()
        & pl.col("_longitude").is_not_null()
        & pl.col("_longitude").is_finite()
        & pl.col("_latitude").is_not_null()
        & pl.col("_latitude").is_finite()
        & (pl.col("_longitude") != 0)
        & (pl.col("_latitude") != 0)
        & pl.col("_longitude").is_between(-180, 180, closed="both")
        & pl.col("_latitude").is_between(-90, 90, closed="both")
    )

    if gps_data.is_empty():
        raise ValueError("No valid GPS fixes remain after coordinate validation")

    longitude_origin = float(gps_data["_longitude"].median())
    latitude_origin = float(gps_data["_latitude"].median())
    longitude_scale = (
        math.pi
        / 180
        * EARTH_RADIUS_METERS
        * math.cos(math.radians(latitude_origin))
    )
    latitude_scale = math.pi / 180 * EARTH_RADIUS_METERS

    gps_data = gps_data.with_columns(
        (
            (pl.col("_longitude") - longitude_origin) * longitude_scale
        ).alias("east"),
        ((pl.col("_latitude") - latitude_origin) * latitude_scale).alias("north"),
    )

    if config.max_distance_meters is not None:
        gps_data = gps_data.filter(
            (pl.col("east").pow(2) + pl.col("north").pow(2)).sqrt()
            <= config.max_distance_meters
        )

    if gps_data.is_empty():
        raise ValueError("No GPS fixes remain after applying the distance filter")

    color_data = signal_samples(
        dataset, config.time_column, config.color_column, "color"
    )

    if color_data.is_empty():
        raise ValueError(f"No valid color values found in '{config.color_column}'")

    plot_data = match_nearest(
        gps_data, color_data, config.match_tolerance_seconds, "color"
    ).select("east", "north", "color")

    if plot_data.is_empty():
        raise ValueError(
            "No GPS fixes could be matched to color values within "
            f"{config.match_tolerance_seconds} seconds"
        )

    return plot_data


def _colorbar_label(
    dataset: TelemetryDataset,
    config: GPSScatterConfig,
) -> str:
    unit = dataset.unit_for(config.color_column)
    return f"{config.color_label} ({unit})" if unit else config.color_label
