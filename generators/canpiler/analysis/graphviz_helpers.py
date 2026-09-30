"""Shared Graphviz formatting for CAN topology artifacts."""

from __future__ import annotations

import json

from core.artifacts import Artifact
from core.utils import print_as_ok
from ..pipeline.models import LinkedMessage


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
CAN_GRAPHS_COLLECTION = "can_graphs"


def quote(value: str) -> str:
    """Return a Graphviz-compatible quoted string."""
    return json.dumps(value, ensure_ascii=False)


def node_id(node_name: str, *, bus_name: str | None = None) -> str:
    """Return a node ID, scoped to a bus when used in a per-bus graph."""
    if bus_name is None:
        return quote(f"node:{node_name}")
    return quote(f"node:{bus_name}:{node_name}")


def bus_id(bus_name: str) -> str:
    return quote(f"bus:{bus_name}")


def message_id(bus_name: str, message_name: str) -> str:
    return quote(f"message:{bus_name}:{message_name}")


def message_label(message: LinkedMessage) -> str:
    """Summarize a linked CAN message for a Graphviz node."""
    period = f"{message.period_ms} ms" if message.period_ms > 0 else "aperiodic"
    frame_type = "extended" if message.is_extended else "standard"
    return "\n".join((
        message.message_name,
        f"ID 0x{message.final_id:X} ({frame_type})",
        f"{period} · {message.dlc} bytes · priority {message.priority}",
    ))


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


def can_graph_artifact(filename: str, lines: list[str]) -> Artifact:
    """Package a DOT graph in the CAN graphs artifact collection."""
    print_as_ok(f"Generated {filename}")
    return Artifact(CAN_GRAPHS_COLLECTION, filename, "\n".join(lines) + "\n")
