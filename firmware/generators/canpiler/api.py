"""
api.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections.abc import Iterable

from core.artifacts import Artifact
from core.config_models import ConfigBundle
from core.contracts import CanContribution
from core.models import CanModel, CompiledCan
from .codegen import generate_headers
from .dbcgen import generate_dbcs
from .linker import link_all
from .load_calc import calculate_bus_load
from .mapper import map_hardware
from .parser import (
    Message,
    RxMessage,
    Signal,
    create_system_context,
    parse_all,
)


class Canpiler:
    def parse(self, config: ConfigBundle) -> CanModel:
        return CanModel(
            nodes=parse_all(config),
            bus_configs=config.buses.copy(),
            custom_types=config.custom_types.copy(),
        )

    def compile(
        self, model: CanModel, contributions: Iterable[CanContribution]
    ) -> CompiledCan:
        self._apply_contributions(model, contributions)
        link_all(model.nodes)
        mappings = map_hardware(model.nodes, model.bus_configs)
        return CompiledCan(create_system_context(
            model.nodes, mappings, model.bus_configs, model.custom_types
        ))

    def generate(self, compiled: CompiledCan) -> list[Artifact]:
        context = compiled.context
        artifacts = generate_headers(context)
        artifacts.extend(generate_dbcs(context))
        calculate_bus_load(context)
        return artifacts

    @staticmethod
    def _apply_contributions(
        model: CanModel, contributions: Iterable[CanContribution]
    ) -> None:
        nodes = {node.name: node for node in model.nodes}

        for contribution in contributions:
            model.custom_types.update(contribution.types)

            for item in contribution.tx_messages:
                node = nodes[item.node_name]
                bus = node.busses[item.bus_name]
                spec = item.message
                message = Message(
                    name=spec["name"],
                    desc=spec.get("desc", ""),
                    priority=spec.get("priority", 0),
                    period=spec.get("period", 0),
                    is_extended=model.bus_configs[item.bus_name].is_extended_id,
                    signals=[Signal(**signal) for signal in spec.get("signals", [])],
                )
                message.validate_semantics(model.custom_types)
                message.resolve_layout(model.custom_types)
                bus.tx_messages.append(message)

            for item in contribution.rx_subscriptions:
                bus = nodes[item.node_name].busses[item.bus_name]
                if item.message_name not in {rx.name for rx in bus.rx_messages}:
                    bus.rx_messages.append(RxMessage(item.message_name, item.callback))
