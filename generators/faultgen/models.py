"""
models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class Fault:
    name: str
    max_val: float
    min_val: float
    priority: str
    time_to_latch: int
    time_to_unlatch: int
    lcd_message: str


@dataclass(frozen=True)
class FaultNode:
    name: str
    enabled: bool
    generate_strings: bool
    busses: frozenset[str]
    tx_message_names: frozenset[str]
    faults: tuple[Fault, ...] = ()


@dataclass(frozen=True)
class FaultPlan:
    nodes: tuple[FaultNode, ...]
    fault_bus_name: str | None
    fault_id_base_type: str = "uint16_t"

    @property
    def modules(self) -> tuple[FaultNode, ...]:
        return tuple(node for node in self.nodes if node.enabled and node.faults)
