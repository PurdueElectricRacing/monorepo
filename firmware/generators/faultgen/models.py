from dataclasses import dataclass, field


@dataclass
class Fault:
    name: str
    max_val: float
    min_val: float
    priority: str
    time_to_latch: int
    time_to_unlatch: int
    lcd_message: str
    absolute_index: int = 0


@dataclass
class FaultNode:
    name: str
    enabled: bool
    generate_strings: bool
    busses: set[str]
    tx_message_names: set[str]
    faults: list[Fault] = field(default_factory=list)


@dataclass
class FaultPlan:
    nodes: list[FaultNode]
    fault_bus_name: str | None
    fault_id_base_type: str = "uint16_t"

    @property
    def modules(self) -> list[FaultNode]:
        return [node for node in self.nodes if node.enabled and node.faults]
