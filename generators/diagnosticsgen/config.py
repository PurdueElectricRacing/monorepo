from pathlib import Path

from pydantic import Field

from core.config import SYSTEM_CONFIG_DIR
from core.declarations import DeclarationModel


class DiagnosticsConfig(DeclarationModel):
    nodes: dict[str, str] = Field(min_length=1)


def load_diagnostics_config(
    path: Path = SYSTEM_CONFIG_DIR / "diagnostics.json",
) -> DiagnosticsConfig:
    try:
        return DiagnosticsConfig.model_validate_json(path.read_bytes())
    except (OSError, ValueError) as error:
        raise ValueError(f"Invalid diagnostics configuration in {path}: {error}") from error
