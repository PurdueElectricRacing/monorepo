"""
config_loader.py

Loads and validates DearUnits configuration with cross-checks that ensure
every node uniqueness, unit composition has its quantity's dimensions, 
dimensionality is consistent, and no composed_of term references an offset-bearing unit.
Also resolves every derived unit's numeric scale from its composed_of terms.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace
from pathlib import Path

from pydantic import ValidationError

from core.config import BASE_TYPES_CONFIG_PATH, COMPOUND_TYPES_CONFIG_PATH
from core.declarations import DeclarationModel
from core.utils import print_as_error, print_as_ok, print_as_warning
from .config_models import (
    BaseQuantitiesConfig,
    BaseQuantityConfig,
    DerivedQuantitiesConfig,
    DerivedQuantityConfig,
    DimensionTermConfig,
    RelationConfig,
)

@dataclass(frozen=True)
class ConfigIssue:
    path: Path
    location: str
    message: str

class UnitConfigValidationError(ValueError):
    pass

@dataclass(frozen=True)
class UnitConfigBundle:
    base_quantities: dict[str, BaseQuantityConfig]
    derived_quantities: dict[str, DerivedQuantityConfig]
    derived_scales: dict[str, dict[str, float]] = field(default_factory=dict)
    relations: tuple[RelationConfig, ...] = ()

def _load_model(path: Path, model_type: type[DeclarationModel], issues: list[ConfigIssue]) -> DeclarationModel | None:
    try:
        contents = path.read_bytes()
    except OSError as error:
        issues.append(ConfigIssue(path, "root", str(error)))
        return None

    try:
        model = model_type.model_validate_json(contents)
    except ValidationError as error:
        for detail in error.errors(include_url=False):
            location = ".".join(str(part) for part in detail["loc"]) or "root"
            issues.append(ConfigIssue(path, location, detail["msg"]))
        return None

    print_as_ok(path.name)
    return model

def _print_issues(issues: list[ConfigIssue]) -> None:
    print_as_warning("Unit configuration validation failed:")
    for issue in issues:
        print_as_error(f"  {issue.path.name}: Field '{issue.location}': {issue.message}")


def _claim_name(
    name: str, owner_label: str, path: Path, location: str,
    owners: dict[str, str], issues: list[ConfigIssue],
) -> None:
    previous = owners.get(name)
    if previous is not None:
        issues.append(ConfigIssue(path, location, f"name '{name}' already defined by {previous}"))
    else:
        owners[name] = owner_label

def _validate_name_uniqueness(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    owners: dict[str, str] = {}

    for quantity in bundle.base_quantities.values():
        label = f"base quantity '{quantity.name}'"
        _claim_name(quantity.name, label, BASE_TYPES_CONFIG_PATH, f"classes.{quantity.name}.name", owners, issues)
        _claim_name(quantity.base_unit, label, BASE_TYPES_CONFIG_PATH, f"classes.{quantity.name}.base_unit", owners, issues)
        for unit in quantity.units:
            _claim_name(unit.name, label, BASE_TYPES_CONFIG_PATH, f"classes.{quantity.name}.units.{unit.name}", owners, issues)

    for derived in bundle.derived_quantities.values():
        label = f"derived quantity '{derived.name}'"
        _claim_name(derived.name, label, COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.name", owners, issues)
        _claim_name(derived.base_unit, label, COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.base_unit", owners, issues)
        for unit in derived.units:
            _claim_name(unit.name, label, COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.units.{unit.name}", owners, issues)


Dimensions = dict[str, int]

def _quantity_dimensions(
    name: str, bundle: UnitConfigBundle, memo: dict[str, Dimensions], stack: tuple[str, ...] = (),
) -> Dimensions | None:
    if name in bundle.base_quantities:
        return {name: 1}
    if name in memo:
        return memo[name]
    if name in stack or name not in bundle.derived_quantities:
        return None
    dims = _terms_dimensions(bundle.derived_quantities[name].composed_of, bundle, memo, (*stack, name))
    if dims is not None:
        memo[name] = dims
    return dims

def _terms_dimensions(
    terms: list[DimensionTermConfig], bundle: UnitConfigBundle,
    memo: dict[str, Dimensions], stack: tuple[str, ...] = (),
) -> Dimensions | None:
    total: Dimensions = {}
    for term in terms:
        inner = _quantity_dimensions(term.group_name, bundle, memo, stack)
        if inner is None:
            return None
        for base_quantity, exponent in inner.items():
            total[base_quantity] = total.get(base_quantity, 0) + exponent * term.exponent
    return {base_quantity: exponent for base_quantity, exponent in total.items() if exponent}

def _format_dimensions(dims: Dimensions) -> str:
    if not dims:
        return "dimensionless"
    return " ".join(f"{name}^{exponent}" for name, exponent in sorted(dims.items()))

def _validate_unit_dimensions(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    memo: dict[str, Dimensions] = {}
    for derived in bundle.derived_quantities.values():
        expected = _quantity_dimensions(derived.name, bundle, memo)
        if expected is None:
            continue
        for unit in derived.units:
            if unit.composed_of is None:
                continue
            actual = _terms_dimensions(unit.composed_of, bundle, memo)
            if actual is not None and actual != expected:
                issues.append(ConfigIssue(
                    COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.units.{unit.name}.composed_of",
                    f"dimensions {_format_dimensions(actual)} don't match {derived.name} ({_format_dimensions(expected)})",
                ))


def _base_quantity_unit_scale(quantity: BaseQuantityConfig, unit_name: str) -> float | None:
    """A base quantity's own base unit is implicit (scale=1.0, not in `.units`)."""
    if unit_name == quantity.base_unit:
        return 1.0
    unit = next((u for u in quantity.units if u.name == unit_name), None)
    return None if unit is None else unit.scale

