from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class GenerationPaths:
    config_dir: Path
    schema_dir: Path
    can_template_dir: Path
    fault_template_dir: Path
    generated_dir: Path
    dbc_dir: Path
