"""
config.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from pathlib import Path


GENERATOR_DIR   = Path(__file__).resolve().parents[1]
REPOSITORY_DIR  = GENERATOR_DIR.parent
CAN_LIBRARY_DIR = REPOSITORY_DIR / "firmware" / "can_library"

CONFIG_DIR         = GENERATOR_DIR / "configs"
CAN_TEMPLATE_DIR   = GENERATOR_DIR / "canpiler" / "codegen" / "templates"
FAULT_TEMPLATE_DIR = GENERATOR_DIR / "faultgen" / "templates"
GENERATED_DIR      = CAN_LIBRARY_DIR / "generated"
DBC_DIR            = REPOSITORY_DIR / "dbc"
TOPOLOGY_DIR       = REPOSITORY_DIR / "docs" / "topology"

SYSTEM_CONFIG_DIR        = CONFIG_DIR / "system"
NODE_CONFIG_DIR          = CONFIG_DIR / "nodes"
EXTERNAL_NODE_CONFIG_DIR = CONFIG_DIR / "external_nodes"
COMMON_TYPES_CONFIG_PATH = SYSTEM_CONFIG_DIR / "common_types.json"
BUS_CONFIG_PATH          = SYSTEM_CONFIG_DIR / "bus_configs.json"
