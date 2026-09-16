"""
generate.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from core.artifacts import clear_artifacts, write_artifacts
from core.config import DBC_DIR, GENERATED_DIR
from core.config_loader import load_config_bundle
from canpiler.api import Canpiler
from faultgen.api import FaultGenerator
from core.utils import get_git_hash

def generate():
    canpiler = Canpiler()
    faultgen = FaultGenerator()
    config = load_config_bundle()

    fault_plan         = faultgen.plan(config)
    fault_contribution = faultgen.contribute(fault_plan)
    source             = canpiler.collect_declarations(config, [fault_contribution])
    can_ir             = canpiler.compile(source)
    linked_can         = canpiler.link(can_ir)
    version            = get_git_hash()

    artifacts  = canpiler.generate(linked_can, version)
    artifacts += faultgen.generate(fault_plan, version)

    output_roots = {"generated": GENERATED_DIR, "dbc": DBC_DIR}
    clear_artifacts(output_roots, {"generated": "*", "dbc": "*.dbc"})
    write_artifacts(output_roots, artifacts)

if __name__ == "__main__":
    generate()
