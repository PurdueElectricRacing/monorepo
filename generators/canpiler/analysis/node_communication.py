"""Generate a Graphviz graph of message exchange between CAN nodes."""

from __future__ import annotations

from collections import defaultdict

from core.artifacts import Artifact
from .graphviz_helpers import can_graph_artifact, graph_header, node_id, node_line, quote
from ..pipeline.models import LinkedCan


def generate_node_communication_graph(linked: LinkedCan) -> Artifact:
    """Connect transmitters to subscribers, labeling each edge by CAN bus."""
    transmitters = {
        (placed.bus_name, placed.message.message_name): placed.node_name
        for placed in linked.tx_messages
    }
    buses_by_pair: dict[tuple[str, str], set[str]] = defaultdict(set)
    for subscription in linked.subscriptions:
        sender = transmitters[(subscription.bus_name, subscription.message_name)]
        buses_by_pair[(sender, subscription.node_name)].add(subscription.bus_name)

    lines = graph_header(
        "CAN_node_communication_topology", "CAN node communication", directed=True
    )

    for node in sorted(linked.nodes, key=lambda item: item.name):
        lines.append(node_line(node_id(node.name), node.name, is_external=node.is_external))

    node_pairs = sorted({
        tuple(sorted((sender, receiver)))
        for sender, receiver in buses_by_pair
    })
    if node_pairs:
        lines.append("")
    for first, second in node_pairs:
        forward_buses = buses_by_pair[(first, second)]
        reverse_buses = buses_by_pair[(second, first)]
        shared_buses = forward_buses & reverse_buses
        edges = (
            (first, second, shared_buses, True),
            (first, second, forward_buses - shared_buses, False),
            (second, first, reverse_buses - shared_buses, False),
        )
        for sender, receiver, buses, bidirectional in edges:
            if not buses:
                continue
            attributes = [f"label={quote(', '.join(sorted(buses)))}"]
            if bidirectional:
                attributes.append("dir=both")
            lines.append(
                f"\t{node_id(sender)} -> {node_id(receiver)} "
                f"[{', '.join(attributes)}];"
            )

    lines.append("}")
    filename = "node_communication.dot"
    return can_graph_artifact(filename, lines)
