from typing import Any, Mapping

from core.artifacts import Artifact
from core.config import CONFIG_DIR, FAULT_TEMPLATE_DIR
from core.contracts import CanContribution, RxSubscriptionContribution, TxMessageContribution
from core.models import CompiledCan
from core.utils import get_jinja_env, load_json, print_as_ok, print_as_success, render_template
from .models import Fault, FaultNode, FaultPlan


class FaultGenerator:
    def plan(self) -> FaultPlan:
        buses = load_json(CONFIG_DIR / "system" / "bus_configs.json")["busses"]
        fault_bus = next((bus["name"] for bus in buses if bus.get("host_fault_library")), None)
        types = load_json(CONFIG_DIR / "system" / "common_types.json").get("types", [])
        fault_id = next((item for item in types if item["name"] == "fault_id_t"), {})

        nodes = []
        for path in sorted((CONFIG_DIR / "nodes").glob("*.json")):
            data = load_json(path)
            nodes.append(FaultNode(
                name=data["node_name"],
                enabled=data["fault_library_enabled"],
                generate_strings=data.get("generate_fault_messages", False),
                busses=set(data.get("busses", {})),
                tx_message_names={
                    message["msg_name"]
                    for message in data.get("busses", {}).get(fault_bus, {}).get("tx", [])
                },
                faults=[self._parse_fault(item) for item in data.get("faults", [])],
            ))

        plan = FaultPlan(nodes, fault_bus, fault_id.get("base_type", "uint16_t"))
        self._validate(plan)
        return plan

    def contribute(self, plan: FaultPlan) -> CanContribution:
        if not plan.modules:
            return CanContribution()

        choices = []
        for index, fault in enumerate(fault for module in plan.modules for fault in module.faults):
            fault.absolute_index = index
            choices.append(fault.name)

        contribution = CanContribution(types={"fault_id_t": {
            "name": "fault_id_t",
            "choices": choices,
            "base_type": plan.fault_id_base_type,
        }})
        event_names = []
        sync_names = []

        for module in plan.modules:
            event_name = f"{module.name.lower()}_fault_event"
            sync_name = f"{module.name.lower()}_fault_sync"
            event_names.append(event_name)
            sync_names.append(sync_name)
            contribution.tx_messages.extend((
                TxMessageContribution(module.name, plan.fault_bus_name, {
                    "name": event_name,
                    "desc": f"Immediate fault event signal for {module.name}",
                    "priority": 0,
                    "signals": [
                        {"name": "idx", "datatype": "fault_id_t", "desc": "Global Fault Index"},
                        {"name": "val", "datatype": "uint16_t", "desc": "Trigger Value"},
                        {"name": "state", "datatype": "bool", "desc": "Latch State (0=unlatched, 1=latched)"},
                    ],
                }),
                TxMessageContribution(module.name, plan.fault_bus_name, {
                    "name": sync_name,
                    "desc": f"Periodic fault synchronization for {module.name}",
                    "priority": 1,
                    "period": 100,
                    "signals": [
                        {"name": fault.name, "datatype": "bool", "length": 1}
                        for fault in module.faults
                    ],
                }),
            ))

        for node in plan.nodes:
            if not node.enabled or plan.fault_bus_name not in node.busses:
                continue
            own = {f"{node.name.lower()}_fault_event", f"{node.name.lower()}_fault_sync"}
            for message_name in event_names + sync_names:
                if message_name not in own:
                    contribution.rx_subscriptions.append(RxSubscriptionContribution(
                        node.name, plan.fault_bus_name, message_name
                    ))
        return contribution

    def generate(self, plan: FaultPlan, compiled_can: CompiledCan) -> list[Artifact]:
        if not plan.modules:
            return []
        env = get_jinja_env(FAULT_TEMPLATE_DIR)
        context = self._render_context(plan, compiled_can.context.version)
        print("Generating fault library implementation data...")
        artifacts = [
            Artifact("generated", "fault_data.h", render_template(env, "fault_data.h.jinja", **context)),
            Artifact("generated", "fault_data.c", render_template(env, "fault_data.c.jinja", **context)),
        ]
        print_as_ok("Generated fault_data.h")
        print_as_ok("Generated fault_data.c")
        print_as_success("Fault library implementation files generated")
        return artifacts

    @staticmethod
    def _parse_fault(data: Mapping[str, Any]) -> Fault:
        return Fault(
            name=data["fault_name"], max_val=data["max"], min_val=data["min"],
            priority=data["priority"], time_to_latch=data["time_to_latch"],
            time_to_unlatch=data["time_to_unlatch"], lcd_message=data["lcd_message"],
        )

    @staticmethod
    def _validate(plan: FaultPlan) -> None:
        names = set()
        if plan.modules and not plan.fault_bus_name:
            raise ValueError("Missing host_fault_library configuration")
        for module in plan.modules:
            if len(module.faults) > 64:
                raise ValueError(f"Node '{module.name}' exceeds the 64-fault limit")
            if plan.fault_bus_name not in module.busses:
                raise ValueError(f"Node '{module.name}' is missing fault bus '{plan.fault_bus_name}'")
            generated = {
                f"{module.name.lower()}_fault_event",
                f"{module.name.lower()}_fault_sync",
            }
            if generated & {name.lower() for name in module.tx_message_names}:
                raise ValueError(f"Node '{module.name}' shadows a generated fault message")
            for fault in module.faults:
                upper = fault.name.upper()
                if upper in names:
                    raise ValueError(f"Global fault name collision: {upper}")
                if fault.min_val >= fault.max_val:
                    raise ValueError(f"Invalid limits for fault '{fault.name}'")
                names.add(upper)

    @staticmethod
    def _render_context(plan: FaultPlan, version: str) -> dict[str, object]:
        owner_nodes = [{
            "name_upper": node.name.upper(),
            "has_faults": bool(node.faults),
            "start_fault_name_upper": node.faults[0].name.upper() if node.faults else None,
            "end_fault_name_upper": node.faults[-1].name.upper() if node.faults else None,
            "generate_fault_strings": node.generate_strings,
        } for node in plan.nodes if node.enabled]

        modules = []
        for module in plan.modules:
            rows = [{
                "name": fault.name,
                "name_upper": fault.name.upper(),
                "max_val": fault.max_val,
                "min_val": fault.min_val,
                "latch_time": fault.time_to_latch,
                "unlatch_time": fault.time_to_unlatch,
                "priority_upper": fault.priority.upper(),
                "lcd_message": fault.lcd_message,
            } for fault in module.faults]
            modules.append({
                "node_name": module.name,
                "faults": rows,
                "first_fault": rows[0],
                "last_fault": rows[-1],
            })

        return {
            "owner_nodes": owner_nodes,
            "fault_modules": modules,
            "total_faults": sum(len(module.faults) for module in plan.modules),
            "version": version,
        }
