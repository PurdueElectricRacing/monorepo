"""
generate.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from core.artifacts import clear_artifacts, write_artifacts
from core.config import (
    CAN_GRAPHS_DIR,
    FIL_CONFIG_DIR,
    SUPERDBC_DIR,
    GENERATED_DIR,
    UNITS_GENERATED_DIR,
)
from core.declaration_loader import DeclarationValidationError, load_declarations
from canpiler.api import Canpiler
from canpiler.pipeline.compiler import CanCompilationError
from faultgen.api import FaultGenerator
from filgen.api import (
    FilGenerator,
    FilScenarioError,
    READY_TO_DRIVE_SCENARIO_FILE,
    load_fil_scenario,
)
from dearunits.api import DearUnits
from dearunits.config_loader import UnitConfigValidationError, load_unit_config_bundle
from core.utils import get_git_hash, print_as_error

def generate() -> None:
    canpiler = Canpiler()
    faultgen = FaultGenerator()
    dearunits = DearUnits()
    filgen = FilGenerator()
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
    fil_scenario       = load_fil_scenario(READY_TO_DRIVE_SCENARIO_FILE)

    artifacts  = canpiler.generate(linked_can, version)
    artifacts += faultgen.generate(fault_plan, version)
    # No version argument here: the FIL JSON schemas forbid unknown keys,
    # so the git version cannot be embedded in the emitted files.
    artifacts += filgen.generate(linked_can, fil_scenario)
    artifacts += dearunits.generate(unit_graph)

    output_roots = {
        "generated": GENERATED_DIR,
        "superdbc": SUPERDBC_DIR,
        "can_graphs": CAN_GRAPHS_DIR,
        "units_generated": UNITS_GENERATED_DIR,
        "fil": FIL_CONFIG_DIR,
    }
    clear_artifacts(
        output_roots,
        {
            "generated": "*",
            "superdbc": (
                "superdbc_*.json",
                "superdbc.schema.json"
            ),
            "can_graphs": (
                "topology_*.dot",
                "message_flow_*.dot",
                "*_message_flow.dot",
                "bus_membership.dot",
                "node_communication.dot",
            ),
            "units_generated": "*",
            "fil": (
                "boards/*.json",
                "network.json",
                "ready_to_drive.json",
                "stimuli/*.json",
            ),
        },
    )
    write_artifacts(output_roots, artifacts)


def main() -> int:
    try:
        generate()
    except (DeclarationValidationError, CanCompilationError, UnitConfigValidationError):
        return 1
    except ValueError as error:
        print_as_error(error)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
