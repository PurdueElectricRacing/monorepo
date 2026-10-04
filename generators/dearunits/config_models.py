"""
config_models.py

A unit graph has two node kinds:
  - base quantities: equivalent dimensional units that can convert to one another
  - derived quantities: units cross compiled across different base quantities

Ensures the validity of the specified JSON format

WARNING: only mark one class `is_angle`. A second compiles fine but silently
produces wrong trig results -- see codegen.py's build_angle_contexts.
"""

from __future__ import annotations

from typing import Annotated, Self

from pydantic import AfterValidator, Field, StringConstraints, model_validator

from core.declarations import DeclarationModel

C_KEYWORDS = frozenset({
    "alignas", "alignof", "auto", "bool", "break", "case", "char", "const", "constexpr",
    "continue", "default", "do", "double", "else", "enum", "extern", "false", "float",
    "for", "goto", "if", "inline", "int", "long", "nullptr", "register", "restrict",
    "return", "short", "signed", "sizeof", "static", "static_assert", "struct", "switch",
    "thread_local", "true", "typedef", "typeof", "typeof_unqual", "union", "unsigned",
    "void", "volatile", "while",
})
RESERVED_NAMES = frozenset({"scalar"})
FORBIDDEN_FRAGMENTS = ("_by_", "_from_")

def _valid_identifier(name: str) -> str:
    if name in C_KEYWORDS or name in RESERVED_NAMES:
        raise ValueError(f"'{name}' is reserved and can't be used as a name")
    if name.endswith(("_from", "_by")) or any(fragment in name for fragment in FORBIDDEN_FRAGMENTS):
        raise ValueError(f"'{name}' contains a fragment the generated function names use as a separator")
    return name

Number = int | float

Identifier = Annotated[
    str,
    StringConstraints(pattern=r"^[a-z][a-z0-9]*(_[a-z0-9]+)*$"),
    AfterValidator(_valid_identifier),
]
Scale = Annotated[Number, Field(allow_inf_nan=False, gt=0)]
Offset = Annotated[Number, Field(allow_inf_nan=False)]

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
            f"base_unit '{base_unit}' of {owner} must not also appear in 'units' "
        )

def _require_unique_group_names(composed_of: list[DimensionTermConfig], owner: str) -> None:
    duplicate = _duplicate([term.group_name for term in composed_of])
    if duplicate is not None:
        raise ValueError(f"{owner} references group '{duplicate}' more than once")

class UnitConfig(DeclarationModel):
    name: Identifier
    scale: Scale = 1.0
    offset: Offset = 0.0

class DimensionTermConfig(DeclarationModel):
    group_name: Identifier
    unit_name: Identifier
    exponent: int

    @model_validator(mode="after")
    def nonzero_exponent(self) -> Self:
        if self.exponent == 0:
            raise ValueError("Exponent must not be zero")
        return self

class DerivedUnitConfig(DeclarationModel):
    name: Identifier
    scale: Scale | None = None
    composed_of: Annotated[list[DimensionTermConfig], Field(min_length=1)] | None = None

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

class BaseQuantityConfig(DeclarationModel):
    name: Identifier
    base_unit: Identifier
    units: Annotated[list[UnitConfig], Field(min_length=1)]
    is_angle: bool = False
    is_dimensionless: bool = False

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
    name: Identifier
    base_unit: Identifier
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
    factor_a: str
    factor_b: str
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
