"""
models.py

Pydantic models mirroring the FIL board and network JSON schemas, so
filgen output is type-checked rather than assembled from raw dicts.
"""

from pydantic import BaseModel, ConfigDict
from typing import Literal


class FilModel(BaseModel):
    """Strict base for emitted FIL JSON documents."""

    model_config = ConfigDict(
        extra="forbid",
        strict=True,
        frozen=True,
    )


class FilBus(FilModel):
    """A single bus in a FIL network config."""

    type: Literal["can"]
    bitrate: int


class FilBoardCanAttachment(FilModel):
    """Maps a board CAN peripheral to a FIL bus key."""

    bus: str


class FilBoard(FilModel):
    """A single board config in a FIL network."""

    schema_version: int
    name: str
    mcu: str
    elf: str
    vector_base: str
    can: dict[str, FilBoardCanAttachment]


class FilNetwork(FilModel):
    """Top-level FIL network config (network.json / ready_to_drive.json)."""

    schema_version: int
    name: str
    buses: dict[str, FilBus]
    boards: list[str]
    stimuli: list[str]
