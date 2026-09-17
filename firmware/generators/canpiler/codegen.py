"""
codegen.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Mapping, Sequence

from jinja2 import Environment

from core.artifacts import Artifact
from core.config_models import BusDeclaration, CustomTypeDeclaration
from core.utils import get_jinja_env, print_as_ok, print_as_success, render_template
from .ir import CompiledNode, LinkedCan, LinkedMessage
from .mapper import HardwareMap, NodeHardwareMap
from .render_contexts import (
    build_node_header_context,
)


def generate_headers(
    linked: LinkedCan,
    hardware_map: HardwareMap,
    version: str,
) -> list[Artifact]:
    print("Generating headers...")
    env = get_jinja_env()
    artifacts = [generate_types_header(env, linked.custom_types)]

    bus_names = sorted({
        bus_name
        for node in linked.nodes
        for bus_name in node.busses
    })
    for bus_name in bus_names:
        messages = tuple(
            sorted(
                (
                    item.message
                    for item in linked.tx_messages
                    if item.bus_name == bus_name
                ),
                key=lambda message: (
                    message.final_id,
                    message.message_name,
                ),
            )
        )
        artifacts.append(
            generate_bus_header(
                env,
                bus_name,
                linked.bus_configs[bus_name],
                messages,
            )
        )

    artifacts.extend(
        generate_node_headers(
            env,
            linked,
            hardware_map,
            version,
        )
    )
    artifacts.append(generate_router_header(env, linked.nodes))

    print_as_success("Successfully generated C headers")

    return artifacts


def generate_types_header(
    env: Environment,
    custom_types: Mapping[str, CustomTypeDeclaration],
) -> Artifact:
    content = render_template(
        env,
        "can_types.h.jinja",
        types=custom_types.values(),
    )
    print_as_ok("Generated can_types.h")
    return Artifact("generated", "can_types.h", content)


def generate_router_header(
    env: Environment,
    nodes: Sequence[CompiledNode],
) -> Artifact:
    content = render_template(
        env,
        "can_router.h.jinja",
        nodes=nodes,
        router_nodes=[node for node in nodes if not node.is_external],
    )
    print_as_ok("Generated can_router.h")
    return Artifact("generated", "can_router.h", content)


def generate_node_headers(
    env: Environment,
    linked: LinkedCan,
    hardware_map: HardwareMap,
    version: str,
) -> list[Artifact]:
    return [
        generate_node_header(
            env,
            node,
            linked,
            hardware_map.get(node.name),
            version,
        )
        for node in linked.nodes
        if not node.is_external
    ]


def generate_node_header(
    env: Environment,
    node: CompiledNode,
    linked: LinkedCan,
    mapping: NodeHardwareMap | None,
    version: str,
) -> Artifact:
    filename = f"{node.name}.h"
    render_context = build_node_header_context(
        node,
        linked,
        mapping,
        version,
    )
    content = render_template(env, "node_header.h.jinja", ctx=render_context)
    print_as_ok(f"Generated {filename}")
    return Artifact("generated", filename, content)


def generate_bus_header(
    env: Environment,
    bus_name: str,
    declaration: BusDeclaration,
    messages: Sequence[LinkedMessage],
) -> Artifact:
    content = render_template(
        env,
        "bus_header.h.jinja",
        bus_name=bus_name,
        declaration=declaration,
        messages=messages,
    )
    print_as_ok(f"Generated {bus_name}.h")
    return Artifact("generated", f"{bus_name}.h", content)
