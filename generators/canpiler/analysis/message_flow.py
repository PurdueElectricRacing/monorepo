"""Generate Graphviz communication-topology graphs from linked CAN data."""

from __future__ import annotations

from collections import defaultdict

from core.artifacts import Artifact
from .graphviz_helpers import (
    MESSAGE_NODE_FILL_COLOR, can_graph_artifact, graph_header, message_id,
    message_label, node_id, node_line, quote,
)
from ..pipeline.models import LinkedCan


def _generate_message_flow_graph(linked: LinkedCan, bus_name: str) -> Artifact:
    filename = f"{bus_name}_message_flow.dot"
    nodes = sorted(
        (node for node in linked.nodes if bus_name in node.busses),
        key=lambda node: node.name,
    )
    messages = sorted(
        (item for item in linked.tx_messages if item.bus_name == bus_name),
        key=lambda item: (
            item.message.final_id,
            item.message.message_name,
            item.node_name,
        ),
    )
    subscriptions = defaultdict(list)
    for subscription in linked.subscriptions:
        if subscription.bus_name == bus_name:
            subscriptions[subscription.message_name].append(subscription)

    lines = graph_header(filename, f"{bus_name} Message Flow", directed=True)

    for node in nodes:
        lines.append(node_line(
            node_id(node.name, bus_name=bus_name), node.name,
            is_external=node.is_external,
        ))

    if nodes and messages:
        lines.append("")

    for placed in messages:
        message = placed.message
        placed_message_id = message_id(bus_name, message.message_name)
        lines.extend((
            f"\t{placed_message_id} [label={quote(message_label(message))}, "
            f"style={quote('rounded,filled')}, "
            f"fillcolor={quote(MESSAGE_NODE_FILL_COLOR)}];",
            f"\t{node_id(placed.node_name, bus_name=bus_name)} -> {placed_message_id} "
            f"[label={quote('TX')}];",
        ))
        for subscription in sorted(
            subscriptions[message.message_name],
            key=lambda item: item.node_name,
        ):
            edge_label = "RX callback" if subscription.callback else "RX"
            attributes = [f"label={quote(edge_label)}", "style=dashed"]
            if subscription.callback:
                attributes.append("penwidth=2")
            lines.append(
                f"\t{placed_message_id} -> {node_id(subscription.node_name, bus_name=bus_name)} "
                f"[{', '.join(attributes)}];"
            )
        lines.append("")

    if lines[-1] == "":
        lines.pop()
    lines.append("}")

    return can_graph_artifact(filename, lines)


def generate_message_flow_graphs(linked: LinkedCan) -> list[Artifact]:
    """Generate one communication-topology DOT artifact per configured bus."""
    return [
        _generate_message_flow_graph(linked, bus_name)
        for bus_name in sorted(linked.bus_configs)
    ]
