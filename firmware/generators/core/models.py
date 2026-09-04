"""
models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

from core.config_models import BusConfig, CustomTypeConfig

if TYPE_CHECKING:
    from canpiler.parser import Node, SystemContext


@dataclass
class CanModel:
    nodes: list[Node]
    bus_configs: dict[str, BusConfig]
    custom_types: dict[str, CustomTypeConfig]


@dataclass
class CompiledCan:
    context: SystemContext
