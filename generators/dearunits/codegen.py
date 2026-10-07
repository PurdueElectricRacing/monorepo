"""
codegen.py

Renders the DearUnits unit graph into a single generated C header
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .models import DerivedQuantity, UnitGraph, Unit
from core.artifacts import Artifact
from core.utils import get_jinja_env, print_as_ok, print_as_success, print_as_warning, render_template


@dataclass
class UnitContext:
    name: str
    type_name: str
    scale: float
    offset: float


@dataclass
class ConstructorParamContext:
    group_name: str
    type_name: str
    exponent: int


@dataclass
class ConstructorContext:
    compound_name: str
    result_type: str
    params: list[ConstructorParamContext]

    @property
    def numerator_terms(self) -> list[str]:
        return [f"{p.group_name}.value" for p in self.params if p.exponent > 0 for _ in range(p.exponent)]

    @property
    def denominator_terms(self) -> list[str]:
        return [f"{p.group_name}.value" for p in self.params if p.exponent < 0 for _ in range(-p.exponent)]


@dataclass
class QuantityContext:
    name: str
    base: UnitContext
    non_base: list[UnitContext] = field(default_factory=list)
    arithmetic: ConstructorContext | None = None

    @property
    def dispatch_name(self) -> str:
        return f"DU_{self.base.name}_from".upper()

    @property
    def has_conversions(self) -> bool:
        return bool(self.non_base)

    @property
    def types(self) -> list[UnitContext]:
        return [self.base, *self.non_base]


@dataclass
class BinaryOpDefinitionContext:
    macro: str
    args: list[str]


@dataclass
class BinaryOpDispatchEntryContext:
    rhs_type: str
    function: str


@dataclass
class BinaryOpDispatchContext:
    lhs_type: str
    entries: list[BinaryOpDispatchEntryContext] = field(default_factory=list)


@dataclass
class SquareOpContext:
    operand_type: str
    operand_name: str
    result_type: str
    result_name: str


@dataclass
class BinaryOpsContext:
    definitions: list[BinaryOpDefinitionContext] = field(default_factory=list)
    multiply: list[BinaryOpDispatchContext] = field(default_factory=list)
    divide: list[BinaryOpDispatchContext] = field(default_factory=list)
    squares: list[SquareOpContext] = field(default_factory=list)


def _unit_context(unit: Unit) -> UnitContext:
    return UnitContext(name=unit.name, type_name=f"{unit.name}_t", scale=unit.scale, offset=unit.offset)


def _build_quantity_context(name: str, base_name: str, units: dict[str, Unit]) -> QuantityContext:
    base = units[base_name]
    return QuantityContext(
        name=name,
        base=_unit_context(base),
        non_base=[_unit_context(unit) for unit_name, unit in units.items() if unit_name != base_name],
    )


def _base_unit_name(quantity_name: str, graph: UnitGraph) -> str:
    if quantity_name in graph.base_quantities:
        return graph.base_quantities[quantity_name].base_name
    return graph.derived_quantities[quantity_name].base_name


def _build_constructor_context(derived: DerivedQuantity, graph: UnitGraph) -> ConstructorContext:
    return ConstructorContext(
        compound_name=derived.name,
        result_type=f"{derived.base_name}_t",
        params=[
            ConstructorParamContext(
                group_name=term.quantity_name,
                type_name=f"{_base_unit_name(term.quantity_name, graph)}_t",
                exponent=term.exponent,
            )
            for term in derived.dimensions
        ],
    )


def _dimensions(quantity_name: str, graph: UnitGraph) -> dict[str, int]:
    """Flattens a quantity into exponents over the base quantities."""
    if quantity_name in graph.base_quantities:
        return {quantity_name: 1}

    dims: dict[str, int] = {}
    for term in graph.derived_quantities[quantity_name].dimensions:
        for base_quantity, exponent in _dimensions(term.quantity_name, graph).items():
            dims[base_quantity] = dims.get(base_quantity, 0) + exponent * term.exponent
    return {base_quantity: exponent for base_quantity, exponent in dims.items() if exponent}


def _combine(lhs: dict[str, int], rhs: dict[str, int], sign: int) -> tuple[tuple[str, int], ...]:
    dims = dict(lhs)
    for base_quantity, exponent in rhs.items():
        dims[base_quantity] = dims.get(base_quantity, 0) + sign * exponent
    return tuple(sorted((base_quantity, exponent) for base_quantity, exponent in dims.items() if exponent))


def build_binary_ops(graph: UnitGraph, quantities: list[QuantityContext]) -> BinaryOpsContext:
    quantity_dims = {name: _dimensions(name, graph) for name in [*graph.base_quantities, *graph.derived_quantities]}
    quantities_by_dims: dict[tuple[tuple[str, int], ...], list[str]] = {}
    for name, dims in quantity_dims.items():
        quantities_by_dims.setdefault(tuple(sorted(dims.items())), []).append(name)

    ambiguous = {
        name for name, dims in quantity_dims.items()
        if len(quantities_by_dims[tuple(sorted(dims.items()))]) > 1
    }

    base_name = {name: _base_unit_name(name, graph) for name in quantity_dims}
    base_type = {name: f"{unit}_t" for name, unit in base_name.items()}

    products: dict[tuple[str, str], str] = {}
    quotients: dict[tuple[str, str], str] = {}
    for relation in graph.relations:
        products[(relation.factor_a, relation.factor_b)] = relation.result
        products[(relation.factor_b, relation.factor_a)] = relation.result
        quotients[(relation.result, relation.factor_a)] = relation.factor_b
        quotients[(relation.result, relation.factor_b)] = relation.factor_a

    def match(dims: tuple[tuple[str, int], ...], description: str) -> str | None:
        matches = quantities_by_dims.get(dims, [])
        if len(matches) > 1:
            print_as_warning(f"{description} has dimensions shared by {', '.join(matches)}; skipped")
        return matches[0] if len(matches) == 1 else None

    def result_type(lhs: str, rhs: str, sign: int, op_name: str) -> str | None:
        explicit = (products if sign > 0 else quotients).get((lhs, rhs))
        if explicit is not None:
            return base_type[explicit]
        if sign < 0 and lhs == rhs:
            return "float"
        if lhs in ambiguous or rhs in ambiguous:
            return None
        dims = _combine(quantity_dims[lhs], quantity_dims[rhs], sign)
        if not dims:
            return "float"
        result = match(dims, f"DU_{op_name}: {lhs} and {rhs}")
        return None if result is None else base_type[result]

    ops = BinaryOpsContext()
    cross: dict[str, dict[str, list[BinaryOpDispatchEntryContext]]] = {"multiply": {}, "divide": {}}
    squared: set[str] = set()
    for lhs in quantity_dims:
        for rhs in quantity_dims:
            for op, sign, op_name in (("multiply", 1, "MULTIPLY"), ("divide", -1, "DIVIDE")):
                result = result_type(lhs, rhs, sign, op_name)
                if result is None:
                    continue
                if result == "float":
                    ops.definitions.append(BinaryOpDefinitionContext(
                        f"DU_DEFINE_{op_name}_TO_FLOAT", [base_name[lhs], base_name[rhs]],
                    ))
                else:
                    ops.definitions.append(BinaryOpDefinitionContext(
                        f"DU_DEFINE_{op_name}", [base_name[lhs], base_name[rhs], result[:-2]],
                    ))
                cross[op].setdefault(lhs, []).append(BinaryOpDispatchEntryContext(
                    base_type[rhs], f"dearunits_{op}_{base_name[lhs]}_by_{base_name[rhs]}",
                ))
                if op == "multiply" and lhs == rhs and result != "float" and result not in squared:
                    squared.add(result)
                    ops.squares.append(SquareOpContext(base_type[lhs], base_name[lhs], result, result[:-2]))

    scalar_first: dict[str, list[BinaryOpDispatchEntryContext]] = {"multiply": [], "divide": []}
    for quantity in quantities:
        for unit in quantity.types:
            ops.definitions.append(BinaryOpDefinitionContext("DU_DEFINE_SCALAR_OPS", [unit.name]))
            for op, dispatch in (("multiply", ops.multiply), ("divide", ops.divide)):
                entries = [BinaryOpDispatchEntryContext("float", f"dearunits_{op}_{unit.name}_by_scalar")]
                if unit is quantity.base:
                    entries += cross[op].get(quantity.name, [])
                dispatch.append(BinaryOpDispatchContext(unit.type_name, entries))
            scalar_first["multiply"].append(BinaryOpDispatchEntryContext(
                unit.type_name, f"dearunits_multiply_scalar_by_{unit.name}",
            ))

        inverse = None if quantity.name in ambiguous else match(
            tuple(sorted((base_quantity, -exponent) for base_quantity, exponent in quantity_dims[quantity.name].items())),
            f"DU_DIVIDE: 1 / {quantity.name}",
        )
        if inverse is not None:
            ops.definitions.append(BinaryOpDefinitionContext(
                "DU_DEFINE_INVERSE", [quantity.base.name, base_name[inverse]],
            ))
            scalar_first["divide"].append(BinaryOpDispatchEntryContext(
                quantity.base.type_name, f"dearunits_divide_scalar_by_{quantity.base.name}",
            ))

    for op, dispatch in (("multiply", ops.multiply), ("divide", ops.divide)):
        if scalar_first[op]:
            dispatch.append(BinaryOpDispatchContext("float", scalar_first[op]))

    return ops


def build_quantity_contexts(graph: UnitGraph) -> list[QuantityContext]:
    quantities = [
        _build_quantity_context(base.name, base.base_name, base.units)
        for base in graph.base_quantities.values()
    ]

    for derived in graph.derived_quantities.values():
        quantity = _build_quantity_context(derived.name, derived.base_name, derived.units)
        quantity.arithmetic = _build_constructor_context(derived, graph)
        quantities.append(quantity)

    return quantities


def generate_units_header(quantities: list[QuantityContext], binary_ops: BinaryOpsContext) -> Artifact:
    env = get_jinja_env()
    content = render_template(
        env,
        "dear_units.h.jinja",
        groups=quantities,
        base_types=[quantity.base for quantity in quantities],
        binary_ops=binary_ops,
    )
    print_as_ok("Generated dear_units.h")
    return Artifact("units_generated", "dear_units.h", content)


def generate_headers(graph: UnitGraph) -> list[Artifact]:
    print("Generating unit headers...")
    quantities = build_quantity_contexts(graph)
    binary_ops = build_binary_ops(graph, quantities)
    artifacts = [
        generate_units_header(quantities, binary_ops),
    ]
    print_as_success("Successfully generated DearUnits headers")
    return artifacts
