import argparse
from pathlib import Path

from core.artifacts import clear_artifacts, write_artifacts
from core.config import GenerationPaths
from core.validator import validate_all
from canpiler.api import Canpiler
from faultgen.api import FaultGenerator


GENERATOR_DIR = Path(__file__).resolve().parent
DEFAULT_CAN_LIBRARY_DIR = GENERATOR_DIR.parent / "can_library"


def generate(paths: GenerationPaths):
    canpiler, faultgen = Canpiler(paths), FaultGenerator(paths)
    if not validate_all(paths.config_dir, paths.schema_dir):
        raise ValueError("Configuration validation failed")

    can_model = canpiler.parse()
    fault_plan = faultgen.plan()
    compiled = canpiler.compile(can_model, [faultgen.contribute(fault_plan)])

    artifacts = canpiler.generate(compiled) + faultgen.generate(fault_plan, compiled)
    roots = {"generated": paths.generated_dir, "dbc": paths.dbc_dir}
    clear_artifacts(roots, {"generated": "*", "dbc": "*.dbc"})
    write_artifacts(roots, artifacts)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config-dir", type=Path, default=GENERATOR_DIR / "configs")
    parser.add_argument("--schema-dir", type=Path, default=GENERATOR_DIR / "schema")
    parser.add_argument("--generated-dir", type=Path, default=DEFAULT_CAN_LIBRARY_DIR / "generated")
    parser.add_argument("--dbc-dir", type=Path, default=DEFAULT_CAN_LIBRARY_DIR / "dbc")
    args = parser.parse_args()
    generate(GenerationPaths(
        config_dir=args.config_dir,
        schema_dir=args.schema_dir,
        can_template_dir=GENERATOR_DIR / "canpiler" / "templates",
        fault_template_dir=GENERATOR_DIR / "faultgen" / "templates",
        generated_dir=args.generated_dir,
        dbc_dir=args.dbc_dir,
    ))


if __name__ == "__main__":
    main()
