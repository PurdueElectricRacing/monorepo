"""
codegen.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Mapping, Sequence

from jinja2 import Environment

from core.artifacts import Artifact
from core.config_models import BusConfig, CustomTypeDeclaration
from core.utils import get_jinja_env, print_as_ok, print_as_success, render_template
from .ir import BuildMetadata, LinkedCan, LinkedMessage
from .mapper import NodeMapping
from .render_contexts import (
    build_can_render_context,
    build_node_header_context,
    build_type_render_views,
)
from .render_models import CanRenderContext, NodeRenderView


def generate_headers(
    linked: LinkedCan,
    mappings: Mapping[str, NodeMapping],
    metadata: BuildMetadata,
) -> list[Artifact]:
    print("Generating headers...")
    context = build_can_render_context(linked, mappings, metadata)
    env = get_jinja_env()
    artifacts = [generate_types_header(env, linked.custom_types)]

    for bus_name, view in context.busses.items():
        artifacts.append(
            generate_bus_header(
                env,
                bus_name,
                linked.bus_configs[bus_name],
                view.messages,
            )
        )

    artifacts.extend(generate_node_headers(env, context))
    artifacts.append(generate_router_header(env, context.nodes))

    print_as_success("Successfully generated C headers")

    return artifacts


def generate_types_header(
    env: Environment,
    custom_types: Mapping[str, CustomTypeDeclaration],
) -> Artifact:
    content = render_template(
        env,
        "can_types.h.jinja",
        types=build_type_render_views(custom_types),
    )
    print_as_ok("Generated can_types.h")
    return Artifact("generated", "can_types.h", content)


def generate_router_header(
    env: Environment,
    nodes: Sequence[NodeRenderView],
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
    context: CanRenderContext,
) -> list[Artifact]:
    return [
        generate_node_header(env, node, context)
        for node in context.nodes if not node.is_external
    ]


def generate_node_header(
    env: Environment,
    node: NodeRenderView,
    context: CanRenderContext,
) -> Artifact:
    filename = f"{node.name}.h"
    render_context = build_node_header_context(node, context)
    content = render_template(env, "node_header.h.jinja", ctx=render_context)
    print_as_ok(f"Generated {filename}")
    return Artifact("generated", filename, content)


def generate_bus_header(
    env: Environment,
    bus_name: str,
    config: BusConfig,
    messages: Sequence[LinkedMessage],
) -> Artifact:
    content = render_template(
        env,
        "bus_header.h.jinja",
        bus_name=bus_name,
        config=config,
        messages=messages,
    )
    print_as_ok(f"Generated {bus_name}.h")
    return Artifact("generated", f"{bus_name}.h", content)
