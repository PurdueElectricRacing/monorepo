"""
config_loader.py

Loads and validates DearUnits configuration with cross-checks that ensure
compound dimensions must reference a real equivalence class, and every 
unit symbol must be unique across the whole graph.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TypeVar

from pydantic import ValidationError

from core.config import BASE_TYPES_CONFIG_PATH, COMPOUND_TYPES_CONFIG_PATH
from core.config_models import ConfigModel
from core.utils import print_as_error, print_as_ok, print_as_success, print_as_warning
from .config_models import (
    CompoundTypeConfig,
    CompoundTypesConfig,
    EquivalenceClassConfig,
    EquivalenceClassesConfig,
)


T = TypeVar("T", bound=ConfigModel)


@dataclass(frozen=True)
class ConfigIssue:
    path: Path
    location: str
    message: str


class ConfigValidationError(ValueError):
    pass


@dataclass(frozen=True)
class UnitConfigBundle:
    classes: dict[str, EquivalenceClassConfig]
    compounds: dict[str, CompoundTypeConfig]


def _load_model(path: Path, model_type: type[T], issues: list[ConfigIssue]) -> T | None:
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


def _validate_dimension_references(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    for compound in bundle.compounds.values():
        for term in compound.dimensions:
            if term.class_name not in bundle.classes:
                issues.append(ConfigIssue(
                    COMPOUND_TYPES_CONFIG_PATH,
                    f"compounds.{compound.name}.dimensions.{term.class_name}",
                    f"unknown equivalence class '{term.class_name}'",
                ))


def _validate_symbol_uniqueness(bundle: UnitConfigBundle, issues: list[ConfigIssue]) -> None:
    owners: dict[str, str] = {}

    for cls in bundle.classes.values():
        for unit in cls.units:
            previous = owners.get(unit.symbol)
            if previous is not None:
                issues.append(ConfigIssue(
                    BASE_TYPES_CONFIG_PATH, f"classes.{cls.name}.units.{unit.symbol}",
                    f"unit symbol '{unit.symbol}' already defined by {previous}",
                ))
            else:
                owners[unit.symbol] = f"class '{cls.name}'"

    for compound in bundle.compounds.values():
        for unit in compound.units:
            previous = owners.get(unit.symbol)
            if previous is not None:
                issues.append(ConfigIssue(
                    COMPOUND_TYPES_CONFIG_PATH, f"compounds.{compound.name}.units.{unit.symbol}",
                    f"unit symbol '{unit.symbol}' already defined by {previous}",
                ))
            else:
                owners[unit.symbol] = f"compound '{compound.name}'"


def load_unit_config_bundle(
    base_types_path: Path = BASE_TYPES_CONFIG_PATH,
    compound_types_path: Path = COMPOUND_TYPES_CONFIG_PATH,
) -> UnitConfigBundle:
    """Load and validate every DearUnits configuration file exactly once."""
    print("Loading and validating unit configs...")
    issues: list[ConfigIssue] = []

    base_types = _load_model(base_types_path, EquivalenceClassesConfig, issues)
    compound_types = _load_model(compound_types_path, CompoundTypesConfig, issues)

    if issues or base_types is None or compound_types is None:
        _print_issues(issues)
        raise ConfigValidationError("Unit configuration validation failed")

    bundle = UnitConfigBundle(
        classes={item.name: item for item in base_types.classes},
        compounds={item.name: item for item in compound_types.compounds},
    )
    _validate_dimension_references(bundle, issues)
    _validate_symbol_uniqueness(bundle, issues)

    if issues:
        _print_issues(issues)
        raise ConfigValidationError("Unit configuration validation failed")

    print_as_success("All unit configs loaded and validated")
    return bundle
