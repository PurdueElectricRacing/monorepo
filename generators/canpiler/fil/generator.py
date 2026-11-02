"""
generator.py

Resolves FIL scenarios into raw FIL stimulus documents. Message ids come
from the linked CAN graph, buses come from the logical bus definitions, and
signal values are packed with the same rules as the generated firmware
macros, so named scenarios stay correct as configs evolve.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

from pydantic import ValidationError

from core.artifacts import Artifact
from core.config import FIL_SCENARIO_DIR, FIL_STIMULI_DIR
from core.utils import print_as_ok, print_as_success

from ..pipeline.models import CompiledMessage, CompiledSignal, LinkedCan, LinkedMessage
from .models import FilCanEvent, FilCanExpect, FilScenario, SignalValue


class FilScenarioError(ValueError):
    """A scenario is malformed or references unknown CAN names."""


FLOAT_LENGTH = 32
BYTE_SWAPPED_LENGTHS = (16, 32, 64)
BYTE_MAX = 0xFF


def _f32(value: float) -> float:
    """Round a Python float through float32, matching the firmware constants."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def load_scenario(path: Path) -> FilScenario:
    """Read and validate one scenario file."""
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise FilScenarioError(f"{path.name}: invalid JSON: {error}") from error

    try:
        return FilScenario.model_validate(data)
    except ValidationError as error:
        raise FilScenarioError(f"{path.name}: invalid scenario: {error}") from error


def generate_stimuli(linked: LinkedCan) -> list[Artifact]:
    """Emit one resolved stimulus artifact per scenario file."""
    print("Resolving FIL scenarios...")
    artifacts = []
    for path in sorted(FIL_SCENARIO_DIR.glob("*.json")):
        relative_path = f"stimuli/{path.stem}_stimuli.json"
        content = render_stimulus(linked, load_scenario(path))
        artifacts.append(Artifact("fil", relative_path, content))
        print_as_ok(f"Generated {relative_path}")

    if artifacts:
        print_as_success("FIL stimuli generated")
    return artifacts


def render_stimulus(linked: LinkedCan, scenario: FilScenario) -> str:
    """Resolve a scenario and serialize it as a FIL stimulus document."""
    events = [_event(linked, entry) for entry in scenario.events]
    expect = [_expect(linked, entry) for entry in scenario.expect]
    return _render(scenario.name, events, expect)


def _event(linked: LinkedCan, entry) -> dict:
    if not isinstance(entry, FilCanEvent):
        return entry.model_dump()
    bus_id = _bus_id(linked, entry.bus)
    message = _find_message(linked, entry.bus, entry.message)
    event: dict = {"type": "can", "at_ms": entry.at_ms}
    if entry.repeat is not None:
        event["repeat"] = {"every_ms": entry.repeat.every_ms, "count": entry.repeat.count}
    event["bus"] = bus_id
    event["id"] = f"0x{message.final_id:x}"
    event["data"] = _payload(message, entry.values, entry.raw)
    return event


def _expect(linked: LinkedCan, entry: FilCanExpect) -> dict:
    bus_id = _bus_id(linked, entry.bus)
    message = _find_message(linked, entry.bus, entry.message)
    return {
        "type": "can",
        "bus": bus_id,
        "id": f"0x{message.final_id:x}",
        "data": _payload(message, entry.values, entry.raw),
        "at_ms": entry.at_ms,
        "window_ms": entry.window_ms,
    }


def _bus_id(linked: LinkedCan, bus_name: str) -> str:
    bus = linked.bus_configs.get(bus_name)
    if bus is None:
        known = ", ".join(sorted(linked.bus_configs))
        raise FilScenarioError(f"unknown bus '{bus_name}' (known: {known})")
    return str(bus.bus_id)


def _find_message(linked: LinkedCan, bus_name: str, message_name: str) -> LinkedMessage:
    for placed in linked.tx_messages:
        if placed.bus_name == bus_name and placed.message.message_name == message_name:
            return placed.message
    for subscription in linked.subscriptions:
        if subscription.bus_name == bus_name and subscription.message_name == message_name:
            return subscription.resolved_message
    raise FilScenarioError(f"unknown CAN message '{message_name}' on bus '{bus_name}'")


