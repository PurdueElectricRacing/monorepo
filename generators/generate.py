"""
generate.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from core.artifacts import clear_artifacts, write_artifacts
from core.config import DBC_DIR, GENERATED_DIR, TOPOLOGY_DIR, UNITS_GENERATED_DIR
from core.declaration_loader import DeclarationValidationError, load_declarations
from canpiler.api import Canpiler
from canpiler.pipeline.compiler import CanCompilationError
from faultgen.api import FaultGenerator
from dearunits.api import DearUnits
from dearunits.config_loader import load_unit_config_bundle
from core.utils import get_git_hash, print_as_error

def generate() -> None:
    canpiler = Canpiler()
    faultgen = FaultGenerator()
    dearunits = DearUnits()
    declarations = load_declarations()
    unit_config = load_unit_config_bundle()

    fault_plan         = faultgen.plan(declarations)
    fault_contribution = faultgen.contribute(fault_plan)
    source             = canpiler.assemble_source(
        declarations,
        [fault_contribution],
    )
    compiled           = canpiler.compile(source)
    linked_can         = canpiler.link(compiled)
    version            = get_git_hash()
    unit_graph         = dearunits.parse(unit_config)

    artifacts  = canpiler.generate(linked_can, version)
    artifacts += faultgen.generate(fault_plan, version)
    artifacts += dearunits.generate(unit_graph)

    output_roots = {
        "generated": GENERATED_DIR,
        "dbc": DBC_DIR,
        "topology": TOPOLOGY_DIR,
        "units_generated": UNITS_GENERATED_DIR,
    }
    clear_artifacts(
        output_roots,
        {
            "generated": "*",
            "dbc": (
                "*.dbc",
                "superdbc_*.json",
                "superdbc.schema.json"
            ),
            "topology": "topology_*.dot",
            "units_generated": "*",
        },
    )
    write_artifacts(output_roots, artifacts)


def main() -> int:
    try:
        generate()
    except (DeclarationValidationError, CanCompilationError):
        return 1
    except ValueError as error:
        print_as_error(error)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
