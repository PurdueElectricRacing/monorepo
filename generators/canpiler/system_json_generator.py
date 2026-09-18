"""Generate the complete CAN system artifact from the linked model."""

import hashlib
import json
from collections import defaultdict

from core.artifacts import Artifact
from core.utils import print_as_ok
from .export_models import (
    BusExport, MessageExport, NodeExport, SignalExport, SystemExport, VersionsExport,
)
from .pipeline_models import LinkedCan, CompiledSignal


def content_hash(buses: dict, schema_version: int = 1) -> str:
    """Return the normalized definitions' SHA-256 digest as lowercase hex."""
    encoded = json.dumps(
        {"schema_version": schema_version, "buses": buses},
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _signal(signal: CompiledSignal, linked: LinkedCan) -> SignalExport:
    custom = linked.custom_types.get(signal.data_type)
    base = custom.base_type if custom else signal.data_type
    choices = signal.choices
    if not choices:
        if custom:
            choices = custom.choices
        elif signal.data_type == "bool":
            choices = ("OFF", "ON")
    raw_type = "float32" if base == "float" else (
        "signed" if base.startswith("int") else "unsigned"
    )
    return SignalExport(
        signal_name=signal.signal_name,
        description=signal.description,
        data_type=signal.data_type,
        raw_type=raw_type,
        start_bit=signal.bit_offset,
        bit_length=signal.length,
        byte_order=signal.byte_order,
        scale=float(signal.scale if signal.scale is not None else 1),
        offset=float(signal.offset if signal.offset is not None else 0),
        minimum=float(signal.min) if signal.min is not None else None,
        maximum=float(signal.max) if signal.max is not None else None,
        unit=signal.unit or "",
        choices={str(raw): label for raw, label in enumerate(choices or ())},
    )


def generate_system_json(linked: LinkedCan, version: str) -> Artifact:
    """Translate validated, linked CAN definitions into the public JSON shape."""
    receivers = defaultdict(set)
    for subscription in linked.subscriptions:
        receivers[(subscription.bus_name, subscription.message_name)].add(
            subscription.node_name
        )
    by_bus = defaultdict(list)
    for placed in linked.tx_messages:
        by_bus[placed.bus_name].append(placed)

    buses = {}
    for bus_name, config in sorted(linked.bus_configs.items()):
        messages = []
        for placed in sorted(by_bus[bus_name], key=lambda item: (
            item.message.is_extended, item.message.final_id, item.message.message_name
        )):
            message = placed.message
            messages.append(MessageExport(
                id=message.final_id,
                is_extended_id=message.is_extended,
                message_name=message.message_name,
                transmitter=placed.node_name,
                receivers=sorted(receivers[(bus_name, message.message_name)]),
                length_bytes=message.dlc,
                nominal_period_ms=message.period_ms if message.period_ms != 0 else None,
                priority=message.priority,
                description=message.description,
                signals=[
                    _signal(signal, linked)
                    for signal in sorted(message.signals, key=lambda s: (s.bit_offset, s.signal_name))
                ],
            ))
        buses[bus_name] = BusExport(
            baud_rate=config.baud_rate,
            nodes=[
                NodeExport(name=node.name, is_external=node.is_external)
                for node in sorted(linked.nodes, key=lambda node: node.name)
                if bus_name in node.busses
            ],
            messages=messages,
        )

    bus_data = {name: bus.model_dump(mode="json") for name, bus in buses.items()}
    document = SystemExport(
        content_hash=content_hash(bus_data),
        versions=VersionsExport(schema_version=1, hash=version),
        buses=buses,
    )
    filename = f"system_{version}.json"
    print_as_ok(f"Generated {filename}")
    return Artifact("dbc", filename, json.dumps(
        document.model_dump(mode="json"), indent=2, ensure_ascii=False, allow_nan=False,
    ) + "\n")
