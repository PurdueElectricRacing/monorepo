from core.artifacts import clear_artifacts, write_artifacts
from core.config import DBC_DIR, GENERATED_DIR
from core.config_loader import load_config_bundle
from canpiler.api import Canpiler
from faultgen.api import FaultGenerator

def generate():
    canpiler = Canpiler()
    faultgen = FaultGenerator()
    config = load_config_bundle()

    can_model          = canpiler.parse(config)
    fault_plan         = faultgen.plan(config)
    fault_contribution = faultgen.contribute(fault_plan)
    compiled_can       = canpiler.compile(can_model, [fault_contribution])

    artifacts  = canpiler.generate(compiled_can)
    artifacts += faultgen.generate(fault_plan, compiled_can)

    output_roots = {"generated": GENERATED_DIR, "dbc": DBC_DIR}
    clear_artifacts(output_roots, {"generated": "*", "dbc": "*.dbc"})
    write_artifacts(output_roots, artifacts)

if __name__ == "__main__":
    generate()
