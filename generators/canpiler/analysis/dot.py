"""Shared Graphviz formatting for CAN topology artifacts."""

from __future__ import annotations

import json

from core.artifacts import Artifact
from core.utils import print_as_ok


FONT_NAME = "Helvetica"
RANK_DIRECTION = "LR"
GRAPH_LABEL_LOCATION = "t"
GRAPH_BACKGROUND_COLOR = "transparent"
GRAPH_FONT_COLOR = "#222222"
NODE_SHAPE = "box"
NODE_STYLE = "filled"
EXTERNAL_NODE_STYLE = "filled,dashed"
NODE_FILL_COLOR = "#D5D5D5"
NODE_BORDER_COLOR = "#777777"
NODE_FONT_COLOR = "#111111"
EDGE_COLOR = "#2F70AD"
EDGE_FONT_COLOR = "#333333"
TOPOLOGY_COLLECTION = "topology"


def quote(value: str) -> str:
    """Return a Graphviz-compatible quoted string."""
    return json.dumps(value, ensure_ascii=False)


def graph_header(name: str, title: str, *, directed: bool) -> list[str]:
    """Return the common graph, node, and edge style declarations."""
    lines = [
        f"{'digraph' if directed else 'graph'} {name} {{",
        f"    rankdir={RANK_DIRECTION};",
        "    graph ["
        f"fontname={quote(FONT_NAME)}, "
        f"labelloc={quote(GRAPH_LABEL_LOCATION)}, "
        f"bgcolor={quote(GRAPH_BACKGROUND_COLOR)}, "
        f"fontcolor={quote(GRAPH_FONT_COLOR)}, "
        f"label={quote(title)}"
        "];",
        "    node ["
        f"fontname={quote(FONT_NAME)}, "
        f"shape={NODE_SHAPE}, style={NODE_STYLE}, "
        f"fillcolor={quote(NODE_FILL_COLOR)}, "
        f"color={quote(NODE_BORDER_COLOR)}, "
        f"fontcolor={quote(NODE_FONT_COLOR)}"
        "];",
    ]
    if directed:
        lines.extend((
            "    edge ["
            f"fontname={quote(FONT_NAME)}, "
            f"color={quote(EDGE_COLOR)}, "
            f"fontcolor={quote(EDGE_FONT_COLOR)}"
            "];",
        ))
    else:
        lines.append(f"    edge [color={quote(EDGE_COLOR)}];")
    lines.append("")
    return lines


def node_line(identifier: str, name: str, *, is_external: bool) -> str:
    """Render a CAN node with the shared external-node style."""
    attributes = [f"label={quote(name)}"]
    if is_external:
        attributes.append(f"style={quote(EXTERNAL_NODE_STYLE)}")
    return f"    {identifier} [{', '.join(attributes)}];"


def topology_artifact(filename: str, lines: list[str]) -> Artifact:
    """Package a DOT graph in the topology artifact collection."""
    print_as_ok(f"Generated {filename}")
    return Artifact(TOPOLOGY_COLLECTION, filename, "\n".join(lines) + "\n")
