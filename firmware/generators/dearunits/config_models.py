"""
config_models.py

A unit graph has two node kinds:
  - equivalence classes: units that measure the same dimension and convert
    to each other by a scale/offset (e.g. "length": m, mm, in).
  - compound types: units that bridge two or more equivalence classes via
    integer exponents (e.g. "velocity": length^1 * time^-1).

Ensures the validity of the specified JSON format
"""

from __future__ import annotations

from typing import Annotated, Self

from pydantic import Field, model_validator

from core.config_models import ConfigModel


Number = int | float


def _duplicate(values: list[str]) -> str | None:
    seen: set[str] = set()
    for value in values:
        if value in seen:
            return value
        seen.add(value)
    return None


class UnitDefConfig(ConfigModel):
    symbol: str
    name: str
    scale: Number = 1.0
    offset: Number = 0.0


def _require_identity_base_unit(base_unit: str, units: list[UnitDefConfig], owner: str) -> UnitDefConfig:
    base = next((unit for unit in units if unit.symbol == base_unit), None)
    if base is None:
        raise ValueError(f"base_unit '{base_unit}' is not defined in '{owner}'")
    if base.scale != 1.0 or base.offset != 0.0:
        raise ValueError(f"base_unit '{base_unit}' of '{owner}' must have scale=1.0 and offset=0.0")
    return base


class EquivalenceClassConfig(ConfigModel):
    name: str
    base_unit: str
    units: Annotated[list[UnitDefConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_class(self) -> Self:
        duplicate = _duplicate([unit.symbol for unit in self.units])
        if duplicate is not None:
            raise ValueError(f"Duplicate symbol found in class '{self.name}': '{duplicate}'")
        _require_identity_base_unit(self.base_unit, self.units, f"class '{self.name}'")
        return self


class EquivalenceClassesConfig(ConfigModel):
    classes: list[EquivalenceClassConfig]

    @model_validator(mode="after")
    def validate_classes(self) -> Self:
        duplicate = _duplicate([item.name for item in self.classes])
        if duplicate is not None:
            raise ValueError(f"Duplicate equivalence class name: '{duplicate}'")
        return self


class DimensionTermConfig(ConfigModel):
    class_name: str
    exponent: int

    @model_validator(mode="after")
    def exponent_is_nonzero(self) -> Self:
        if self.exponent == 0:
            raise ValueError("Dimension exponent must not be zero")
        return self


class CompoundTypeConfig(ConfigModel):
    name: str
    base_unit: str
    dimensions: Annotated[list[DimensionTermConfig], Field(min_length=2)]
    units: Annotated[list[UnitDefConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_compound(self) -> Self:
        duplicate = _duplicate([term.class_name for term in self.dimensions])
        if duplicate is not None:
            raise ValueError(f"Dimension '{duplicate}' appears more than once in compound '{self.name}'")
        duplicate = _duplicate([unit.symbol for unit in self.units])
        if duplicate is not None:
            raise ValueError(f"Duplicate unit symbol in compound '{self.name}': '{duplicate}'")
        _require_identity_base_unit(self.base_unit, self.units, f"Compound '{self.name}'")
        return self


class CompoundTypesConfig(ConfigModel):
    compounds: list[CompoundTypeConfig]

    @model_validator(mode="after")
    def validate_compounds(self) -> Self:
        duplicate = _duplicate([item.name for item in self.compounds])
        if duplicate is not None:
            raise ValueError(f"Duplicate compound type name: '{duplicate}'")
        return self