def _base_quantity_unit_offset(quantity: BaseQuantityConfig, unit_name: str) -> float:
    if unit_name == quantity.base_unit:
        return 0.0
    unit = next((u for u in quantity.units if u.name == unit_name), None)
    return 0.0 if unit is None else unit.offset

def _validate_no_offset_in_composed_of(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    "Compound types do not support types with none_zero offset types"
    for derived in bundle.derived_quantities.values():
        term_lists = [(derived.composed_of, f"compounds.{derived.name}.composed_of")]
        for unit in derived.units:
            if unit.composed_of is not None:
                term_lists.append((unit.composed_of, f"compounds.{derived.name}.units.{unit.name}.composed_of"))

        for terms, location in term_lists:
            for term in terms:
                quantity = bundle.base_quantities.get(term.group_name)
                if quantity is None:
                    continue
                offset = _base_quantity_unit_offset(quantity, term.unit_name)
                if offset != 0.0:
                    issues.append(ConfigIssue(
                        COMPOUND_TYPES_CONFIG_PATH, location,
                        f"'{term.unit_name}' has a nonzero offset ({offset}) and can't be used in composed_of "
                        "-- it's not a pure ratio of its base unit",
                    ))

def _resolve_term(
    term: DimensionTermConfig,
    base_quantities: dict[str, BaseQuantityConfig],
    resolved: dict[str, dict[str, float]],
) -> float | None:
    if term.group_name in base_quantities:
        scale = _base_quantity_unit_scale(base_quantities[term.group_name], term.unit_name)
        return None if scale is None else scale ** term.exponent

    if term.group_name in resolved and term.unit_name in resolved[term.group_name]:
        return resolved[term.group_name][term.unit_name] ** term.exponent

    return None


@dataclass(frozen=True)
class _PendingUnit:
    quantity: DerivedQuantityConfig
    unit_name: str
    scale: float | None
    composed_of: list[DimensionTermConfig] | None
    location: str

def _resolve_derived_scales(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> dict[str, dict[str, float]]:
    resolved: dict[str, dict[str, float]] = {name: {} for name in bundle.derived_quantities}
    pending: list[_PendingUnit] = []

    for derived in bundle.derived_quantities.values():
        pending.append(_PendingUnit(
            derived, derived.base_unit, None, derived.composed_of,
            f"compounds.{derived.name}.composed_of",
        ))
        for unit in derived.units:
            pending.append(_PendingUnit(
                derived, unit.name, unit.scale, unit.composed_of,
                f"compounds.{derived.name}.units.{unit.name}.composed_of",
            ))

    progressed = True
    while pending and progressed:
        progressed = False
        still_pending = []
        for item in pending:
            if item.scale is not None:
                resolved[item.quantity.name][item.unit_name] = item.scale
                progressed = True
                continue

            total = 1.0
            for term in item.composed_of:
                factor = _resolve_term(term, bundle.base_quantities, resolved)
                if factor is None:
                    total = None
                    break
                total *= factor

            if total is None:
                still_pending.append(item)
            else:
                resolved[item.quantity.name][item.unit_name] = total
                progressed = True
        pending = still_pending

    for item in pending:
        issues.append(ConfigIssue(
            COMPOUND_TYPES_CONFIG_PATH, item.location,
            "could not resolve composed_of (unknown quantity/unit, or a dependency cycle)",
        ))

    for derived in bundle.derived_quantities.values():
        base_scale = resolved[derived.name].get(derived.base_unit)
        if base_scale is not None and not math.isclose(base_scale, 1.0, rel_tol=1e-9, abs_tol=1e-9):
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.composed_of",
                f"base_unit '{derived.base_unit}' must derive to scale=1.0, got {base_scale}",
            ))

    return resolved


