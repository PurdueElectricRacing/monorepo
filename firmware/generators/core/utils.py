"""
utils.py

Author: Irving Wang (irvingw@purdue.edu)
"""

import json
import hashlib
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING, Any

from jinja2 import Environment, FileSystemLoader, select_autoescape
from core.config import CAN_TEMPLATE_DIR, FAULT_TEMPLATE_DIR

if TYPE_CHECKING:
    from canpiler.parser import Message

CTYPE_SIZES = {
    "uint8_t": 8, "int8_t": 8,
    "uint16_t": 16, "int16_t": 16,
    "uint32_t": 32, "int32_t": 32,
    "uint64_t": 64, "int64_t": 64,
    "float": 32,
    "bool": 1
}

class bcolors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    ORANGE = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_as_error(message: object) -> None:
    print(f"{bcolors.RED}[ERROR] {message}{bcolors.ENDC}")

def print_as_warning(message: object) -> None:
    print(f"{bcolors.ORANGE}[WARNING] {message}{bcolors.ENDC}")

def print_as_ok(message: object) -> None:
    print(f"[OK] {message}")

def print_as_success(message: object) -> None:
    print(f"{bcolors.GREEN}[SUCCESS] {message}{bcolors.ENDC}")

def load_json(filepath: str | Path) -> Any:
    """Load JSON file"""
    with open(filepath) as f:
        return json.load(f)

def to_macro_name(name: str) -> str:
    return name.upper()

def get_git_hash() -> str:
    """
    Returns the short git hash of the current commit
    """
    try:
        result = subprocess.run(['git', 'rev-parse', '--short=7', 'HEAD'], 
                                capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"

def get_layout_hash(message: "Message") -> str:
    """
    Calculates a fingerprint hash for a message layout
    """
    layout_str = ""
    # Add signal properties that affect layout into a single string
    # ! does not account for message ID or period, as they do not affect the layout
    for sig in message.signals:
        layout_str += f"{sig.name}:{sig.c_type}:{sig.bit_shift}:{sig.length};"

    payload = layout_str.encode('utf-8')
    full_hash = hashlib.sha256(payload).hexdigest().upper()
    return f"0x{full_hash[:16]}" # take the first 16 chars (to fit in 64 bits)


def get_jinja_env() -> Environment:
    env = Environment(
        loader=FileSystemLoader([str(CAN_TEMPLATE_DIR), str(FAULT_TEMPLATE_DIR)]),
        autoescape=select_autoescape(),
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True
    )
    
    # Custom Filters
    env.filters['to_c_hex'] = lambda v: f"0x{v:X}" if v is not None else "0"
    
    def format_float(val: float) -> str:
        """Format float to .6g and ensure it looks like a float literal in C"""
        s = f"{val:.6g}"
        if '.' not in s and 'e' not in s:
            s = f"{val:.1f}"
        return s + "f"
    
    env.filters['format_float'] = format_float
    env.filters['to_c_string'] = lambda v: json.dumps(v) if v else "nullptr"
    
    return env

def render_template(
    env: Environment, template_name: str, **context: object
) -> str:
    template = env.get_template(template_name)
    return template.render(**context)
