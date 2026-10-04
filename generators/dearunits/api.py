"""
api.py

DearUnits: assembles the validated unit config into the unit graph,
then renders it into generated C artifacts.
"""

from .codegen import generate_headers
from .config_loader import UnitConfigBundle
from .models import BaseQuantity, DerivedQuantity, DimensionTerm, Relation, Unit, UnitGraph
from core.artifacts import Artifact

class DearUnits:
    def parse(self, bundle: UnitConfigBundle) -> UnitGraph:
        graph = UnitGraph()

        for config in bundle.base_quantities.values():
            units = {
                unit.name: Unit(unit.name, unit.scale, unit.offset)
                for unit in config.units
            }
            units[config.base_unit] = Unit(config.base_unit, scale=1.0)
            graph.base_quantities[config.name] = BaseQuantity(
                name=config.name,
                base_name=config.base_unit,
                units=units,
            )

        for config in bundle.derived_quantities.values():
            scales = bundle.derived_scales[config.name]
            units = {
                unit.name: Unit(unit.name, scales[unit.name])
                for unit in config.units
            }
            units[config.base_unit] = Unit(config.base_unit, scales[config.base_unit])
            graph.derived_quantities[config.name] = DerivedQuantity(
                name=config.name,
                base_name=config.base_unit,
                dimensions=tuple(
                    DimensionTerm(term.group_name, term.exponent)
                    for term in config.composed_of
                ),
                units=units,
            )

        graph.relations = [Relation(r.factor_a, r.factor_b, r.result) for r in bundle.relations]
        graph.angle_class = bundle.angle_class

        return graph

    def generate(self, graph: UnitGraph) -> list[Artifact]:
        return generate_headers(graph)
