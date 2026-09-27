"""
config_loader.py

Loads and validates DearUnits configuration with a cross-check that ensures
every unit name is unique across the whole graph.
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
        issues.append(ConfigIssue(path, location, f"unit name '{name}' already defined by {previous}"))
    else:
        owners[name] = owner_label

def _validate_name_uniqueness(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    owners: dict[str, str] = {}

    for quantity in bundle.base_quantities.values():
        _claim_name(quantity.base_unit, f"base quantity '{quantity.name}'", BASE_TYPES_CONFIG_PATH, f"classes.{quantity.name}.base_unit", owners, issues)
        for unit in quantity.units:
            _claim_name(unit.name, f"base quantity '{quantity.name}'", BASE_TYPES_CONFIG_PATH, f"classes.{quantity.name}.units.{unit.name}", owners, issues)

    for derived in bundle.derived_quantities.values():
        _claim_name(derived.base_unit, f"derived quantity '{derived.name}'", COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.base_unit", owners, issues)
        for unit in derived.units:
            _claim_name(unit.name, f"derived quantity '{derived.name}'", COMPOUND_TYPES_CONFIG_PATH, f"compounds.{derived.name}.units.{unit.name}", owners, issues)


def _base_quantity_unit_scale(quantity: BaseQuantityConfig, unit_name: str) -> float | None:
    """A base quantity's own base unit is implicit (scale=1.0, not in `.units`)."""
    if unit_name == quantity.base_unit:
        return 1.0
    unit = next((u for u in quantity.units if u.name == unit_name), None)
    return None if unit is None else unit.scale

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


def _validate_relations(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    quantities = {*bundle.base_quantities, *bundle.derived_quantities}
    products: dict[frozenset[str], str] = {}
    quotients: dict[tuple[str, str], str] = {}

    for index, relation in enumerate(bundle.relations):
        location = f"relations.{index}"
        unknown = [name for name in (relation.lhs, relation.rhs, relation.result) if name not in quantities]
        if unknown:
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, location,
                f"unknown base/derived quantity name(s): {', '.join(unknown)}",
            ))
            continue

        pair = frozenset((relation.lhs, relation.rhs))
        if pair in products:
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, location,
                f"'{relation.lhs}' * '{relation.rhs}' is already defined as '{products[pair]}'",
            ))
            continue
        products[pair] = relation.result

        # result / lhs = rhs and result / rhs = lhs must not contradict another relation
        for divisor, quotient in ((relation.lhs, relation.rhs), (relation.rhs, relation.lhs)):
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
        raise ValueError("Unit configuration validation failed")

    bundle = UnitConfigBundle(
        base_quantities={item.name: item for item in base_types.classes},
        derived_quantities={item.name: item for item in compound_types.compounds},
        relations=tuple(compound_types.relations),
    )
    _validate_name_uniqueness(bundle, issues)
    _validate_relations(bundle, issues)
    derived_scales = _resolve_derived_scales(bundle, issues)

    if issues:
        _print_issues(issues)
        raise ValueError("Unit configuration validation failed")

    return replace(bundle, derived_scales=derived_scales)
