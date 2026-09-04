from pathlib import Path


GENERATOR_DIR   = Path(__file__).resolve().parents[1]
CAN_LIBRARY_DIR = GENERATOR_DIR.parent / "can_library"

CONFIG_DIR         = GENERATOR_DIR / "configs"
CAN_TEMPLATE_DIR   = GENERATOR_DIR / "canpiler" / "templates"
FAULT_TEMPLATE_DIR = GENERATOR_DIR / "faultgen" / "templates"
GENERATED_DIR      = CAN_LIBRARY_DIR / "generated"
DBC_DIR            = CAN_LIBRARY_DIR / "dbc"

SYSTEM_CONFIG_DIR        = CONFIG_DIR / "system"
NODE_CONFIG_DIR          = CONFIG_DIR / "nodes"
EXTERNAL_NODE_CONFIG_DIR = CONFIG_DIR / "external_nodes"
COMMON_TYPES_CONFIG_PATH = SYSTEM_CONFIG_DIR / "common_types.json"
BUS_CONFIG_PATH          = SYSTEM_CONFIG_DIR / "bus_configs.json"
