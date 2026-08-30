from pathlib import Path

from core.artifacts import clear_artifacts, write_artifacts
from core.config import GenerationPaths
from core.validator import validate_all
from canpiler.api import Canpiler
from faultgen.api import FaultGenerator


GENERATOR_DIR = Path(__file__).resolve().parent
CAN_LIBRARY_DIR = GENERATOR_DIR.parent / "can_library"

paths = GenerationPaths(
    config_dir=GENERATOR_DIR / "configs",
    schema_dir=GENERATOR_DIR / "schema",
    can_template_dir=GENERATOR_DIR / "canpiler" / "templates",
    fault_template_dir=GENERATOR_DIR / "faultgen" / "templates",
    generated_dir=CAN_LIBRARY_DIR / "generated",
    dbc_dir=CAN_LIBRARY_DIR / "dbc",
)


if not validate_all(paths.config_dir, paths.schema_dir):
    raise ValueError("Configuration validation failed")

canpiler = Canpiler(paths)
faultgen = FaultGenerator(paths)

can_model = canpiler.parse()
fault_plan = faultgen.plan()
fault_contribution = faultgen.contribute(fault_plan)
compiled_can = canpiler.compile(can_model, [fault_contribution])

artifacts = canpiler.generate(compiled_can)
artifacts += faultgen.generate(fault_plan, compiled_can)

output_roots = {"generated": paths.generated_dir, "dbc": paths.dbc_dir}
clear_artifacts(output_roots, {"generated": "*", "dbc": "*.dbc"})
write_artifacts(output_roots, artifacts)
