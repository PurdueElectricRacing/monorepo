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
    load_bus_configs,
    load_custom_types,
    parse_all,
)


class Canpiler:
    def __init__(self, paths):
        self.paths = paths

    def parse(self) -> CanModel:
        return CanModel(
            nodes=parse_all(self.paths.config_dir),
            bus_configs=load_bus_configs(self.paths.config_dir),
            custom_types=load_custom_types(self.paths.config_dir),
        )

    def compile(self, model: CanModel, contributions) -> CompiledCan:
        self._apply_contributions(model, contributions)
        link_all(model.nodes)
        mappings = map_hardware(model.nodes, model.bus_configs)
        return CompiledCan(create_system_context(
            model.nodes, mappings, model.bus_configs, model.custom_types
        ))

    def generate(self, compiled: CompiledCan) -> list:
        context = compiled.context
        artifacts = generate_headers(context, self.paths.can_template_dir)
        artifacts.extend(generate_dbcs(context))
        calculate_bus_load(context)
        return artifacts

    @staticmethod
    def _apply_contributions(model: CanModel, contributions) -> None:
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
                    is_extended=model.bus_configs[item.bus_name].get("is_extended_id", False),
                    signals=[Signal(**signal) for signal in spec.get("signals", [])],
                )
                message.validate_semantics(model.custom_types)
                message.resolve_layout(model.custom_types)
                bus.tx_messages.append(message)

            for item in contribution.rx_subscriptions:
                bus = nodes[item.node_name].busses[item.bus_name]
                if item.message_name not in {rx.name for rx in bus.rx_messages}:
                    bus.rx_messages.append(RxMessage(item.message_name, item.callback))
