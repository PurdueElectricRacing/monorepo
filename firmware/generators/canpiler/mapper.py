"""
mapper.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections import defaultdict
from dataclasses import dataclass
from typing import Mapping

from .ir import CompiledNode, LinkedCan, LinkedMessage, frozen_mapping
from core.utils import print_as_warning

# Maximum FDCAN filter counts (STM32G4)
MAX_FDCAN_SID_FILTERS = 28
MAX_FDCAN_XID_FILTERS = 8


@dataclass(frozen=True)
class FilterBank:
    bank_idx: int
    msg1: LinkedMessage
    msg2: LinkedMessage | None = None


@dataclass(frozen=True)
class FdcanFilters:
    accept_all: bool
    std_ids: tuple[LinkedMessage, ...] = ()
    ext_ids: tuple[LinkedMessage, ...] = ()


@dataclass(frozen=True)
class BxcanFilters:
    accept_all: bool
    accept_bank_idx: int
    banks: tuple[FilterBank, ...] = ()


PeripheralFilters = FdcanFilters | BxcanFilters


@dataclass(frozen=True)
class NodeHardwareMap:
    filters: Mapping[str, PeripheralFilters]


HardwareMap = Mapping[str, NodeHardwareMap]


def map_hardware(linked: LinkedCan) -> HardwareMap:
    """
    Hardware Mapper stage.
    Assigns physical resources (like bxCAN filter banks or FDCAN filter lists) to nodes.
    """
    mappings = {}

    for node in linked.nodes:
        if node.is_external:
            continue

        mappings[node.name] = map_node_hardware(node, linked)

    return frozen_mapping(mappings)


def is_fdcan_peripheral(periph: str) -> bool:
    """Check if peripheral is FDCAN (G4) vs bxCAN (F4)"""
    return periph.startswith("FDCAN")


def map_node_hardware(
    node: CompiledNode,
    linked: LinkedCan,
) -> NodeHardwareMap:
    peripherals = sorted({bus.peripheral for bus in node.busses.values()})

    periph_to_buses: dict[str, list[str]] = defaultdict(list)

    for bus_name, bus in node.busses.items():
        periph_to_buses[bus.peripheral].append(bus_name)

    for periph, bus_names in sorted(periph_to_buses.items()):
        unique_buses = sorted(set(bus_names))
        if len(unique_buses) > 1:
            print_as_warning(
                f"Node '{node.name}': peripheral '{periph}' is used by multiple logical buses "
                f"({', '.join(unique_buses)}). RX filters are merged per peripheral; confirm this is intended."
            )

    # Group linked RX messages by peripheral.
    periph_to_msgs: dict[str, list[LinkedMessage]] = {
        peripheral: []
        for peripheral in peripherals
    }

    for subscription in linked.subscriptions:
        if subscription.node_name != node.name:
            continue

        attachment = node.busses[subscription.bus_name]
        periph_to_msgs[attachment.peripheral].append(subscription.resolved_message)

    filters = {}
    for periph in peripherals:
        msgs = periph_to_msgs[periph]
        accept_all = any(
            bus.accept_all_messages
            for bus in node.busses.values()
            if bus.peripheral == periph
        )

        if is_fdcan_peripheral(periph):
            filters[periph] = map_fdcan_filters(
                node.name,
                periph,
                msgs,
                accept_all,
            )
        else:
            filters[periph] = map_bxcan_filters(
                node.name,
                periph,
                msgs,
                accept_all,
            )

    return NodeHardwareMap(filters=frozen_mapping(filters))


def map_fdcan_filters(
    node_name: str,
    periph: str,
    msgs: list[LinkedMessage],
    accept_all: bool,
) -> FdcanFilters:
    """Map messages to FDCAN standard and extended ID filter lists"""
    std_ids = tuple(msg for msg in msgs if not msg.is_extended)
    ext_ids = tuple(msg for msg in msgs if msg.is_extended)

    # Check filter limits
    if len(std_ids) > MAX_FDCAN_SID_FILTERS:
        raise ValueError(
            f"Node '{node_name}' exceeds FDCAN standard ID filter limit for {periph} "
            f"({len(std_ids)} > {MAX_FDCAN_SID_FILTERS})"
        )
    if len(ext_ids) > MAX_FDCAN_XID_FILTERS:
        raise ValueError(
            f"Node '{node_name}' exceeds FDCAN extended ID filter limit for {periph} "
            f"({len(ext_ids)} > {MAX_FDCAN_XID_FILTERS})"
        )

    return FdcanFilters(
        accept_all=accept_all,
        std_ids=std_ids,
        ext_ids=ext_ids,
    )


def map_bxcan_filters(
    node_name: str,
    periph: str,
    msgs: list[LinkedMessage],
    accept_all: bool,
) -> BxcanFilters:
    """Map messages to bxCAN filter banks"""
    banks = []

    # bxCAN filter bank assignment
    # CAN1: 0-13, CAN2: 14-27
    bank_offset = 0 if periph == "CAN1" else 14
    max_bank = 13 if periph == "CAN1" else 27

    for i in range(0, len(msgs), 2):
        bank_idx = bank_offset + (i // 2)

        if bank_idx > max_bank:
            raise ValueError(
                f"Node '{node_name}' exceeds available bxCAN filter banks for {periph} (limit 14 filters)."
            )

        msg1 = msgs[i]
        msg2 = msgs[i + 1] if i + 1 < len(msgs) else None

        fb = FilterBank(
            bank_idx=bank_idx,
            msg1=msg1,
            msg2=msg2,
        )
        banks.append(fb)

    return BxcanFilters(
        accept_all=accept_all,
        accept_bank_idx=bank_offset,
        banks=tuple(banks),
    )
