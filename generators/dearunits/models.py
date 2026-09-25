"""
models.py

DearUnits internal IR: the unit graph assembled from validated config.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class UnitNode:
    name: str
    scale: float
    offset: float = 0.0


@dataclass
class DimensionTerm:
    group_name: str  # a class's or another compound's name
    exponent: int


@dataclass
class EquivalenceClass:
    name: str
    base_name: str
    units: dict[str, UnitNode] = field(default_factory=dict)


@dataclass
class CompoundType:
    name: str
    base_name: str
    dimensions: tuple[DimensionTerm, ...]
    units: dict[str, UnitNode] = field(default_factory=dict)

@dataclass
class UnitGraph:
    classes: dict[str, EquivalenceClass] = field(default_factory=dict)
    compounds: dict[str, CompoundType] = field(default_factory=dict)
