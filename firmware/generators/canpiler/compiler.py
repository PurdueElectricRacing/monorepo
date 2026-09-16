"""
compiler.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

import hashlib
from collections.abc import Iterable
from dataclasses import replace

from core.config_models import ConfigBundle, CustomTypeDeclaration, MessageDeclaration
from core.contracts import CanContribution
from core.utils import CTYPE_SIZES, print_as_ok, print_as_success, print_as_warning
from .ir import (
    BusAttachmentIR,
    CanIR,
    CanSource,
    MessageIR,
    NodeIR,
    RxSubscriptionIR,
    SignalIR,
    SourceBusAttachment,
    SourceNode,
    TxMessageIR,
    frozen_mapping,
)


def collect_declarations(
    config: ConfigBundle,
    contributions: Iterable[CanContribution],
) -> CanSource:
    """Combine configured and generated declarations before compilation."""
    custom_types = dict(config.custom_types)
    nodes: list[SourceNode] = []

    for node in config.internal_nodes:
        busses = tuple(
            SourceBusAttachment(
                bus_name=bus_name,
                peripheral=attachment.peripheral,
                tx_messages=tuple(attachment.tx),
                rx_subscriptions=tuple(attachment.rx),
                accept_all_messages=attachment.accept_all_messages,
            )
            for bus_name, attachment in node.busses.items()
        )

        nodes.append(
            SourceNode(
                node_name=node.node_name,
                busses=busses,
            )
        )

    for node in config.external_nodes:
        bus = SourceBusAttachment(
            bus_name=node.bus_name,
            peripheral="UNKNOWN",
            tx_messages=tuple(node.tx),
            rx_subscriptions=tuple(node.rx),
        )

        nodes.append(
            SourceNode(
                node_name=node.node_name,
                is_external=True,
                busses=(bus,),
            )
        )

    node_positions = {node.node_name: index for index, node in enumerate(nodes)}

    for contribution in contributions:
        for item in contribution.custom_types:
            declaration = item.declaration
            existing = custom_types.get(declaration.name)
            if item.mode == "add":
                if existing is not None:
                    raise ValueError(f"Custom type '{declaration.name}' already exists")
            elif existing is None:
                raise ValueError(
                    f"Cannot replace unknown custom type '{declaration.name}'"
                )
            elif existing.base_type != declaration.base_type:
                raise ValueError(
                    f"Replacement for custom type '{declaration.name}' changes "
                    f"base type from '{existing.base_type}' to '{declaration.base_type}'"
                )
            custom_types[declaration.name] = declaration

        for item in contribution.tx_messages:
            nodes = _append_tx(
                nodes,
                node_positions,
                item.node_name,
                item.bus_name,
                item.message,
            )

        for item in contribution.rx_subscriptions:
            nodes = _merge_rx(
                nodes,
                node_positions,
                item.node_name,
                item.bus_name,
                item.subscription,
            )

    return CanSource(
        nodes=tuple(nodes),
        bus_definitions=frozen_mapping(config.buses),
        custom_types=frozen_mapping(custom_types),
    )


def _bus_position(node: SourceNode, bus_name: str) -> int:
    for index, bus in enumerate(node.busses):
        if bus.bus_name == bus_name:
            return index
    raise ValueError(f"Node '{node.node_name}' is not attached to bus '{bus_name}'")


def _source_node(
    nodes: list[SourceNode],
    positions: dict[str, int],
    node_name: str,
) -> tuple[int, SourceNode]:
    try:
        index = positions[node_name]
    except KeyError as error:
        raise ValueError(f"Contribution targets unknown node '{node_name}'") from error
    return index, nodes[index]


def _append_tx(
    nodes: list[SourceNode],
    positions: dict[str, int],
    node_name: str,
    bus_name: str,
    message: MessageDeclaration,
) -> list[SourceNode]:
    node_index, node = _source_node(nodes, positions, node_name)
    bus_index = _bus_position(node, bus_name)
    busses = list(node.busses)
    bus = busses[bus_index]
    busses[bus_index] = replace(bus, tx_messages=bus.tx_messages + (message,))
    updated = list(nodes)
    updated[node_index] = replace(node, busses=tuple(busses))
    return updated


def _merge_rx(
    nodes,
    positions,
    node_name,
    bus_name,
    subscription,
):
    node_index, node = _source_node(nodes, positions, node_name)
    bus_index = _bus_position(node, bus_name)
    busses = list(node.busses)
    bus = busses[bus_index]
    subscriptions = list(bus.rx_subscriptions)

    for index, existing in enumerate(subscriptions):
        if existing.message_name == subscription.message_name:
            subscriptions[index] = existing.model_copy(
                update={
                    "callback": existing.callback or subscription.callback,
                }
            )
            break
    else:
        subscriptions.append(subscription)

    busses[bus_index] = replace(bus, rx_subscriptions=tuple(subscriptions))

    updated = list(nodes)
    updated[node_index] = replace(node, busses=tuple(busses))

    return updated


def compile_source(source: CanSource) -> CanIR:
    print("Compiling CAN declarations and performing semantic validation...")
    nodes = []
    tx_messages = []
    rx_subscriptions = []

    for source_node in source.nodes:
        busses = {}

        for source_bus in source_node.busses:
            bus_definition = source.bus_definitions[source_bus.bus_name]
            messages = tuple(
                _compile_message(
                    message,
                    bus_definition.is_extended_id,
                    source.custom_types,
                )
                for message in source_bus.tx_messages
            )

            bus = BusAttachmentIR(
                name=source_bus.bus_name,
                peripheral=source_bus.peripheral,
                accept_all_messages=source_bus.accept_all_messages,
            )

            tx_messages.extend(
                TxMessageIR(
                    node_name=source_node.node_name,
                    bus_name=source_bus.bus_name,
                    message=message,
                )
                for message in messages
            )
            rx_subscriptions.extend(
                RxSubscriptionIR(
                    node_name=source_node.node_name,
                    bus_name=source_bus.bus_name,
                    message_name=item.message_name,
                    callback=item.callback,
                )
                for item in source_bus.rx_subscriptions
            )

            for message in messages:
                _warn_priority_period_convention(
                    source_node.node_name,
                    bus.name,
                    message,
                )

            busses[bus.name] = bus

        nodes.append(
            NodeIR(
                source_node.node_name,
                frozen_mapping(busses),
                source_node.is_external,
            )
        )
        print_as_ok(f"Compiled {source_node.node_name}")

    print_as_success("All CAN declarations compiled successfully")

    return CanIR(
        tuple(nodes),
        tuple(tx_messages),
        tuple(rx_subscriptions),
        frozen_mapping(source.bus_definitions),
        frozen_mapping(source.custom_types),
    )


def _compile_message(
    declaration: MessageDeclaration,
    is_extended: bool,
    custom_types: dict[str, CustomTypeDeclaration],
) -> MessageIR:
    current_offset = 0
    signals = []

    for signal in declaration.signals:
        if signal.data_type in CTYPE_SIZES:
            base_type = signal.data_type
        elif signal.data_type in custom_types:
            base_type = custom_types[signal.data_type].base_type
        else:
            raise ValueError(
                f"Signal '{signal.signal_name}' in message "
                f"'{declaration.message_name}' has unknown type '{signal.data_type}'"
            )

        length = signal.length or CTYPE_SIZES[base_type]

        if declaration.byte_order == "big_endian" and length > 8:
            if current_offset % 8 != 0 or length not in (16, 32, 64):
                raise ValueError(
                    f"Signal '{signal.signal_name}' in big-endian message "
                    f"'{declaration.message_name}' must be byte-aligned with a "
                    "16, 32, or 64-bit length"
                )
            bit_offset = (current_offset // 8) * 8 + 7
            byte_order = "big_endian"
        else:
            bit_offset = current_offset
            byte_order = "little_endian"

        signals.append(
            SignalIR(
                signal_name=signal.signal_name,
                data_type=signal.data_type,
                description=signal.description,
                length=length,
                unit=signal.unit,
                choices=(
                    tuple(signal.choices)
                    if signal.choices is not None
                    else None
                ),
                scale=signal.scale,
                offset=signal.offset,
                min=signal.min,
                max=signal.max,
                byte_order=byte_order,
                bit_offset=bit_offset,
                bit_shift=current_offset,
                is_signed=base_type.startswith("int"),
                mask=(1 << length) - 1,
            )
        )
        current_offset += length

    if current_offset > 64:
        raise ValueError(
            f"Message '{declaration.message_name}' exceeds 64 bits (has {current_offset})"
        )

    id_override = int(declaration.id_override, 0) if declaration.id_override else None
    limit = 0x1FFFFFFF if is_extended else 0x7FF
    if id_override is not None and id_override > limit:
        raise ValueError(
            f"Message '{declaration.message_name}' override ID {hex(id_override)} "
            f"exceeds {'29' if is_extended else '11'}-bit limit"
        )

    layout = "".join(
        f"{signal.signal_name}:{signal.data_type}:"
        f"{signal.bit_shift}:{signal.length};"
        for signal in signals
    )
    layout_hash = f"0x{hashlib.sha256(layout.encode()).hexdigest()[:16].upper()}"
    return MessageIR(
        message_name=declaration.message_name,
        description=declaration.description,
        signals=tuple(signals),
        priority=declaration.priority,
        period_ms=declaration.period_ms,
        id_override=id_override,
        is_extended=is_extended,
        byte_order=declaration.byte_order,
        dlc=(current_offset + 7) // 8,
        layout_hash=layout_hash,
    )


def _warn_priority_period_convention(
    node_name: str,
    bus_name: str,
    msg: MessageIR,
) -> None:
    reason = None
    if msg.priority == 0 and msg.period_ms > 0:
        reason = "priority 0 is event-based, but period_ms is positive (use 0 or omit period_ms for event-triggered messages)"
    elif msg.priority in (1, 2) and msg.period_ms == 0:
        reason = f"priority {msg.priority} is periodic by convention, but period_ms is 0 (set a positive period_ms for periodic messages)"
    elif msg.priority == 4 and msg.period_ms <= 200:
        reason = "priority 4 is low-frequency periodic telemetry (<5 Hz), but period_ms is not greater than 200 ms"
    elif msg.priority == 5 and (msg.period_ms == 0 or msg.period_ms >= 200):
        reason = "priority 5 is high-frequency telemetry (>5 Hz), but period_ms is not between 1 and 199 ms"

    if reason:
        print_as_warning(
            f"Node '{node_name}': Bus '{bus_name}': Message '{msg.message_name}' "
            f"violates priority-period convention: priority={msg.priority}, "
            f"period={msg.period_ms} ms. {reason}."
        )
