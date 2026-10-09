"""Shared library of derived signal channels.

Derived channels are defined once in ``analysis/configuration/derived_signals/*.toml`` files and
referenced by name from plot configurations::

    [[channel]]
    name = "fl.Fz"
    expression = "VCAN.DRIVELINE.front_shockpots.left * sqrt(2)"
    unit = "mm"
    label = "Front left vertical load proxy"

Files are loaded in sorted filename order; a channel may only reference raw
telemetry columns or channels defined *earlier* (in an earlier file, or above
in the same file), which keeps composition unambiguous.

Loading validates everything up front — duplicate names, collisions with raw
telemetry columns, unknown references, and expression syntax — so a bad
library file fails loudly at startup rather than mid-plot.
"""

from __future__ import annotations

import difflib
import tomllib
from dataclasses import dataclass
from pathlib import Path

import polars as pl
from pydantic import ConfigDict, Field

from ..config import NonEmptyString, StrictConfig
from ..preprocessing.telemetry_dataset import SignalMetadata, TelemetryDataset
from .expression import ExpressionError, parse_expression


class ChannelDefinition(StrictConfig):
    """A single derived channel in the signal library."""

    name: NonEmptyString
    expression: NonEmptyString
    unit: NonEmptyString | None = None
    label: NonEmptyString | None = None


class SignalLibraryFile(StrictConfig):
    """The schema of a single analysis/configuration/derived_signals/*.toml file."""

    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    channels: list[ChannelDefinition] = Field(alias="channel", min_length=1)


@dataclass(frozen=True)
class _CompiledChannel:
    definition: ChannelDefinition
    expression: pl.Expr
    references: tuple[str, ...]


class SignalLibrary:
    """All derived channels from ``analysis/configuration/derived_signals/*.toml``, in load order."""

    def __init__(self, channels: dict[str, _CompiledChannel]) -> None:
        self._channels = channels

    # --- loading -------------------------------------------------------------

    @classmethod
    def load_for_dataset(
        cls, directory: str | Path, dataset: TelemetryDataset
    ) -> "SignalLibrary":
        """Load all library files and validate them against the dataset's columns.

        Raises ``FileNotFoundError`` if the directory is missing and ``ValueError``
        for any validation failure (bad TOML, schema, duplicate or colliding
        names, invalid expressions).
        """
        directory = Path(directory)
        if not directory.is_dir():
            raise FileNotFoundError(f"Signal library directory not found: {directory}")

        files = [(path, cls._read_file(path)) for path in sorted(directory.glob("*.toml"))]

        return cls._compile(
            files,
            raw_columns=frozenset(dataset.frame.columns),
            source_label=str(directory),
        )

    @staticmethod
    def _read_file(path: Path) -> SignalLibraryFile:
        try:
            with path.open("rb") as handle:
                raw = tomllib.load(handle)
        except tomllib.TOMLDecodeError as error:
            raise ValueError(f"Invalid TOML in signal library file {path.name}: {error}") from error

        try:
            return SignalLibraryFile.model_validate(raw)
        except Exception as error:  # pydantic.ValidationError
            raise ValueError(f"Invalid signal library file {path.name}: {error}") from error

    @classmethod
    def _compile(
        cls,
        files: list[tuple[Path, SignalLibraryFile]],
        raw_columns: frozenset[str],
        source_label: str,
    ) -> "SignalLibrary":
        channels: dict[str, _CompiledChannel] = {}

        for path, library_file in files:
            for definition in library_file.channels:
                # Channels defined earlier (in this or a previous file) are
                # valid references for this one.
                known_names = frozenset(raw_columns | channels.keys())

                if definition.name in raw_columns:
                    raise ValueError(
                        f"{path.name}: channel '{definition.name}' collides with an "
                        "existing telemetry column"
                    )
                if definition.name in channels:
                    raise ValueError(
                        f"{path.name}: duplicate channel name '{definition.name}' "
                        f"(first defined in {source_label})"
                    )

                try:
                    parsed = parse_expression(definition.expression, known_names)
                except ExpressionError as error:
                    raise ValueError(
                        f"{path.name}: invalid expression for channel "
                        f"'{definition.name}': {error}"
                    ) from error

                channels[definition.name] = _CompiledChannel(
                    definition=definition,
                    expression=parsed.expression,
                    references=parsed.references,
                )

        return cls(channels)

    # --- queries -------------------------------------------------------------

    def __contains__(self, name: str) -> bool:
        return name in self._channels

    def names(self) -> list[str]:
        return list(self._channels)

    def unit_for(self, name: str) -> str | None:
        channel = self._channels.get(name)
        return channel.definition.unit if channel else None

    def _unknown_name_message(self, name: str) -> str:
        suggestions = difflib.get_close_matches(name, self._channels, n=3, cutoff=0.7)
        suffix = f" Did you mean: {', '.join(suggestions)}?" if suggestions else ""
        return (
            f"Unknown derived channel {name!r}.{suffix} Available channels: "
            f"{', '.join(self._channels) or '(none)'}"
        )

    # --- materialization -----------------------------------------------------

    def _dependency_order(self, names: list[str]) -> list[_CompiledChannel]:
        """The transitive closure of ``names``, in evaluation order.

        ``self._channels`` is already in a valid evaluation order (a channel
        may only reference channels defined earlier, enforced at compile
        time), so a simple pass in insertion order suffices.
        """
        for name in names:
            if name not in self._channels:
                raise ValueError(self._unknown_name_message(name))

        needed: set[str] = set()
        stack = list(names)
        while stack:
            name = stack.pop()
            if name in needed:
                continue
            needed.add(name)
            # Only follow references that are themselves channels; the rest are
            # raw telemetry columns that the dataset already provides.
            stack.extend(
                ref for ref in self._channels[name].references if ref in self._channels
            )

        return [channel for name, channel in self._channels.items() if name in needed]

    def enrich(self, dataset: TelemetryDataset, names: list[str]) -> TelemetryDataset:
        """Return a dataset with the requested channels materialized as columns.

        Only channels actually referenced by ``names`` (and their dependencies)
        are computed.
        """
        if not names:
            return dataset

        ordered = self._dependency_order(names)
        wanted = {channel.definition.name for channel in ordered}

        referenced_raw = {
            ref for channel in ordered for ref in channel.references
        } - wanted
        missing_raw = sorted(
            ref for ref in referenced_raw if ref not in dataset.frame.columns
        )
        if missing_raw:
            raise ValueError(
                f"Signal library channels reference unknown columns: {missing_raw}"
            )

        # Channels may reference earlier channels, and with_columns evaluates
        # every expression against the incoming frame, so materialize one
        # column at a time in dependency order.
        frame = dataset.frame
        for channel in ordered:
            frame = frame.with_columns(
                channel.expression.alias(channel.definition.name)
            )

        metadata = dict(dataset.signal_metadata)
        for name in wanted:
            definition = self._channels[name].definition
            metadata[name] = SignalMetadata(
                bus="derived",
                node="",
                message="",
                message_description="",
                signal=name,
                signal_description=definition.label or "",
                unit=definition.unit,
            )

        return TelemetryDataset(frame=frame, signal_metadata=metadata)
