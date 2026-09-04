"""
config_loader.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TypeVar

from pydantic import ValidationError

from core.config import CONFIG_DIR
from core.config_models import (
    BusRegistryConfig,
    ConfigBundle,
    ConfigModel,
    ExternalNodeConfig,
    InternalNodeConfig,
    CommonTypesConfig,
)
from core.utils import print_as_error, print_as_ok, print_as_success, print_as_warning


T = TypeVar("T", bound=ConfigModel)


@dataclass(frozen=True)
class ConfigIssue:
    path: Path
    location: str
    message: str


class ConfigValidationError(ValueError):
    pass


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
    print_as_warning("Configuration validation failed:")
    for issue in issues:
        print_as_error(
            f"  {issue.path.name}: Field '{issue.location}': {issue.message}"
        )


def _validate_references(
    bundle: ConfigBundle,
    internal_sources: list[Path],
    external_sources: list[Path],
    issues: list[ConfigIssue],
) -> None:
    known_buses = set(bundle.buses)
    node_names: dict[str, Path] = {}

    for path, node in zip(internal_sources, bundle.internal_nodes):
        previous = node_names.get(node.node_name)
        if previous is not None:
            issues.append(ConfigIssue(
                path, "node_name",
                f"duplicate node name '{node.node_name}' (also defined by {previous.name})",
            ))
        else:
            node_names[node.node_name] = path

        for bus_name in node.busses:
            if bus_name not in known_buses:
                issues.append(ConfigIssue(
                    path, f"busses.{bus_name}",
                    f"unknown bus '{bus_name}'",
                ))

    for path, node in zip(external_sources, bundle.external_nodes):
        previous = node_names.get(node.node_name)
        if previous is not None:
            issues.append(ConfigIssue(
                path, "node_name",
                f"duplicate node name '{node.node_name}' (also defined by {previous.name})",
            ))
        else:
            node_names[node.node_name] = path

        if node.bus_name not in known_buses:
            issues.append(ConfigIssue(
                path, "bus_name", f"unknown bus '{node.bus_name}'"
            ))


def load_config_bundle(config_dir: Path = CONFIG_DIR) -> ConfigBundle:
    """Load and validate every generator configuration file exactly once."""
    print("Loading and validating configs...")
    issues: list[ConfigIssue] = []

    bus_registry = _load_model(
        config_dir / "system" / "bus_configs.json", BusRegistryConfig, issues
    )
    common_types = _load_model(
        config_dir / "system" / "common_types.json", CommonTypesConfig, issues
    )
    internal_models = [
        (path, model)
        for path in sorted((config_dir / "nodes").glob("*.json"))
        if (model := _load_model(path, InternalNodeConfig, issues)) is not None
    ]
    external_models = [
        (path, model)
        for path in sorted((config_dir / "external_nodes").glob("*.json"))
        if (model := _load_model(path, ExternalNodeConfig, issues)) is not None
    ]

    if issues or bus_registry is None or common_types is None:
        _print_issues(issues)
        raise ConfigValidationError("Configuration validation failed")

    bundle = ConfigBundle(
        buses={bus.name: bus for bus in bus_registry.busses},
        custom_types={item.name: item for item in common_types.types},
        internal_nodes=[model for _, model in internal_models],
        external_nodes=[model for _, model in external_models],
    )
    _validate_references(
        bundle,
        [path for path, _ in internal_models],
        [path for path, _ in external_models],
        issues,
    )

    if issues:
        _print_issues(issues)
        raise ConfigValidationError("Configuration validation failed")

    print_as_success("All configs loaded and validated")
    return bundle
