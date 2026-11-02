"""
models.py

Hand-maintained FIL scenario schema. Scenarios reference CAN traffic by
logical bus, message, and signal names; the fil pass resolves those names
into the raw ids and packed payloads FIL consumes.
"""

from __future__ import annotations

from typing import Annotated, Literal, Union

from pydantic import Field

from core.declarations import DeclarationModel

SignalValue = bool | int | float | str


class FilRepeat(DeclarationModel):
    every_ms: int
    count: int


class FilGpioEvent(DeclarationModel):
    type: Literal["gpio"]
    at_ms: int
    board: str
    pin: str
    value: bool


class FilAdcEvent(DeclarationModel):
    type: Literal["adc"]
    at_ms: int
    board: str
    instance: str
    channel: int
    value: int


class FilCanEvent(DeclarationModel):
    type: Literal["can"]
    bus: str
    message: str
    at_ms: int
    repeat: FilRepeat | None = None
    values: dict[str, SignalValue] | None = None
    raw: list[int] | None = None


class FilCanExpect(DeclarationModel):
    type: Literal["can"]
    bus: str
    message: str
    at_ms: int
    window_ms: int
    values: dict[str, SignalValue] | None = None
    raw: list[int] | None = None


FilEvent = Annotated[
    Union[FilGpioEvent, FilAdcEvent, FilCanEvent],
    Field(discriminator="type"),
]


class FilScenario(DeclarationModel):
    schema_version: Literal[1]
    name: str
    events: list[FilEvent]
    expect: list[FilCanExpect]
