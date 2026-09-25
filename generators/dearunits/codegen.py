"""
codegen.py

Renders the DearUnits unit graph into a single generated C header
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .models import CompoundType, UnitGraph, UnitNode
from core.artifacts import Artifact
from core.utils import get_jinja_env, print_as_ok, print_as_success, render_template


@dataclass
class UnitTypeContext:
    name: str
    type_name: str
    scale: float
    offset: float


@dataclass
class ArithmeticParam:
    group_name: str
    type_name: str
    exponent: int


@dataclass
class CompoundArithmeticContext:
    compound_name: str
    result_type: str
    params: list[ArithmeticParam]

    @property
    def numerator_terms(self) -> list[str]:
        return [f"{p.group_name}.value" for p in self.params if p.exponent > 0 for _ in range(p.exponent)]

    @property
    def denominator_terms(self) -> list[str]:
        return [f"{p.group_name}.value" for p in self.params if p.exponent < 0 for _ in range(-p.exponent)]


@dataclass
class GroupContext:
    name: str
    base: UnitTypeContext
    non_base: list[UnitTypeContext] = field(default_factory=list)
    arithmetic: CompoundArithmeticContext | None = None

    @property
    def dispatch_name(self) -> str:
        return f"{self.base.name}_from"

    @property
    def has_conversions(self) -> bool:
        return bool(self.non_base)


def _unit_context(unit: UnitNode) -> UnitTypeContext:
    return UnitTypeContext(name=unit.name, type_name=f"{unit.name}_t", scale=unit.scale, offset=unit.offset)


def _build_group(name: str, base_name: str, units: dict[str, UnitNode]) -> GroupContext:
    base = units[base_name]
    return GroupContext(
        name=name,
        base=_unit_context(base),
        non_base=[_unit_context(unit) for unit_name, unit in units.items() if unit_name != base_name],
    )


def _group_base_name(group_name: str, graph: UnitGraph) -> str:
    if group_name in graph.classes:
        return graph.classes[group_name].base_name
    return graph.compounds[group_name].base_name


def _build_arithmetic_context(compound: CompoundType, graph: UnitGraph) -> CompoundArithmeticContext:
    return CompoundArithmeticContext(
        compound_name=compound.name,
        result_type=f"{compound.base_name}_t",
        params=[
            ArithmeticParam(
                group_name=term.group_name,
                type_name=f"{_group_base_name(term.group_name, graph)}_t",
                exponent=term.exponent,
            )
            for term in compound.dimensions
        ],
    )


def build_group_contexts(graph: UnitGraph) -> list[GroupContext]:
    groups = [_build_group(cls.name, cls.base_name, cls.units) for cls in graph.classes.values()]

    for compound in graph.compounds.values():
        group = _build_group(compound.name, compound.base_name, compound.units)
        group.arithmetic = _build_arithmetic_context(compound, graph)
        groups.append(group)

    return groups


def generate_units_header(graph: UnitGraph) -> Artifact:
    env = get_jinja_env()
    content = render_template(env, "dear_units.h.jinja", groups=build_group_contexts(graph))
    print_as_ok("Generated dear_units.h")
    return Artifact("units_generated", "dear_units.h", content)


def generate_headers(graph: UnitGraph) -> list[Artifact]:
    print("Generating unit headers...")
    artifacts = [generate_units_header(graph)]
    print_as_success("Successfully generated DearUnits headers")
    return artifacts
