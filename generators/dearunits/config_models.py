"""
config_models.py

A unit graph has two node kinds:
  - equivalence classes: equivalent dimensional units that can convert to one another
  - compound types: units cross compiled across different classes

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

def _require_unique_group_names(composed_of: list[CompositionTermConfig], owner: str) -> None:
    duplicate = _duplicate([term.group_name for term in composed_of])
    if duplicate is not None:
        raise ValueError(f"{owner} references group '{duplicate}' more than once in composed_of")

class UnitDefConfig(DeclarationModel):
    name: str
    scale: Number = 1.0
    offset: Number = 0.0

class CompositionTermConfig(DeclarationModel):
    group_name: str
    unit_name: str
    exponent: int

    @model_validator(mode="after")
    def exponent_is_nonzero(self) -> Self:
        if self.exponent == 0:
            raise ValueError("Composition exponent must not be zero")
        return self

class CompoundUnitDefConfig(DeclarationModel):
    name: str
    scale: Number | None = None
    composed_of: Annotated[list[CompositionTermConfig], Field(min_length=1)] | None = None

    @model_validator(mode="after")
    def exactly_one_of_scale_or_composed_of(self) -> Self:
        if (self.scale is None) == (self.composed_of is None):
            raise ValueError(f"unit '{self.name}' must set exactly one of 'scale' or 'composed_of'")
        return self

    @model_validator(mode="after")
    def composed_of_group_names_unique(self) -> Self:
        if self.composed_of is not None:
            _require_unique_group_names(self.composed_of, f"unit '{self.name}'")
        return self

class EquivalenceClassConfig(DeclarationModel):
    """`base_unit` names this class's identity unit (scale=1.0, offset=0.0)
    -- it is NOT listed in `units`, which holds only the other, non-base
    units. This is implicit rather than an authored entry so the identity
    invariant can't be gotten wrong: there's nothing to validate."""
    name: str
    base_unit: str
    units: Annotated[list[UnitDefConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_class(self) -> Self:
        unit_names = [unit.name for unit in self.units]
        duplicate = _duplicate(unit_names)
        if duplicate is not None:
            raise ValueError(f"Duplicate unit name found in class '{self.name}': '{duplicate}'")
        _require_base_not_duplicated(self.base_unit, unit_names, f"class '{self.name}'")
        return self

class EquivalenceClassesConfig(DeclarationModel):
    classes: list[EquivalenceClassConfig]

    @model_validator(mode="after")
    def validate_classes(self) -> Self:
        duplicate = _duplicate([item.name for item in self.classes])
        if duplicate is not None:
            raise ValueError(f"Duplicate equivalence class name: '{duplicate}'")
        return self

class CompoundTypeConfig(DeclarationModel):
    """`base_unit` names this compound's identity unit (scale=1.0, implicit,
    same as `EquivalenceClassConfig` -- not listed in `units`). Its
    dimensional makeup is `composed_of` here at the top level (e.g. velocity
    = length^1 * time^-1) -- this doubles as both the numeric identity check
    (must derive to 1.0) and the `dimensions` DearUnits generates a real
    arithmetic function from (e.g. velocity_from(meter_t, second_t))."""
    name: str
    base_unit: str
    composed_of: Annotated[list[CompositionTermConfig], Field(min_length=1)]
    units: Annotated[list[CompoundUnitDefConfig], Field(min_length=1)]

    @model_validator(mode="after")
    def validate_compound(self) -> Self:
        unit_names = [unit.name for unit in self.units]
        duplicate = _duplicate(unit_names)
        if duplicate is not None:
            raise ValueError(f"Duplicate unit name in compound '{self.name}': '{duplicate}'")
        _require_base_not_duplicated(self.base_unit, unit_names, f"compound '{self.name}'")
        _require_unique_group_names(self.composed_of, f"compound '{self.name}''s base_unit")
        return self

class CompoundTypesConfig(DeclarationModel):
    compounds: list[CompoundTypeConfig]

    @model_validator(mode="after")
    def validate_compounds(self) -> Self:
        duplicate = _duplicate([item.name for item in self.compounds])
        if duplicate is not None:
            raise ValueError(f"Duplicate compound type name: '{duplicate}'")
        return self
