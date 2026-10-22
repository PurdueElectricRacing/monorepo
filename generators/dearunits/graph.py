"""
graph.py

Renders the DearUnits unit graph into Graphviz DOT diagrams
"""

from __future__ import annotations

import html
import json

from .codegen import _combine, _dimensions
from .models import DimensionTerm, Unit, UnitGraph
from core.artifacts import Artifact
from core.utils import print_as_ok


FONT_NAME = "Helvetica"
GRAPH_FONT_COLOR = "#8A8A8A"
NODE_FONT_COLOR = "#111111"
EDGE_FONT_COLOR = "#8A8A8A"
BASE_FILL_COLOR = "#B8D6EF"
DERIVED_FILL_COLOR = "#D5EBC8"
BORDER_COLOR = "#777777"
COMPOSITION_EDGE_COLOR = "#2F70AD"
RELATION_EDGE_COLOR = "#B5562F"
UNIT_GRAPHS_COLLECTION = "unit_graphs"


def _quote(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def _quantity_id(name: str) -> str:
    return _quote(f"quantity:{name}")


def _graph_header(filename: str, title: str, *, edge_color: str, rankdir: str = "LR") -> list[str]:
    return [
        f"digraph {_quote(filename)} {{",
        f"\trankdir={rankdir};",
        f"\tgraph [fontname={_quote(FONT_NAME)}, labelloc=\"t\", bgcolor=\"transparent\", "
        f"fontcolor={_quote(GRAPH_FONT_COLOR)}, label={_quote(title)}];",
        f"\tnode [fontname={_quote(FONT_NAME)}, shape=box, style=\"filled,rounded\", "
        f"color={_quote(BORDER_COLOR)}, fontcolor={_quote(NODE_FONT_COLOR)}];",
        f"\tedge [fontname={_quote(FONT_NAME)}, color={_quote(edge_color)}, "
        f"fontcolor={_quote(EDGE_FONT_COLOR)}];",
        "",
    ]


def _quantity_node(name: str, fill_color: str) -> str:
    return f"\t{_quantity_id(name)} [label={_quote(name)}, fillcolor={_quote(fill_color)}];"


def _unit_graph_artifact(filename: str, lines: list[str]) -> Artifact:
    lines.append("}")
    print_as_ok(f"Generated {filename}")
    return Artifact(UNIT_GRAPHS_COLLECTION, filename, "\n".join(lines) + "\n")


def _junction_node(identifier: str, color: str, label: str = "×") -> str:
    return (
        f"\t{identifier} [label={_quote(label)}, shape=circle, style=filled, width=0.3, fixedsize=true, "
        f"fillcolor=\"white\", color={_quote(color)}, fontcolor={_quote(color)}];"
    )


def _generate_relations_graph(graph: UnitGraph) -> Artifact:
    filename = "unit_relations.dot"
    lines = _graph_header(filename, "DearUnits Relations", edge_color=RELATION_EDGE_COLOR)

    involved = {name for relation in graph.relations for name in (relation.factor_a, relation.factor_b, relation.result)}
    for name in sorted(involved):
        fill_color = BASE_FILL_COLOR if name in graph.base_quantities else DERIVED_FILL_COLOR
        lines.append(_quantity_node(name, fill_color))

    lines.append("")
    for index, relation in enumerate(graph.relations):
        junction = _quote(f"relation:{index}")
        lines.append(_junction_node(junction, RELATION_EDGE_COLOR))
        lines.append(f"\t{_quantity_id(relation.factor_a)} -> {junction} [arrowhead=none];")
        lines.append(f"\t{_quantity_id(relation.factor_b)} -> {junction} [arrowhead=none];")
        lines.append(f"\t{junction} -> {_quantity_id(relation.result)};")

    return _unit_graph_artifact(filename, lines)


def _hub_node(name: str, base_name: str, units: dict[str, Unit], fill_color: str) -> str:
    rows = [
        f"<TR><TD COLSPAN=\"2\" BORDER=\"0\"><B>{html.escape(name)}</B></TD></TR>",
        f"<TR><TD PORT=\"hub\" COLSPAN=\"2\" BGCOLOR={_quote(fill_color)}><B>{html.escape(base_name)}</B></TD></TR>",
    ]
    rows += [
        f"<TR><TD ALIGN=\"LEFT\">{html.escape(unit_name)}</TD><TD ALIGN=\"RIGHT\">{_conversion_label(unit)}</TD></TR>"
        for unit_name, unit in sorted(units.items())
        if unit_name != base_name
    ]
    label = (
        f"<<TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"2\" CELLPADDING=\"3\" STYLE=\"rounded\" "
        f"BGCOLOR=\"white\" COLOR={_quote(BORDER_COLOR)}>{''.join(rows)}</TABLE>>"
    )
    return f"\t{_quantity_id(name)} [label={label}, shape=plain];"


def _conversion_label(unit: Unit) -> str:
    label = f"×{unit.scale:g}"
    if unit.offset:
        label += f" {'+' if unit.offset > 0 else '−'} {abs(unit.offset):g}"
    return label


def _unit_row(unit: Unit, is_base: bool) -> str:
    name = html.escape(unit.name)
    if is_base:
        return f"<TR><TD ALIGN=\"LEFT\"><B>{name}</B></TD><TD ALIGN=\"RIGHT\">base</TD></TR>"
    return f"<TR><TD ALIGN=\"LEFT\">{name}</TD><TD ALIGN=\"RIGHT\">{_conversion_label(unit)}</TD></TR>"


def _conversion_node(name: str, base_name: str, units: dict[str, Unit], fill_color: str, order: int) -> str:
    rows = [_unit_row(units[base_name], is_base=True)]
    rows += [_unit_row(unit, is_base=False) for unit_name, unit in sorted(units.items()) if unit_name != base_name]
    label = (
        f"<<TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"2\">"
        f"<TR><TD COLSPAN=\"2\"><B>{html.escape(name)}</B></TD></TR>"
        f"{''.join(rows)}</TABLE>>"
    )
    return f"\t{_quantity_id(name)} [label={label}, fillcolor={_quote(fill_color)}, sortv={order}];"


def _generate_conversions_graph(graph: UnitGraph) -> Artifact:
    filename = "unit_conversions.dot"
    lines = _graph_header(filename, "DearUnits Conversions", edge_color=BORDER_COLOR, rankdir="TB")
    lines.insert(2, "\tpack=true;")
    lines.insert(3, "\tpackmode=\"array_u5\";")

    quantities = [
        *((name, base, BASE_FILL_COLOR) for name, base in sorted(graph.base_quantities.items())),
        *((name, derived, DERIVED_FILL_COLOR) for name, derived in sorted(graph.derived_quantities.items())),
    ]
    for order, (name, quantity, fill_color) in enumerate(quantities):
        lines.append(_conversion_node(name, quantity.base_name, quantity.units, fill_color, order))

    return _unit_graph_artifact(filename, lines)


def _flow_candidates(graph: UnitGraph, names: list[str]) -> dict[str, list[tuple[str, str, int, bool]]]:
    dims = {name: _dimensions(name, graph) for name in names}
    by_dims: dict[tuple[tuple[str, int], ...], list[str]] = {}
    for name, quantity_dims in dims.items():
        by_dims.setdefault(tuple(sorted(quantity_dims.items())), []).append(name)
    ambiguous = {name for name, quantity_dims in dims.items() if len(by_dims[tuple(sorted(quantity_dims.items()))]) > 1}

    products: dict[tuple[str, str], str] = {}
    quotients: dict[tuple[str, str], str] = {}
    for relation in graph.relations:
        products[(relation.factor_a, relation.factor_b)] = relation.result
        products[(relation.factor_b, relation.factor_a)] = relation.result
        quotients[(relation.result, relation.factor_a)] = relation.factor_b
        quotients[(relation.result, relation.factor_b)] = relation.factor_a

    def result(a: str, b: str, sign: int) -> tuple[str | None, bool]:
        explicit = (products if sign > 0 else quotients).get((a, b))
        if explicit:
            return explicit, True
        if a in ambiguous or b in ambiguous:
            return None, False
        matches = by_dims.get(_combine(dims[a], dims[b], sign), [])
        return (matches[0] if len(matches) == 1 else None), False

    candidates: dict[str, list[tuple[str, str, int, bool]]] = {}
    for a in names:
        for b in names:
            for sign in (1, -1):
                name, explicit = result(a, b, sign)
                if name and name in graph.derived_quantities and name not in (a, b):
                    candidates.setdefault(name, []).append((a, b, sign, explicit))
    return candidates


def _flow_choices(graph: UnitGraph) -> dict[str, tuple[str, str, int] | DimensionTerm]:
    candidates = _flow_candidates(graph, [*graph.base_quantities, *graph.derived_quantities])
    level = {name: 0 for name in graph.base_quantities}
    choices: dict[str, tuple[str, str, int] | DimensionTerm] = {}
    pending = set(graph.derived_quantities)
    while pending:
        best = None
        for name in sorted(pending):
            for a, b, sign, explicit in candidates.get(name, []):
                if a in level and b in level:
                    key = (max(level[a], level[b]), not explicit, sign < 0, a, b)
                    if best is None or key < best[0]:
                        best = (key, name, (a, b, sign))
        if best is None:
            break
        key, name, choice = best
        level[name] = key[0] + 1
        choices[name] = choice
        pending.discard(name)

    for name, derived in graph.derived_quantities.items():
        if len(derived.dimensions) == 1:
            choices[name] = derived.dimensions[0]
    return choices


def _generate_flow_graph(graph: UnitGraph) -> Artifact:
    filename = "unit_flow.dot"
    lines = _graph_header(filename, "DearUnits Flow", edge_color=COMPOSITION_EDGE_COLOR)
    lines[-1:-1] = ["\tnodesep=0.3;", "\tranksep=0.8;"]

    hub_ids = []
    for name, base in sorted(graph.base_quantities.items()):
        hub = _quantity_id(name)
        hub_ids.append(hub)
        lines.append(
            f"\t{hub} [label=<<B>{html.escape(name)}</B><BR/>{html.escape(base.base_name)}>, "
            f"fillcolor={_quote(BASE_FILL_COLOR)}];"
        )
        for unit_name, unit in sorted(base.units.items()):
            if unit_name == base.base_name:
                continue
            unit_id = _quote(f"unit:{name}:{unit_name}")
            lines.append(f"\t{unit_id} [label={_quote(unit_name)}, fillcolor=\"white\"];")
            lines.append(
                f"\t{unit_id} -> {hub} [label={_quote(_conversion_label(unit))}, "
                f"color={_quote(BORDER_COLOR)}, tailport=e, headport=w];"
            )
    lines.append("\t{ rank=same; " + "; ".join(hub_ids) + "; }")

    for name, derived in sorted(graph.derived_quantities.items()):
        lines.append(_hub_node(name, derived.base_name, derived.units, DERIVED_FILL_COLOR))

    def source(name: str) -> str:
        return f"{_quantity_id(name)}:e" if name in graph.base_quantities else f"{_quantity_id(name)}:hub:e"

    def target(name: str) -> str:
        return f"{_quantity_id(name)}:hub:w"

    lines.append("")
    for name, choice in sorted(_flow_choices(graph).items()):
        if isinstance(choice, DimensionTerm):
            label = "" if choice.exponent == 1 else f" [label={_quote(str(choice.exponent))}]"
            lines.append(f"\t{source(choice.quantity_name)} -> {target(name)}{label};")
            continue
        a, b, sign = choice
        junction = _quote(f"op:{name}")
        lines.append(_junction_node(junction, COMPOSITION_EDGE_COLOR, "×" if sign > 0 else "÷"))
        if a == b:
            lines.append(f"\t{source(a)} -> {junction} [arrowhead=none, label=\"2\"];")
        else:
            lines.append(f"\t{source(a)} -> {junction} [arrowhead=none];")
            lines.append(f"\t{source(b)} -> {junction} [arrowhead=none{', style=dashed' if sign < 0 else ''}];")
        lines.append(f"\t{junction} -> {target(name)};")

    return _unit_graph_artifact(filename, lines)


def generate_unit_graphs(graph: UnitGraph) -> list[Artifact]:
    return [
        _generate_flow_graph(graph),
        _generate_relations_graph(graph),
        _generate_conversions_graph(graph),
    ]
