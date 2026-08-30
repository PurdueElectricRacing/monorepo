from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

from core.contracts import CustomTypeContribution

if TYPE_CHECKING:
    from canpiler.parser import Node, SystemContext


@dataclass
class CanModel:
    nodes: list[Node]
    bus_configs: dict[str, dict[str, object]]
    custom_types: dict[str, CustomTypeContribution]


@dataclass
class CompiledCan:
    context: SystemContext
