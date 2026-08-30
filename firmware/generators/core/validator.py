"""
Shared configuration validation for the composed generation pipeline.

Author: Irving Wang (irvingw@purdue.edu)
"""

from pathlib import Path
from typing import Any, Mapping

from jsonschema import Draft202012Validator
from referencing import Registry, Resource
from core.utils import load_json, print_as_error, print_as_ok, print_as_warning, print_as_success

Schema = Mapping[str, Any]
SchemaStore = Mapping[str, Schema]


def validate_against_schema(
    data: object,
    schema: Schema,
    schema_store: SchemaStore | None = None,
    filename: str = "<unknown>",
) -> bool:
    """
    Validate data against schema and return success status.
    Prints all validation errors found in the file.
    """
    if schema_store:
        registry = Registry().with_resources([
            (uri, Resource.from_contents(content))
            for uri, content in schema_store.items()
        ])
    else:
        registry = Registry()

    validator = Draft202012Validator(schema, registry=registry)

    # Collect all validation errors
    errors = list(validator.iter_errors(data))
    if not errors:
        return True

    print_as_warning(f"Schema validation failed for {filename}:")
    for error in errors:
        path = ".".join(map(str, error.path)) or "root"
        print_as_error(f"  Field '{path}': {error.message}")
    
    return False

def validate_common_types(config_dir: Path, schema_dir: Path) -> bool:
    type_registry_schema = load_json(schema_dir / 'type_registry.schema.json')
    common_types = load_json(config_dir / 'system' / 'common_types.json')
    
    if validate_against_schema(common_types, type_registry_schema, filename='common_types.json'):
        print_as_ok("common_types.json")
        return True
    return False
    
def validate_bus_config(
    config_dir: Path, schema_dir: Path, schema_store: SchemaStore
) -> bool:
    bus_schema = load_json(schema_dir / 'bus.schema.json')
    buses = load_json(config_dir / 'system' / 'bus_configs.json')

    if validate_against_schema(buses, bus_schema, schema_store, filename='bus_configs.json'):
        print_as_ok("bus_config.json")
        return True
    else:
        print_as_error("Validation failed for bus_configs.json")
        return False

def validate_internal_nodes(
    config_dir: Path, schema_dir: Path, schema_store: SchemaStore
) -> bool:
    node_schema = load_json(schema_dir / 'node.schema.json')
    all_valid = True
    
    for node_file in sorted((config_dir / 'nodes').glob('*.json')):
        node_data = load_json(node_file)
        
        if validate_against_schema(node_data, node_schema, schema_store, filename=node_file.name):
            print_as_ok(f"{node_file.name}")
        else:
            all_valid = False
    return all_valid

def validate_external_nodes(
    config_dir: Path, schema_dir: Path, schema_store: SchemaStore
) -> bool:
    external_node_schema = load_json(schema_dir / 'external_node.schema.json')
    all_valid = True

    for node_file in sorted((config_dir / 'external_nodes').glob('*.json')):
        node_data = load_json(node_file)
        
        if validate_against_schema(node_data, external_node_schema, schema_store, filename=node_file.name):
            print_as_ok(f"{node_file.name}")
        else:
            all_valid = False
    return all_valid

def validate_all(config_dir: Path, schema_dir: Path) -> bool:
    print("Validating configs against schema...")

    all_valid = True
    
    # Load shared schemas for references
    message_schema = load_json(schema_dir / 'message.schema.json')
    signal_schema = load_json(schema_dir / 'signal.schema.json')
    fault_schema = load_json(schema_dir / 'fault.schema.json')
    
    schema_store = {
        'https://github.com/PER/canpiler/message.schema.json': message_schema,
        'https://github.com/PER/canpiler/signal.schema.json': signal_schema,
        'https://github.com/PER/canpiler/fault.schema.json': fault_schema,
    }

    # Validate custom types schema
    if not validate_common_types(config_dir, schema_dir):
        all_valid = False

    # Validate bus configs
    if not validate_bus_config(config_dir, schema_dir, schema_store):
        all_valid = False
    
    # Validate node configs
    if not validate_internal_nodes(config_dir, schema_dir, schema_store):
        all_valid = False
    
    if not validate_external_nodes(config_dir, schema_dir, schema_store):
        all_valid = False
    
    if all_valid:
        print_as_success("All configs passed schema validation")
        return True
    else:
        print_as_warning("One or more configs failed schema validation")
        return False
