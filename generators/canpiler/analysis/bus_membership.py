"""Generate a Graphviz graph of CAN nodes and their bus attachments."""

from __future__ import annotations

from core.artifacts import Artifact
from .graphviz_helpers import (
    BUS_NODE_FILL_COLOR, bus_id, can_graph_artifact, graph_header, node_id,
    node_line, quote,
)
from ..pipeline.models import LinkedCan


def generate_bus_membership_graph(linked: LinkedCan) -> Artifact:
    """Generate a system-wide graph of nodes and their CAN bus attachments."""
    filename = "bus_membership.dot"
    lines = graph_header(filename, "Bus Membership", directed=False)

    for bus_name, config in sorted(linked.bus_configs.items()):
        label = f"{bus_name}\n{config.baud_rate // 1000} kbit/s"
        lines.append(
            f"\t{bus_id(bus_name)} [label={quote(label)}, shape=ellipse, "
            f"fillcolor={quote(BUS_NODE_FILL_COLOR)}];"
        )

    lines.append("")
    for node in sorted(linked.nodes, key=lambda item: item.name):
        lines.append(node_line(node_id(node.name), node.name, is_external=node.is_external))
        for bus_name in sorted(node.busses):
            lines.append(f"\t{bus_id(bus_name)} -- {node_id(node.name)};")

    lines.append("}")
    return can_graph_artifact(filename, lines)
