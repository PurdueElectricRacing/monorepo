from dataclasses import dataclass
from typing import Mapping

import polars as pl


@dataclass(frozen=True)
class SignalMetadata:
    bus: str
    node: str
    message: str
    message_description: str
    signal: str
    signal_description: str
    unit: str | None


@dataclass(frozen=True)
class TelemetryDataset:
    frame: pl.DataFrame
    signal_metadata: Mapping[str, SignalMetadata]

    def unit_for(self, column: str) -> str | None:
        metadata = self.signal_metadata.get(column)
        return metadata.unit if metadata else None
