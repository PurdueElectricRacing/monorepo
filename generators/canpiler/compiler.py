"""
compiler.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

import hashlib
from collections.abc import Iterable, Mapping

from core.declarations import CanDeclarations, CustomTypeDeclaration, MessageDeclaration
from core.contributions import DeclarationContribution, RxDeclaration, TxDeclaration
from core.utils import (
    CTYPE_SIZES,
    print_as_error,
    print_as_ok,
    print_as_success,
    print_as_warning,
)
from .pipeline_models import (
    CanSource,
    CompiledBusAttachment,
    CompiledCan,
    CompiledMessage,
    CompiledNode,
    CompiledRxSubscription,
    CompiledSignal,
    CompiledTxMessage,
    SourceBusAttachment,
    SourceNode,
    frozen_mapping,
)


class CanCompilationError(ValueError):
    pass


def assemble_source(
    declarations: CanDeclarations,
    contributions: Iterable[DeclarationContribution],
) -> CanSource:
    """Combine configured and generated declarations before compilation."""
    custom_types = dict(declarations.custom_types)
    nodes = []
    bus_attachments = []
    tx_messages = []
    rx_subscriptions = []

    for node in declarations.internal_nodes:
        nodes.append(SourceNode(node_name=node.node_name))

        for bus_name, attachment in node.busses.items():
            bus_attachments.append(
                SourceBusAttachment(
                    node_name=node.node_name,
                    bus_name=bus_name,
                    peripheral=attachment.peripheral,
                    accept_all_messages=attachment.accept_all_messages,
                )
            )
            tx_messages.extend(
                TxDeclaration(node.node_name, bus_name, message)
                for message in attachment.tx
            )
            rx_subscriptions.extend(
                RxDeclaration(node.node_name, bus_name, subscription)
                for subscription in attachment.rx
            )

    for node in declarations.external_nodes:
        nodes.append(
            SourceNode(
                node_name=node.node_name,
                is_external=True,
            )
        )
        bus_attachments.append(
            SourceBusAttachment(
                node_name=node.node_name,
                bus_name=node.bus_name,
                peripheral="UNKNOWN",
            )
        )
        tx_messages.extend(
            TxDeclaration(node.node_name, node.bus_name, message)
            for message in node.tx
        )
        rx_subscriptions.extend(
            RxDeclaration(node.node_name, node.bus_name, subscription)
            for subscription in node.rx
        )

    attachment_keys = {
        (attachment.node_name, attachment.bus_name)
        for attachment in bus_attachments
    }

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
            _validate_target(
                attachment_keys,
                item.node_name,
                item.bus_name,
            )
            tx_messages.append(item)

        for item in contribution.rx_subscriptions:
            _validate_target(
                attachment_keys,
                item.node_name,
                item.bus_name,
            )
            _merge_rx(rx_subscriptions, item)

    return CanSource(
        nodes=tuple(nodes),
        bus_attachments=tuple(bus_attachments),
        tx_messages=tuple(tx_messages),
        rx_subscriptions=tuple(rx_subscriptions),
        bus_definitions=frozen_mapping(declarations.buses),
        custom_types=frozen_mapping(custom_types),
    )


def _validate_target(
    attachment_keys: set[tuple[str, str]],
    node_name: str,
    bus_name: str,
) -> None:
    if (node_name, bus_name) in attachment_keys:
        return

    known_nodes = {name for name, _ in attachment_keys}
    if node_name not in known_nodes:
        raise ValueError(f"Contribution targets unknown node '{node_name}'")

    raise ValueError(f"Node '{node_name}' is not attached to bus '{bus_name}'")


def _merge_rx(
    subscriptions: list[RxDeclaration],
    incoming: RxDeclaration,
) -> None:
    for index, existing in enumerate(subscriptions):
        if (
            existing.node_name == incoming.node_name
            and existing.bus_name == incoming.bus_name
            and existing.subscription.message_name
            == incoming.subscription.message_name
        ):
            subscription = existing.subscription.model_copy(
                update={
                    "callback": (
                        existing.subscription.callback
                        or incoming.subscription.callback
                    )
                }
            )
            subscriptions[index] = RxDeclaration(
                incoming.node_name,
                incoming.bus_name,
                subscription,
            )
            return

    subscriptions.append(incoming)


def compile_source(source: CanSource) -> CompiledCan:
    print("Compiling CAN declarations and performing semantic validation...")
    nodes = []

    for source_node in source.nodes:
        busses = {}

        for source_bus in source.bus_attachments:
            if source_bus.node_name != source_node.node_name:
                continue
            bus = CompiledBusAttachment(
                name=source_bus.bus_name,
                peripheral=source_bus.peripheral,
                accept_all_messages=source_bus.accept_all_messages,
            )
            busses[bus.name] = bus

        nodes.append(
            CompiledNode(
                source_node.node_name,
                frozen_mapping(busses),
                source_node.is_external,
            )
        )
        print_as_ok(f"Compiled {source_node.node_name}")

    tx_messages = []
    issues = []
    for item in source.tx_messages:
        bus_definition = source.bus_definitions[item.bus_name]
        try:
            message = compile_message(
                item.message,
                bus_definition.is_extended_id,
                source.custom_types,
            )
        except ValueError as error:
            issues.append(
                f"Node '{item.node_name}', bus '{item.bus_name}', "
                f"message '{item.message.message_name}': {error}"
            )
            continue

        _warn_priority_period_convention(
            item.node_name,
            item.bus_name,
            message,
        )
        tx_messages.append(
            CompiledTxMessage(
                node_name=item.node_name,
                bus_name=item.bus_name,
                message=message,
            )
        )

    if issues:
        count = len(issues)
        print_as_warning(
            f"CAN compilation failed with {count} "
            f"{'issue' if count == 1 else 'issues'}:"
        )
        for issue in issues:
            print_as_error(f"  {issue}")

        raise CanCompilationError("; ".join(issues))

    rx_subscriptions = tuple(
        CompiledRxSubscription(
            node_name=item.node_name,
            bus_name=item.bus_name,
            message_name=item.subscription.message_name,
            callback=item.subscription.callback,
        )
        for item in source.rx_subscriptions
    )

    print_as_success("All CAN declarations compiled successfully")

    return CompiledCan(
        tuple(nodes),
        tuple(tx_messages),
        rx_subscriptions,
        frozen_mapping(source.bus_definitions),
        frozen_mapping(source.custom_types),
    )


def compile_message(
    declaration: MessageDeclaration,
    is_extended: bool,
    custom_types: Mapping[str, CustomTypeDeclaration],
) -> CompiledMessage:
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

        choices = signal.choices
        if not choices and signal.data_type in custom_types:
            choices = custom_types[signal.data_type].choices
        if base_type == "float":
            if length != 32 or choices:
                raise ValueError(
                    f"Signal '{signal.signal_name}' in message "
                    f"'{declaration.message_name}': float requires 32 bits and no choices"
                )
        elif choices:
            value_bits = length - int(base_type.startswith("int"))
            if len(choices) > (1 << value_bits):
                raise ValueError(
                    f"Signal '{signal.signal_name}' in message "
                    f"'{declaration.message_name}': enum values do not fit {length}-bit {base_type}"
                )

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
            CompiledSignal(
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
    return CompiledMessage(
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
    msg: CompiledMessage,
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
