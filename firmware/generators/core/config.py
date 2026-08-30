from pathlib import Path


GENERATOR_DIR = Path(__file__).resolve().parents[1]
CAN_LIBRARY_DIR = GENERATOR_DIR.parent / "can_library"

CONFIG_DIR = GENERATOR_DIR / "configs"
SCHEMA_DIR = GENERATOR_DIR / "schema"
CAN_TEMPLATE_DIR = GENERATOR_DIR / "canpiler" / "templates"
FAULT_TEMPLATE_DIR = GENERATOR_DIR / "faultgen" / "templates"
GENERATED_DIR = CAN_LIBRARY_DIR / "generated"
DBC_DIR = CAN_LIBRARY_DIR / "dbc"
