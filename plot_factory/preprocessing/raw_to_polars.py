import csv
from pathlib import Path

import polars as pl

from .telemetry_dataset import SignalMetadata, TelemetryDataset


METADATA_ROW_COUNT = 7
REAL_TIME_FORMAT = "%Y-%m-%dT%H:%M:%S.%3fZ"


def _raw_to_polars(csv_path: str | Path) -> TelemetryDataset:
    """Read a raw telemetry CSV and its signal metadata."""
    csv_path = Path(csv_path)

    with csv_path.open(newline="", encoding="utf-8") as file:
        reader = csv.reader(file)
        metadata = [next(reader) for _ in range(METADATA_ROW_COUNT)]

    column_names = ["real_time", "daq_timestamp", "metadata"]
    name_counts: dict[str, int] = {}
    signal_metadata: dict[str, SignalMetadata] = {}

    for column_index in range(3, len(metadata[0])):
        bus = metadata[0][column_index].strip()
        node = metadata[1][column_index].strip()
        message = metadata[2][column_index].strip()
        signal = metadata[4][column_index].strip()

        parts = [bus, node, message, signal]
        base_name = ".".join(part for part in parts if part)
        base_name = base_name or f"signal_{column_index}"

        name_counts[base_name] = name_counts.get(base_name, 0) + 1
        count = name_counts[base_name]
        column_name = base_name if count == 1 else f"{base_name}_{count}"
        column_names.append(column_name)

        unit = metadata[6][column_index].strip()
        signal_metadata[column_name] = SignalMetadata(
            bus=bus,
            node=node,
            message=message,
            message_description=metadata[3][column_index].strip(),
            signal=signal,
            signal_description=metadata[5][column_index].strip(),
            unit=unit or None,
        )

    # Read all columns as strings to avoid schema inference issues with mixed int/float data
    frame = pl.read_csv(
        csv_path,
        has_header=False,
        skip_rows=METADATA_ROW_COUNT,
        new_columns=column_names,
        infer_schema_length=None,
        schema_overrides={name: pl.String for name in column_names},
    ).drop("metadata")

    # Convert known columns to their proper types
    signal_columns = [col for col in column_names if col not in ("real_time", "daq_timestamp", "metadata")]
    frame = frame.with_columns([
        pl.col("real_time").str.to_datetime(format=REAL_TIME_FORMAT, strict=False),
        pl.col("daq_timestamp").cast(pl.Float64, strict=False),
        *(pl.col(col).cast(pl.Float64, strict=False) for col in signal_columns),
    ])

    return TelemetryDataset(frame=frame, signal_metadata=signal_metadata)


def load_from_folder(folder_path: str | Path) -> TelemetryDataset:
    """Read all CSV files from a folder and concatenate them into a single dataset."""
    folder_path = Path(folder_path)
    if not folder_path.is_dir():
        raise FileNotFoundError(f"Data directory not found: {folder_path}")

    csv_files = sorted(f for f in folder_path.glob("*.csv") if f.name != "combined_data.csv")

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {folder_path}")

    datasets = [_raw_to_polars(csv_file) for csv_file in csv_files]

    # Concatenate all frames
    combined_frame = pl.concat([ds.frame for ds in datasets], how="vertical_relaxed")

    # Merge signal metadata (later files override earlier ones for duplicate keys)
    merged_metadata: dict[str, SignalMetadata] = {}
    for ds in datasets:
        merged_metadata.update(ds.signal_metadata)

    return TelemetryDataset(frame=combined_frame, signal_metadata=merged_metadata)
