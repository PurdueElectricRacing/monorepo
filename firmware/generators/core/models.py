from dataclasses import dataclass
from typing import Any


@dataclass
class CanModel:
    nodes: list
    bus_configs: dict
    custom_types: dict


@dataclass
class CompiledCan:
    context: Any
