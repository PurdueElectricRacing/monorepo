"""
mapper.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass, field
from collections import defaultdict
from typing import Any, List, Dict, Optional
from .parser import Node, Message
from core.utils import print_as_warning

# Maximum FDCAN filter counts (STM32G4)
MAX_FDCAN_SID_FILTERS = 28
MAX_FDCAN_XID_FILTERS = 8


@dataclass
class FilterBank:
    """bxCAN filter bank (F4)"""

    bank_idx: int
    msg1: Optional[Message] = None
    msg2: Optional[Message] = None
    is_ext1: bool = False
    is_ext2: bool = False


@dataclass
class FDCANFilters:
    """FDCAN filter lists (G4)"""

    std_ids: List[Message] = field(default_factory=list)  # Standard ID messages
    ext_ids: List[Message] = field(default_factory=list)  # Extended ID messages


@dataclass
class NodeMapping:
    node_name: str
    # Peripheral -> List of FilterBanks (for bxCAN)
    filters: Dict[str, List[FilterBank]] = field(default_factory=dict)
    # Peripheral -> FDCANFilters (for FDCAN)
    fdcan_filters: Dict[str, FDCANFilters] = field(default_factory=dict)
    # Peripheral -> bool
    accept_all: Dict[str, bool] = field(default_factory=dict)


def map_hardware(
    nodes: List[Node], bus_configs: Dict[str, Dict[str, Any]]
) -> Dict[str, NodeMapping]:
    """
    Hardware Mapper stage.
    Assigns physical resources (like bxCAN filter banks or FDCAN filter lists) to nodes.
    """
    mappings = {}
    for node in nodes:
        if node.is_external:
            continue
        mappings[node.name] = map_node_hardware(node, bus_configs)
    return mappings


def is_fdcan_peripheral(periph: str) -> bool:
    """Check if peripheral is FDCAN (G4) vs bxCAN (F4)"""
    return periph.startswith("FDCAN")


def map_node_hardware(
    node: Node, bus_configs: Dict[str, Dict[str, Any]]
) -> NodeMapping:
    mapping = NodeMapping(node_name=node.name)

    peripherals = sorted(list(set(bus.peripheral for bus in node.busses.values())))

    periph_to_buses: Dict[str, List[str]] = defaultdict(list)
    for bus_name, bus in node.busses.items():
        periph_to_buses[bus.peripheral].append(bus_name)
    for periph, bus_names in sorted(periph_to_buses.items()):
        unique_buses = sorted(set(bus_names))
        if len(unique_buses) > 1:
            print_as_warning(
                f"Node '{node.name}': peripheral '{periph}' is used by multiple logical buses "
                f"({', '.join(unique_buses)}). RX filters are merged per peripheral; confirm this is intended."
            )

    # Group RX messages by peripheral
    periph_to_msgs: Dict[str, List[tuple[Message, str]]] = {p: [] for p in peripherals}
    for bus_name, bus in node.busses.items():
        for rx_msg in bus.rx_messages:
            if rx_msg.resolved_message:
                periph_to_msgs[bus.peripheral].append(
                    (rx_msg.resolved_message, bus_name)
                )

    for periph in peripherals:
        msgs = periph_to_msgs[periph]
        mapping.accept_all[periph] = any(
            bus.accept_all_messages
            for bus in node.busses.values()
            if bus.peripheral == periph
        )

        if is_fdcan_peripheral(periph):
            # FDCAN filter mapping (G4)
            mapping.fdcan_filters[periph] = map_fdcan_filters(node.name, periph, msgs)
            mapping.filters[periph] = []  # Empty for FDCAN
        else:
            # bxCAN filter bank mapping (F4)
            mapping.filters[periph] = map_bxcan_filters(node.name, periph, msgs)
            mapping.fdcan_filters[periph] = FDCANFilters()  # Empty for bxCAN

    return mapping


def map_fdcan_filters(
    node_name: str, periph: str, msgs: List[tuple[Message, str]]
) -> FDCANFilters:
    """Map messages to FDCAN standard and extended ID filter lists"""
    filters = FDCANFilters()

    if not msgs:
        return filters

    for msg, bus_name in msgs:
        if msg.is_extended:
            filters.ext_ids.append(msg)
        else:
            filters.std_ids.append(msg)

    # Check filter limits
    if len(filters.std_ids) > MAX_FDCAN_SID_FILTERS:
        raise ValueError(
            f"Node '{node_name}' exceeds FDCAN standard ID filter limit for {periph} "
            f"({len(filters.std_ids)} > {MAX_FDCAN_SID_FILTERS})"
        )
    if len(filters.ext_ids) > MAX_FDCAN_XID_FILTERS:
        raise ValueError(
            f"Node '{node_name}' exceeds FDCAN extended ID filter limit for {periph} "
            f"({len(filters.ext_ids)} > {MAX_FDCAN_XID_FILTERS})"
        )

    return filters


def map_bxcan_filters(node_name: str, periph: str, msgs: List[tuple[Message, str]]) -> List[FilterBank]:
    """Map messages to bxCAN filter banks"""
    banks: List[FilterBank] = []

    if not msgs:
        return banks

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

        msg1, bus1 = msgs[i]
        msg2, bus2 = msgs[i + 1] if i + 1 < len(msgs) else (None, None)

        is_ext1 = msg1.is_extended
        is_ext2 = False
        if msg2:
            is_ext2 = msg2.is_extended

        fb = FilterBank(
            bank_idx=bank_idx, msg1=msg1, msg2=msg2, is_ext1=is_ext1, is_ext2=is_ext2
        )
        banks.append(fb)

    return banks
