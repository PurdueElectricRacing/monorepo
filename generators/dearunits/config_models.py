"""
config_models.py

A unit graph has two node kinds:
  - base quantities: equivalent dimensional units that can convert to one another
  - derived quantities: units cross compiled across different base quantities

Ensures the validity of the specified JSON format
"""

from __future__ import annotations

from typing import Annotated, Self

from pydantic import Field, model_validator

from core.declarations import DeclarationModel

Number = int | float

def _duplicate(values: list[str]) -> str | None:
    seen: set[str] = set()
    for value in values:
        if value in seen:
            return value
        seen.add(value)
    return None

def _require_base_not_duplicated(base_unit: str, unit_names: list[str], owner: str) -> None:
    if base_unit in unit_names:
        raise ValueError(
            f"base_unit '{base_unit}' of {owner} must not also appear in 'units' -- "
            "it's implicit (scale=1.0), listing it would define it twice"
        )

def _require_unique_group_names(composed_of: list[DimensionTermConfig], owner: str) -> None:
    duplicate = _duplicate([term.group_name for term in composed_of])
    if duplicate is not None:
        raise ValueError(f"{owner} references group '{duplicate}' more than once in composed_of")

class UnitConfig(DeclarationModel):
    name: str
    scale: Number = 1.0
    offset: Number = 0.0

    @model_validator(mode="after")
    def scale_is_nonzero(self) -> Self:
        if self.scale == 0:
            raise ValueError(f"unit '{self.name}' has scale=0, which is never a valid conversion factor")
        return self

class DimensionTermConfig(DeclarationModel):
    group_name: str
    unit_name: str
    exponent: int

    @model_validator(mode="after")
    def exponent_is_nonzero(self) -> Self:
        if self.exponent == 0:
            raise ValueError("Composition exponent must not be zero")
        return self

class DerivedUnitConfig(DeclarationModel):
    name: str
    scale: Number | None = None
    composed_of: Annotated[list[DimensionTermConfig], Field(min_length=1)] | None = None

    @model_validator(mode="after")
    def exactly_one_of_scale_or_composed_of(self) -> Self:
        if (self.scale is None) == (self.composed_of is None):
            raise ValueError(f"unit '{self.name}' must set exactly one of 'scale' or 'composed_of'")
        return self

    @model_validator(mode="after")
    def scale_is_nonzero(self) -> Self:
        if self.scale == 0:
            raise ValueError(f"unit '{self.name}' has scale=0, which is never a valid conversion factor")
        return self

    @model_validator(mode="after")
    def composed_of_group_names_unique(self) -> Self:
        if self.composed_of is not None:
            _require_unique_group_names(self.composed_of, f"unit '{self.name}'")
        return self

class BaseQuantityConfig(DeclarationModel):
    name: str
    base_unit: str
    units: Annotated[list[UnitConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_base_quantity(self) -> Self:
        unit_names = [unit.name for unit in self.units]
        duplicate = _duplicate(unit_names)
        if duplicate is not None:
            raise ValueError(f"Duplicate unit name found in base quantity '{self.name}': '{duplicate}'")
        _require_base_not_duplicated(self.base_unit, unit_names, f"base quantity '{self.name}'")
        return self

class BaseQuantitiesConfig(DeclarationModel):
    classes: list[BaseQuantityConfig]

    @model_validator(mode="after")
    def validate_base_quantities(self) -> Self:
        duplicate = _duplicate([item.name for item in self.classes])
        if duplicate is not None:
            raise ValueError(f"Duplicate base quantity name: '{duplicate}'")
        return self

class DerivedQuantityConfig(DeclarationModel):
    name: str
    base_unit: str
    composed_of: Annotated[list[DimensionTermConfig], Field(min_length=1)]
    units: Annotated[list[DerivedUnitConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_derived_quantity(self) -> Self:
        unit_names = [unit.name for unit in self.units]
        duplicate = _duplicate(unit_names)
        if duplicate is not None:
            raise ValueError(f"Duplicate unit name in derived quantity '{self.name}': '{duplicate}'")
        _require_base_not_duplicated(self.base_unit, unit_names, f"derived quantity '{self.name}'")
        _require_unique_group_names(self.composed_of, f"derived quantity '{self.name}''s base_unit")
        return self

class RelationConfig(DeclarationModel):
    lhs: str
    rhs: str
    result: str

class DerivedQuantitiesConfig(DeclarationModel):
    compounds: list[DerivedQuantityConfig]
    relations: list[RelationConfig] = []

    @model_validator(mode="after")
    def validate_derived_quantities(self) -> Self:
        duplicate = _duplicate([item.name for item in self.compounds])
        if duplicate is not None:
            raise ValueError(f"Duplicate derived quantity name: '{duplicate}'")
        return self
