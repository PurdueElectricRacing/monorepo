"""
models.py

DearUnits internal IR: the unit graph assembled from validated config.
"""

from __future__ import annotations

from dataclasses import dataclass, field

ANGLE_QUANTITY = "angle"


@dataclass
class Unit:
    name: str
    scale: float
    offset: float = 0.0


@dataclass
class DimensionTerm:
    quantity_name: str  # a base or derived quantity's name
    exponent: int


@dataclass
class BaseQuantity:
    name: str
    base_name: str
    units: dict[str, Unit] = field(default_factory=dict)


@dataclass
class DerivedQuantity:
    name: str
    base_name: str
    dimensions: tuple[DimensionTerm, ...]
    units: dict[str, Unit] = field(default_factory=dict)

@dataclass
class Relation:
    lhs: str
    rhs: str
    result: str

@dataclass
class UnitGraph:
    base_quantities: dict[str, BaseQuantity] = field(default_factory=dict)
    derived_quantities: dict[str, DerivedQuantity] = field(default_factory=dict)
    relations: list[Relation] = field(default_factory=list)
