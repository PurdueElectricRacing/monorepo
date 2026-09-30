"""
api.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Iterable

from core.artifacts import Artifact
from core.declarations import CanDeclarations
from core.contributions import DeclarationContribution
from .codegen.generator import generate_headers
from .pipeline.compiler import assemble_source, compile_source
from .dbc.generator import generate_dbcs
from .superdbc.generator import generate_superdbc_schema, generate_superdbc
from .pipeline.models import CanSource, CompiledCan, LinkedCan
from .pipeline.linker import link_can
from .analysis.bus_load import calculate_bus_load
from .analysis.bus_membership import generate_bus_membership_graph
from .analysis.node_communication import generate_node_communication_graph
from .analysis.message_flow import generate_message_flow_graphs
from .codegen.hardware import map_hardware


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
        artifacts.append(generate_superdbc(linked, version))
        artifacts.append(generate_superdbc_schema())
        artifacts.extend(generate_dbcs(linked, version))
        artifacts.extend(generate_message_flow_graphs(linked))
        artifacts.append(generate_bus_membership_graph(linked))
        artifacts.append(generate_node_communication_graph(linked))
        calculate_bus_load(linked)
        return artifacts
