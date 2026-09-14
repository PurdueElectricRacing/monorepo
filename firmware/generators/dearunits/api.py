"""
api.py

DearUnits: assembles the validated unit config into the unit graph.
Codegen stages (compile/generate) land in a later step.
"""

from .config_loader import UnitConfigBundle
from .models import CompoundType, DimensionTerm, EquivalenceClass, UnitGraph, UnitNode


class DearUnits:
    def parse(self, bundle: UnitConfigBundle) -> UnitGraph:
        graph = UnitGraph()

        for config in bundle.classes.values():
            graph.classes[config.name] = EquivalenceClass(
                name=config.name,
                base_symbol=config.base_unit,
                units={
                    unit.symbol: UnitNode(unit.symbol, unit.name, unit.scale, unit.offset)
                    for unit in config.units
                },
            )

        for config in bundle.compounds.values():
            graph.compounds[config.name] = CompoundType(
                name=config.name,
                base_symbol=config.base_unit,
                dimensions=tuple(
                    DimensionTerm(term.class_name, term.exponent)
                    for term in config.dimensions
                ),
                units={
                    unit.symbol: UnitNode(unit.symbol, unit.name, unit.scale, unit.offset)
                    for unit in config.units
                },
            )

        return graph
