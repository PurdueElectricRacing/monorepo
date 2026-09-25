"""
api.py

DearUnits: assembles the validated unit config into the unit graph,
then renders it into generated C artifacts.
"""

from .codegen import generate_headers
from .config_loader import UnitConfigBundle
from .models import CompoundType, DimensionTerm, EquivalenceClass, UnitGraph, UnitNode
from core.artifacts import Artifact


class DearUnits:
    def parse(self, bundle: UnitConfigBundle) -> UnitGraph:
        graph = UnitGraph()

        for config in bundle.classes.values():
            units = {
                unit.name: UnitNode(unit.name, unit.scale, unit.offset)
                for unit in config.units
            }
            units[config.base_unit] = UnitNode(config.base_unit, scale=1.0)
            graph.classes[config.name] = EquivalenceClass(
                name=config.name,
                base_name=config.base_unit,
                units=units,
            )

        for config in bundle.compounds.values():
            scales = bundle.compound_scales[config.name]
            units = {
                unit.name: UnitNode(unit.name, scales[unit.name])
                for unit in config.units
            }
            units[config.base_unit] = UnitNode(config.base_unit, scales[config.base_unit])
            graph.compounds[config.name] = CompoundType(
                name=config.name,
                base_name=config.base_unit,
                dimensions=tuple(
                    DimensionTerm(term.group_name, term.exponent)
                    for term in config.composed_of
                ),
                units=units,
            )

        return graph

    def generate(self, graph: UnitGraph) -> list[Artifact]:
        return generate_headers(graph)
