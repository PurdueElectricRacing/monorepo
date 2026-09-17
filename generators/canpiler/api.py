"""
api.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Iterable

from core.artifacts import Artifact
from core.declarations import CanDeclarations
from core.contributions import DeclarationContribution
from .code_generator import generate_headers
from .compiler import assemble_source, compile_source
from .dbc_generator import generate_dbcs
from .pipeline_models import CanSource, CompiledCan, LinkedCan
from .linker import link_can
from .bus_load_analyzer import calculate_bus_load
from .hardware_mapper import map_hardware


class Canpiler:
    def assemble_source(
        self,
        declarations: CanDeclarations,
        contributions: Iterable[DeclarationContribution] = (),
    ) -> CanSource:
        return assemble_source(declarations, contributions)

    def compile(self, source: CanSource) -> CompiledCan:
        return compile_source(source)

    def link(self, compiled: CompiledCan) -> LinkedCan:
        return link_can(compiled)

    def generate(
        self,
        linked: LinkedCan,
        version: str,
    ) -> list[Artifact]:
        hardware_map = map_hardware(linked)
        artifacts = generate_headers(linked, hardware_map, version)
        artifacts.extend(generate_dbcs(linked, version))
        calculate_bus_load(linked)
        return artifacts
