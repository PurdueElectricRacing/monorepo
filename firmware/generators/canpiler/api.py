"""
api.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Iterable

from core.artifacts import Artifact
from core.config_models import ConfigBundle
from core.contracts import CanContribution
from .codegen import generate_headers
from .compiler import collect_declarations, compile_source
from .dbcgen import generate_dbcs
from .ir import CanIR, CanSource, LinkedCan
from .linker import link_can
from .load_calc import calculate_bus_load
from .mapper import map_hardware


class Canpiler:
    def collect_declarations(
        self,
        config: ConfigBundle,
        contributions: Iterable[CanContribution] = (),
    ) -> CanSource:
        return collect_declarations(config, contributions)

    def compile(self, source: CanSource) -> CanIR:
        return compile_source(source)

    def link(self, can_ir: CanIR) -> LinkedCan:
        return link_can(can_ir)

    def generate(
        self,
        linked: LinkedCan,
        version: str,
    ) -> list[Artifact]:
        mappings = map_hardware(linked)
        artifacts = generate_headers(linked, mappings, version)
        artifacts.extend(generate_dbcs(linked, version))
        calculate_bus_load(linked)
        return artifacts
