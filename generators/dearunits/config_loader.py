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
    CompositionTermConfig,
    CompoundTypeConfig,
    CompoundTypesConfig,
    EquivalenceClassConfig,
    EquivalenceClassesConfig,
)

@dataclass(frozen=True)
class ConfigIssue:
    path: Path
    location: str
    message: str

@dataclass(frozen=True)
class UnitConfigBundle:
    classes: dict[str, EquivalenceClassConfig]
    compounds: dict[str, CompoundTypeConfig]
    compound_scales: dict[str, dict[str, float]] = field(default_factory=dict)

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

    for cls in bundle.classes.values():
        _claim_name(cls.base_unit, f"class '{cls.name}'", BASE_TYPES_CONFIG_PATH, f"classes.{cls.name}.base_unit", owners, issues)
        for unit in cls.units:
            _claim_name(unit.name, f"class '{cls.name}'", BASE_TYPES_CONFIG_PATH, f"classes.{cls.name}.units.{unit.name}", owners, issues)

    for compound in bundle.compounds.values():
        _claim_name(compound.base_unit, f"compound '{compound.name}'", COMPOUND_TYPES_CONFIG_PATH, f"compounds.{compound.name}.base_unit", owners, issues)
        for unit in compound.units:
            _claim_name(unit.name, f"compound '{compound.name}'", COMPOUND_TYPES_CONFIG_PATH, f"compounds.{compound.name}.units.{unit.name}", owners, issues)


def _class_unit_scale(cls: EquivalenceClassConfig, unit_name: str) -> float | None:
    """A class's own base unit is implicit (scale=1.0, not in `.units`)."""
    if unit_name == cls.base_unit:
        return 1.0
    unit = next((u for u in cls.units if u.name == unit_name), None)
    return None if unit is None else unit.scale

def _resolve_term(
    term: CompositionTermConfig,
    classes: dict[str, EquivalenceClassConfig],
    resolved: dict[str, dict[str, float]],
) -> float | None:
    if term.group_name in classes:
        scale = _class_unit_scale(classes[term.group_name], term.unit_name)
        return None if scale is None else scale ** term.exponent

    if term.group_name in resolved and term.unit_name in resolved[term.group_name]:
        return resolved[term.group_name][term.unit_name] ** term.exponent

    return None


@dataclass(frozen=True)
class _PendingUnit:
    compound: CompoundTypeConfig
    unit_name: str
    scale: float | None
    composed_of: list[CompositionTermConfig] | None
    location: str

def _resolve_compound_scales(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> dict[str, dict[str, float]]:
    """Resolves every compound's units *and* its own implicit base unit in
    one fixed-point loop -- a base's `composed_of` can reference another
    compound (e.g. torque = force^1 * length^1), exactly like a non-base
    unit can (e.g. torque's pound_foot -> force.pound_force), so both need
    the same dependency-ordered resolution."""
    resolved: dict[str, dict[str, float]] = {name: {} for name in bundle.compounds}
    pending: list[_PendingUnit] = []

    for compound in bundle.compounds.values():
        pending.append(_PendingUnit(
            compound, compound.base_unit, None, compound.composed_of,
            f"compounds.{compound.name}.composed_of",
        ))
        for unit in compound.units:
            pending.append(_PendingUnit(
                compound, unit.name, unit.scale, unit.composed_of,
                f"compounds.{compound.name}.units.{unit.name}.composed_of",
            ))

    progressed = True
    while pending and progressed:
        progressed = False
        still_pending = []
        for item in pending:
            if item.scale is not None:
                resolved[item.compound.name][item.unit_name] = item.scale
                progressed = True
                continue

            total = 1.0
            for term in item.composed_of:
                factor = _resolve_term(term, bundle.classes, resolved)
                if factor is None:
                    total = None
                    break
                total *= factor

            if total is None:
                still_pending.append(item)
            else:
                resolved[item.compound.name][item.unit_name] = total
                progressed = True
        pending = still_pending

    for item in pending:
        issues.append(ConfigIssue(
            COMPOUND_TYPES_CONFIG_PATH, item.location,
            "could not resolve composed_of (unknown class/compound/unit, or a dependency cycle)",
        ))

    for compound in bundle.compounds.values():
        base_scale = resolved[compound.name].get(compound.base_unit)
        if base_scale is not None and not math.isclose(base_scale, 1.0, rel_tol=1e-9, abs_tol=1e-9):
            issues.append(ConfigIssue(
                COMPOUND_TYPES_CONFIG_PATH, f"compounds.{compound.name}.composed_of",
                f"base_unit '{compound.base_unit}' must derive to scale=1.0, got {base_scale}",
            ))

    return resolved


def load_unit_config_bundle(
    base_types_path: Path = BASE_TYPES_CONFIG_PATH,
    compound_types_path: Path = COMPOUND_TYPES_CONFIG_PATH,
) -> UnitConfigBundle:
    issues: list[ConfigIssue] = []

    base_types = _load_model(base_types_path, EquivalenceClassesConfig, issues)
    compound_types = _load_model(compound_types_path, CompoundTypesConfig, issues)

    if issues or base_types is None or compound_types is None:
        _print_issues(issues)
        raise ValueError("Unit configuration validation failed")

    bundle = UnitConfigBundle(
        classes={item.name: item for item in base_types.classes},
        compounds={item.name: item for item in compound_types.compounds},
    )
    _validate_name_uniqueness(bundle, issues)
    compound_scales = _resolve_compound_scales(bundle, issues)

    if issues:
        _print_issues(issues)
        raise ValueError("Unit configuration validation failed")

    return replace(bundle, compound_scales=compound_scales)
