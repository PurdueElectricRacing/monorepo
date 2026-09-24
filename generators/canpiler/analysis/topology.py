"""Generate Graphviz communication-topology graphs from linked CAN data."""

from __future__ import annotations

import json
from collections import defaultdict

from core.artifacts import Artifact
from core.utils import print_as_ok
from ..pipeline.models import LinkedCan, LinkedMessage


def _quote(value: str) -> str:
    """Return a Graphviz-compatible quoted string."""
    return json.dumps(value, ensure_ascii=False)


def _node_id(bus_name: str, node_name: str) -> str:
    return _quote(f"node:{bus_name}:{node_name}")


def _message_id(bus_name: str, message_name: str) -> str:
    return _quote(f"message:{bus_name}:{message_name}")


def _message_label(message: LinkedMessage) -> str:
    period = f"{message.period_ms} ms" if message.period_ms > 0 else "aperiodic"
    frame_type = "extended" if message.is_extended else "standard"
    return "\n".join((
        message.message_name,
        f"ID 0x{message.final_id:X} ({frame_type})",
        f"{period} · {message.dlc} bytes · priority {message.priority}",
    ))


def _generate_bus_graph(linked: LinkedCan, bus_name: str) -> Artifact:
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
    lines = [
        "digraph CAN_topology {",
        "    rankdir=LR;",
        "    graph ["
        f"fontname={_quote('Helvetica')}, "
        f"labelloc={_quote('t')}, "
        f"bgcolor={_quote('transparent')}, "
        f"fontcolor={_quote('#222222')}, "
        f"label={_quote(title)}"
        "];",
        "    node ["
        f"fontname={_quote('Helvetica')}, "
        "shape=box, style=filled, "
        f"fillcolor={_quote('#D5D5D5')}, "
        f"color={_quote('#777777')}, "
        f"fontcolor={_quote('#111111')}"
        "];",
        "    edge ["
        f"fontname={_quote('Helvetica')}, "
        f"color={_quote('#2F70AD')}, "
        f"fontcolor={_quote('#333333')}"
        "];",
        "",
    ]

    for node in nodes:
        attributes = [
            f"label={_quote(node.name)}",
        ]
        if node.is_external:
            attributes.append(f"style={_quote('filled,dashed')}")
        lines.append(f"    {_node_id(bus_name, node.name)} [{', '.join(attributes)}];")

    if nodes and messages:
        lines.append("")

    for placed in messages:
        message = placed.message
        message_id = _message_id(bus_name, message.message_name)
        lines.extend((
            f"    {message_id} [label={_quote(_message_label(message))}, "
            f"style={_quote('rounded,filled')}, fillcolor={_quote('#E0E0E0')}];",
            f"    {_node_id(bus_name, placed.node_name)} -> {message_id} [label={_quote('TX')}];",
        ))
        for subscription in sorted(
            subscriptions[message.message_name],
            key=lambda item: item.node_name,
        ):
            edge_label = "RX callback" if subscription.callback else "RX"
            attributes = [f"label={_quote(edge_label)}", "style=dashed"]
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

    filename = f"topology_{bus_name}.dot"
    print_as_ok(f"Generated {filename}")
    return Artifact("dbc", filename, "\n".join(lines) + "\n")


def generate_topology_graphs(linked: LinkedCan) -> list[Artifact]:
    """Generate one communication-topology DOT artifact per configured bus."""
    return [
        _generate_bus_graph(linked, bus_name)
        for bus_name in sorted(linked.bus_configs)
    ]
