"""
models.py

Pydantic models mirroring the FIL board and network JSON schemas, so
filgen output is type-checked rather than assembled from raw dicts.
"""

from pydantic import BaseModel, ConfigDict, Field
from typing import Annotated, Literal, Union


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


class FilScenarioRepeat(FilModel):
    """Repetition spec for a scenario CAN event."""

    every_ms: int
    count: int


class FilScenarioGpio(FilModel):
    """Sets a GPIO pin on a board."""

    type: Literal["gpio"] = "gpio"
    at_ms: int
    board: str
    pin: str
    value: bool


class FilScenarioAdc(FilModel):
    """Loads a raw ADC sample."""

    type: Literal["adc"] = "adc"
    at_ms: int
    board: str
    instance: str
    channel: int
    value: int


class FilScenarioCan(FilModel):
    """Sends or expects a CAN message, keyed by logical bus and message name."""

    type: Literal["can"] = "can"
    bus: str
    message: str
    at_ms: int
    repeat: FilScenarioRepeat | None = None
    values: dict[str, int | float | str] | None = None
    raw: list[int] | None = None
    window_ms: int | None = None


FilScenarioCanEntry = Annotated[
    Union[FilScenarioGpio, FilScenarioAdc, FilScenarioCan],
    Field(discriminator="type"),
]

FilScenarioEvent = FilScenarioCanEntry
FilScenarioExpect = FilScenarioCanEntry


class FilScenario(FilModel):
    """A hand-maintained FIL scenario (tests/fil/scenarios/*.json)."""

    schema_version: Literal[1]
    name: str
    events: list[FilScenarioEvent]
    expect: list[FilScenarioExpect]


class FilStimulusCanEvent(FilModel):
    """Resolved CAN stimulus event."""

    type: Literal["can"] = "can"
    at_ms: int
    repeat: FilScenarioRepeat | None
    bus: str
    id: str
    data: list[int]


class FilStimulusCanExpect(FilModel):
    """Resolved CAN expectation."""

    type: Literal["can"] = "can"
    bus: str
    id: str
    data: list[int]
    at_ms: int
    window_ms: int


class FilStimulus(FilModel):
    """Resolved FIL stimulus file."""

    schema_version: Literal[1]
    name: str
    events: list
    expect: list
