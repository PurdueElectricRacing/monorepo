"""
linker.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections import defaultdict
from collections.abc import Iterable

from core.utils import print_as_ok, print_as_success
from .ir import (
    CanIR,
    LinkedCan,
    LinkedMessage,
    LinkedRxSubscription,
    MessageIR,
    MessageKey,
    TxMessageIR,
    frozen_mapping,
)


def link_can(can_ir: CanIR) -> LinkedCan:
    print("Linking CAN IDs...")
    transmitters = _index_transmitters(can_ir.tx_messages)
    linked_messages = _link_messages(can_ir.tx_messages)
    subscriptions = _resolve_subscriptions(
        can_ir,
        transmitters,
        linked_messages,
    )

    print_as_success("Successfully linked all CAN IDs")

    return LinkedCan(
        nodes=can_ir.nodes,
        messages=frozen_mapping(linked_messages),
        transmitters=frozen_mapping(transmitters),
        tx_order=tuple(placed.key for placed in can_ir.tx_messages),
        subscriptions=subscriptions,
        bus_configs=can_ir.bus_definitions,
        custom_types=can_ir.custom_types,
    )


def _index_transmitters(
    messages: Iterable[TxMessageIR],
) -> dict[MessageKey, str]:
    transmitters = {}
    for placed in messages:
        previous = transmitters.get(placed.key)
        if previous is not None:
            if previous != placed.node_name:
                raise ValueError(
                    f"Bus '{placed.bus_name}': message "
                    f"'{placed.message.message_name}' is transmitted by both "
                    f"'{previous}' and '{placed.node_name}'"
                )
            raise ValueError(
                f"Node '{placed.node_name}': duplicate TX message "
                f"'{placed.message.message_name}' on bus '{placed.bus_name}'"
            )
        transmitters[placed.key] = placed.node_name
    return transmitters


def _link_messages(
    messages: Iterable[TxMessageIR],
) -> dict[MessageKey, LinkedMessage]:
    by_bus: dict[str, list[MessageIR]] = defaultdict(list)
    for placed in messages:
        by_bus[placed.bus_name].append(placed.message)

    linked = {}
    for bus_name, bus_messages in by_bus.items():
        assigned_messages = _assign_bus_ids(
            bus_name,
            bus_messages,
        )

        for message_name, message in assigned_messages.items():
            linked[MessageKey(bus_name, message_name)] = message

        print_as_ok(f"Bus '{bus_name}' linked successfully")

    return linked


def _resolve_subscriptions(
    can_ir: CanIR,
    transmitters: dict[MessageKey, str],
    messages: dict[MessageKey, LinkedMessage],
) -> tuple[LinkedRxSubscription, ...]:
    resolved = []
    for subscription in can_ir.rx_subscriptions:
        producer = transmitters.get(subscription.key)
        if producer is None:
            raise ValueError(
                f"Node '{subscription.node_name}': RX message "
                f"'{subscription.message_name}' has no transmitter on bus "
                f"'{subscription.bus_name}'"
            )
        if producer == subscription.node_name:
            raise ValueError(
                f"Node '{subscription.node_name}': invalid self-RX for "
                f"'{subscription.message_name}' on bus '{subscription.bus_name}'"
            )
        resolved.append(
            LinkedRxSubscription(
                node_name=subscription.node_name,
                bus_name=subscription.bus_name,
                message_name=subscription.message_name,
                callback=subscription.callback,
                resolved_message=messages[subscription.key],
            )
        )

    return tuple(resolved)


def _assign_bus_ids(
    bus_name: str,
    messages: Iterable[MessageIR],
) -> dict[str, LinkedMessage]:
    by_priority: dict[int, list[MessageIR]] = defaultdict(list)
    overrides_by_priority: dict[int, set[int]] = defaultdict(set)
    used_ids = set()
    for message in messages:
        by_priority[message.priority].append(message)

        if message.id_override is None:
            continue

        if message.id_override in used_ids:
            raise ValueError(
                f"Bus '{bus_name}': duplicate ID override {hex(message.id_override)}"
            )

        overrides_by_priority[message.priority].add(message.id_override)
        used_ids.add(message.id_override)

    _validate_override_order(bus_name, overrides_by_priority)

    result = {}
    next_available = 1
    for priority in sorted(by_priority):
        group_overrides = overrides_by_priority[priority]
        if group_overrides and next_available > min(group_overrides):
            raise ValueError(
                f"Bus '{bus_name}': dynamic IDs collide with priority "
                f"{priority} override"
            )
        sorted_messages = sorted(
            by_priority[priority],
            key=lambda item: item.message_name,
        )

        for message in sorted_messages:
            final_id, next_available = _select_id(
                message,
                next_available,
                used_ids,
            )
            limit = 0x1FFFFFFF if message.is_extended else 0x7FF

            if final_id > limit:
                raise ValueError(
                    f"Bus '{bus_name}': message '{message.message_name}' ID "
                    f"{hex(final_id)} exceeds CAN limit {hex(limit)}"
                )
            result[message.message_name] = LinkedMessage(
                **message.__dict__,
                final_id=final_id,
            )

        if group_overrides:
            next_available = max(next_available, max(group_overrides) + 1)

    return result


def _validate_override_order(
    bus_name: str,
    overrides_by_priority: dict[int, set[int]],
) -> None:
    max_seen = -1
    for priority in sorted(overrides_by_priority):
        overrides = overrides_by_priority[priority]
        if overrides and min(overrides) <= max_seen:
            raise ValueError(
                f"Bus '{bus_name}': priority {priority} override "
                f"{hex(min(overrides))} violates priority order"
            )
        if overrides:
            max_seen = max(max_seen, max(overrides))


def _select_id(
    message: MessageIR,
    next_available: int,
    used_ids: set[int],
) -> tuple[int, int]:
    if message.id_override is not None:
        return message.id_override, next_available

    while next_available in used_ids:
        next_available += 1

    final_id = next_available
    used_ids.add(final_id)

    return final_id, next_available + 1