def _payload(
    message: LinkedMessage,
    values: dict[str, SignalValue] | None,
    raw: list[int] | None,
) -> list[int]:
    if raw is not None and values is not None:
        raise FilScenarioError(
            f"message '{message.message_name}' sets both 'values' and 'raw'; "
            "use exactly one"
        )
    if raw is not None:
        return _raw_bytes(message, raw)

    values = values or {}
    signals = {signal.signal_name: signal for signal in message.signals}
    for name in values:
        if name not in signals:
            known = ", ".join(signals)
            raise FilScenarioError(
                f"message '{message.message_name}' has no signal '{name}' "
                f"(known: {known})"
            )

    payload = 0
    for signal in message.signals:
        if signal.signal_name not in values:
            continue
        payload |= _pack_signal(signal, values[signal.signal_name]) << signal.bit_shift
    return list(payload.to_bytes(message.dlc, "little"))


def _raw_bytes(message: CompiledMessage, raw: list[int]) -> list[int]:
    for byte in raw:
        if not 0 <= byte <= BYTE_MAX:
            raise FilScenarioError(
                f"message '{message.message_name}' raw byte {byte} is out of range"
            )
    return list(raw)


def _pack_signal(signal: CompiledSignal, value: SignalValue) -> int:
    """Encode one physical value, mirroring the generated PACK macros."""
    if isinstance(value, str):
        value = _choice_value(signal, value)
    if signal.data_type == "float" and signal.length == FLOAT_LENGTH:
        raw = int.from_bytes(struct.pack("<f", float(value)), "little")
    else:
        raw = _encode_scaled(signal, value)

    if not signal.data_type == "float":
        _check_range(signal, value, raw)

    raw &= signal.mask
    if signal.byte_order == "big_endian" and signal.length in BYTE_SWAPPED_LENGTHS:
        raw = int.from_bytes(raw.to_bytes(signal.length // 8, "little"), "big")
    return raw & signal.mask


def _encode_scaled(signal: CompiledSignal, value: SignalValue) -> int:
    """Scale one physical value in float32, mirroring the firmware PACK macros.

    The firmware stores `1/scale` and `offset` as float32 constants and packs
    with `(value - OFFSET) * PACK_COEFF`, so the arithmetic is done in float32
    rather than double precision. Doing it in double here can truncate
    differently (e.g. 0.7 at scale 0.1 gives 6 instead of 7).
    """
    scale = signal.scale if signal.scale is not None else 1.0
    offset = signal.offset if signal.offset is not None else 0.0
    coeff = _f32(1.0 / scale)
    shifted = _f32(_f32(float(value)) - _f32(float(offset)))
    return int(_f32(shifted * coeff))


def _check_range(signal: CompiledSignal, value: SignalValue, raw: int) -> None:
    """Reject physical values that do not fit the signal's width or signedness."""
    if signal.is_signed:
        low, high = -(1 << (signal.length - 1)), (1 << (signal.length - 1)) - 1
    else:
        low, high = 0, (1 << signal.length) - 1
    if not low <= raw <= high:
        raise FilScenarioError(
            f"signal '{signal.signal_name}' value {value} encodes to {raw}, "
            f"outside [{low}, {high}]"
        )


def _choice_value(signal: CompiledSignal, name: str) -> int:
    if not signal.choices:
        raise FilScenarioError(
            f"signal '{signal.signal_name}' has no enum choices for value '{name}'"
        )
    for choice in signal.choices:
        if choice.name == name:
            return choice.value
    known = ", ".join(choice.name for choice in signal.choices)
    raise FilScenarioError(
        f"unknown choice '{name}' for signal '{signal.signal_name}' "
        f"(known: {known})"
    )


def _render(name: str, events: list[dict], expect: list[dict]) -> str:
    """Serialize one stimulus document, one entry per line grouped by type."""
    lines = [
        "{",
        f"  {json.dumps('schema_version')}: 1,",
        f"  {json.dumps('name')}: {json.dumps(name)},",
        f"  {json.dumps('events')}: [",
    ]
    lines.extend(_render_entries(events))
    lines.append("  ],")
    lines.append(f"  {json.dumps('expect')}: [")
    lines.extend(_render_entries(expect))
    lines.extend(["  ]", "}", ""])
    return "\n".join(lines)


def _render_entries(entries: list[dict]) -> list[str]:
    """Render entries as compact one-per-line JSON, separated by blank lines."""
    lines = []
    previous_type = None
    for index, entry in enumerate(entries):
        if previous_type is not None and entry["type"] != previous_type:
            lines.append("")
        separator = "," if index != len(entries) - 1 else ""
        lines.append(f"    {json.dumps(entry)}{separator}")
        previous_type = entry["type"]
    return lines
