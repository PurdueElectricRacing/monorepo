"""
models.py

DearUnits internal IR: the unit graph assembled from validated config.

Nodes are unit symbols. Equivalence classes cluster the nodes that measure
the same dimension (they convert to each other by scale/offset alone).
Compound types are the edges that bridge two or more equivalence classes
together, described as a product of those classes raised to integer
exponents (e.g. velocity = length^1 * time^-1).
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class UnitNode:
    symbol: str
    name: str
    scale: float
    offset: float = 0.0


@dataclass
class DimensionTerm:
    class_name: str
    exponent: int


@dataclass
class EquivalenceClass:
    name: str
    base_symbol: str
    units: dict[str, UnitNode] = field(default_factory=dict)


@dataclass
class CompoundType:
    name: str
    base_symbol: str
    dimensions: tuple[DimensionTerm, ...]
    units: dict[str, UnitNode] = field(default_factory=dict)

@dataclass
class UnitGraph:
    classes: dict[str, EquivalenceClass] = field(default_factory=dict)
    compounds: dict[str, CompoundType] = field(default_factory=dict)
