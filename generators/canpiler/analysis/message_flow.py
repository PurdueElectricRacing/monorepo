"""Generate Graphviz communication-topology graphs from linked CAN data."""

from __future__ import annotations

from collections import defaultdict

from core.artifacts import Artifact
from .dot import graph_header, node_line, quote, topology_artifact
from ..pipeline.models import LinkedCan, LinkedMessage


def _node_id(bus_name: str, node_name: str) -> str:
    return quote(f"node:{bus_name}:{node_name}")


def _message_id(bus_name: str, message_name: str) -> str:
    return quote(f"message:{bus_name}:{message_name}")


def _message_label(message: LinkedMessage) -> str:
    period = f"{message.period_ms} ms" if message.period_ms > 0 else "aperiodic"
    frame_type = "extended" if message.is_extended else "standard"
    return "\n".join((
        message.message_name,
        f"ID 0x{message.final_id:X} ({frame_type})",
        f"{period} · {message.dlc} bytes · priority {message.priority}",
    ))


def _generate_message_flow_graph(linked: LinkedCan, bus_name: str) -> Artifact:
    config = linked.bus_configs[bus_name]
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

    title = f"{bus_name} — {config.baud_rate // 1000} kbit/s"
    lines = graph_header("CAN_topology", title, directed=True)

    for node in nodes:
        lines.append(node_line(
            _node_id(bus_name, node.name), node.name, is_external=node.is_external
        ))

    if nodes and messages:
        lines.append("")

    for placed in messages:
        message = placed.message
        message_id = _message_id(bus_name, message.message_name)
        lines.extend((
            f"    {message_id} [label={quote(_message_label(message))}, "
            f"style={quote('rounded,filled')}, fillcolor={quote('#E0E0E0')}];",
            f"    {_node_id(bus_name, placed.node_name)} -> {message_id} [label={quote('TX')}];",
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
                f"    {message_id} -> {_node_id(bus_name, subscription.node_name)} "
                f"[{', '.join(attributes)}];"
            )
        lines.append("")

    if lines[-1] == "":
        lines.pop()
    lines.append("}")

    filename = f"message_flow_{bus_name}.dot"
    return topology_artifact(filename, lines)


def generate_message_flow_graphs(linked: LinkedCan) -> list[Artifact]:
    """Generate one communication-topology DOT artifact per configured bus."""
    return [
        _generate_message_flow_graph(linked, bus_name)
        for bus_name in sorted(linked.bus_configs)
    ]