def _dimensionless_quantity_names(bundle: UnitConfigBundle) -> set[str]:
    return {
        quantity.name for quantity in bundle.base_quantities.values()
        if quantity.is_angle or quantity.is_dimensionless
    }

def _validate_relations(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    quantities = {*bundle.base_quantities, *bundle.derived_quantities}
    dimensionless = _dimensionless_quantity_names(bundle)
    products: dict[frozenset[str], str] = {}
    quotients: dict[tuple[str, str], str] = {}

    for index, relation in enumerate(bundle.relations):
        location = f"relations.{index}"
        unknown = [name for name in (relation.factor_a, relation.factor_b, relation.result) if name not in quantities]
        if unknown:
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, location,
                f"unknown base/derived quantity name(s): {', '.join(unknown)}",
            ))
            continue

        memo: dict[str, Dimensions] = {}
        a, b, result = (_quantity_dimensions(name, bundle, memo) for name in (relation.factor_a, relation.factor_b, relation.result))
        if a is not None and b is not None and result is not None:
            product: Dimensions = {}
            for dims in (a, b):
                for name, exponent in dims.items():
                    product[name] = product.get(name, 0) + exponent
            product = {name: exponent for name, exponent in product.items() if exponent and name not in dimensionless}
            expected = {name: exponent for name, exponent in result.items() if name not in dimensionless}
            if product != expected:
                issues.append(ConfigIssue(
                    COMPOUND_TYPES_CONFIG_PATH, location,
                    f"'{relation.factor_a}' * '{relation.factor_b}' has dimensions {_format_dimensions(product)}, "
                    f"but '{relation.result}' is {_format_dimensions(expected)} (dimensionless quantities are ignored)",
                ))
                continue

        pair = frozenset((relation.factor_a, relation.factor_b))
        if pair in products:
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, location,
                f"'{relation.factor_a}' * '{relation.factor_b}' is already defined as '{products[pair]}'",
            ))
            continue
        products[pair] = relation.result

        # result / factor_a = factor_b and result / factor_b = factor_a must not contradict another relation
        for divisor, quotient in ((relation.factor_a, relation.factor_b), (relation.factor_b, relation.factor_a)):
            previous = quotients.setdefault((relation.result, divisor), quotient)
            if previous != quotient:
                issues.append(ConfigIssue(
                    COMPOUND_TYPES_CONFIG_PATH, location,
                    f"'{relation.result}' / '{divisor}' would be both '{previous}' and '{quotient}'",
                ))


def load_unit_config_bundle(
    base_types_path: Path = BASE_TYPES_CONFIG_PATH,
    compound_types_path: Path = COMPOUND_TYPES_CONFIG_PATH,
) -> UnitConfigBundle:
    issues: list[ConfigIssue] = []

    base_types = _load_model(base_types_path, BaseQuantitiesConfig, issues)
    compound_types = _load_model(compound_types_path, DerivedQuantitiesConfig, issues)

    if issues or base_types is None or compound_types is None:
        _print_issues(issues)
        raise UnitConfigValidationError("Unit configuration validation failed")

    bundle = UnitConfigBundle(
        base_quantities={item.name: item for item in base_types.classes},
        derived_quantities={item.name: item for item in compound_types.compounds},
        relations=tuple(compound_types.relations),
    )
    _validate_name_uniqueness(bundle, issues)
    _validate_unit_dimensions(bundle, issues)
    _validate_no_offset_in_composed_of(bundle, issues)
    _validate_relations(bundle, issues)
    derived_scales = _resolve_derived_scales(bundle, issues)

    if issues:
        _print_issues(issues)
        raise UnitConfigValidationError("Unit configuration validation failed")

    return replace(bundle, derived_scales=derived_scales)
