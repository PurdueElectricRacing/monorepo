"""
api.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from core.artifacts import Artifact
from core.config_models import (
    CanDeclarations,
    CustomTypeDeclaration,
    FaultDeclaration,
    MessageDeclaration,
    RxSubscriptionDeclaration,
    SignalDeclaration,
)
from core.contracts import (
    DeclarationContribution,
    CustomTypeContribution,
    RxDeclaration,
    TxDeclaration,
)
from core.utils import get_jinja_env, print_as_ok, print_as_success, render_template
from .models import Fault, FaultNode, FaultPlan


class FaultGenerator:
    def plan(self, declarations: CanDeclarations) -> FaultPlan:
        fault_bus = next(
            (
                bus.name
                for bus in declarations.buses.values()
                if bus.host_fault_library
            ),
            None,
        )
        fault_id = declarations.custom_types.get("fault_id_t")

        nodes = []
        for data in declarations.internal_nodes:
            tx_messages = (
                data.busses[fault_bus].tx
                if fault_bus in data.busses
                else []
            )

            nodes.append(
                FaultNode(
                    name=data.node_name,
                    enabled=data.fault_library_enabled,
                    generate_strings=data.generate_fault_messages,
                    busses=frozenset(data.busses),
                    tx_message_names=frozenset(
                        message.message_name
                        for message in tx_messages
                    ),
                    faults=tuple(
                        self._parse_fault(item)
                        for item in data.faults
                    ),
                )
            )

        plan = FaultPlan(
            tuple(nodes),
            fault_bus,
            fault_id.base_type if fault_id is not None else "uint16_t",
        )
        self._validate(plan)
        return plan

    def contribute(self, plan: FaultPlan) -> DeclarationContribution:
        if not plan.modules:
            return DeclarationContribution()

        choices = [
            fault.name for module in plan.modules for fault in module.faults
        ]

        tx_messages = []
        rx_subscriptions = []
        event_names = []
        sync_names = []

        for module in plan.modules:
            event_name = f"{module.name.lower()}_fault_event"
            sync_name = f"{module.name.lower()}_fault_sync"
            event_names.append(event_name)
            sync_names.append(sync_name)
            tx_messages.extend(
                (
                    TxDeclaration(
                        module.name,
                        plan.fault_bus_name,
                        MessageDeclaration(
                            message_name=event_name,
                            description=(
                                "Immediate fault event signal for "
                                f"{module.name}"
                            ),
                            priority=0,
                            signals=[
                                SignalDeclaration(
                                    signal_name="idx",
                                    data_type="fault_id_t",
                                    description="Global Fault Index",
                                ),
                                SignalDeclaration(
                                    signal_name="val",
                                    data_type="uint16_t",
                                    description="Trigger Value",
                                ),
                                SignalDeclaration(
                                    signal_name="state",
                                    data_type="bool",
                                    description=(
                                        "Latch State "
                                        "(0=unlatched, 1=latched)"
                                    ),
                                ),
                            ],
                        ),
                    ),
                    TxDeclaration(
                        module.name,
                        plan.fault_bus_name,
                        MessageDeclaration(
                            message_name=sync_name,
                            description=(
                                "Periodic fault synchronization for "
                                f"{module.name}"
                            ),
                            priority=1,
                            period_ms=100,
                            signals=[
                                SignalDeclaration(
                                    signal_name=fault.name,
                                    data_type="bool",
                                    length=1,
                                )
                                for fault in module.faults
                            ],
                        ),
                    ),
                )
            )

        for node in plan.nodes:
            if not node.enabled or plan.fault_bus_name not in node.busses:
                continue

            own = {f"{node.name.lower()}_fault_event", f"{node.name.lower()}_fault_sync"}

            for message_name in event_names + sync_names:
                if message_name not in own:
                    rx_subscriptions.append(
                        RxDeclaration(
                            node.name,
                            plan.fault_bus_name,
                            RxSubscriptionDeclaration(
                                message_name=message_name,
                                callback=True,
                            ),
                        )
                    )

        return DeclarationContribution(
            custom_types=(
                CustomTypeContribution(
                    CustomTypeDeclaration(
                        name="fault_id_t",
                        choices=choices,
                        base_type=plan.fault_id_base_type,
                    ),
                    mode="replace",
                ),
            ),
            tx_messages=tuple(tx_messages),
            rx_subscriptions=tuple(rx_subscriptions),
        )

    def generate(
        self,
        plan: FaultPlan,
        version: str,
    ) -> list[Artifact]:
        if not plan.modules:
            return []

        env = get_jinja_env()
        context = self._render_context(plan, version)

        print("Generating fault library implementation data...")
        artifacts = [
            Artifact(
                "generated",
                "fault_data.h",
                render_template(
                    env,
                    "fault_data.h.jinja",
                    **context,
                ),
            ),
            Artifact(
                "generated",
                "fault_data.c",
                render_template(
                    env,
                    "fault_data.c.jinja",
                    **context,
                ),
            ),
        ]

        print_as_ok("Generated fault_data.h")
        print_as_ok("Generated fault_data.c")
        print_as_success("Fault library implementation files generated")
        return artifacts

    @staticmethod
    def _parse_fault(data: FaultDeclaration) -> Fault:
        return Fault(
            name=data.fault_name,
            max_val=data.max,
            min_val=data.min,
            priority=data.priority,
            time_to_latch=data.time_to_latch,
            time_to_unlatch=data.time_to_unlatch,
            lcd_message=data.lcd_message,
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
    def _render_context(
        plan: FaultPlan,
        version: str,
    ) -> dict[str, object]:
        owner_nodes = [
            {
                "name_upper": node.name.upper(),
                "has_faults": bool(node.faults),
                "start_fault_name_upper": (
                    node.faults[0].name.upper()
                    if node.faults
                    else None
                ),
                "end_fault_name_upper": (
                    node.faults[-1].name.upper()
                    if node.faults
                    else None
                ),
                "generate_fault_strings": node.generate_strings,
            }
            for node in plan.nodes
            if node.enabled
        ]

        modules = []
        for module in plan.modules:
            rows = [
                {
                    "name": fault.name,
                    "name_upper": fault.name.upper(),
                    "max_val": fault.max_val,
                    "min_val": fault.min_val,
                    "latch_time": fault.time_to_latch,
                    "unlatch_time": fault.time_to_unlatch,
                    "priority_upper": fault.priority.upper(),
                    "lcd_message": fault.lcd_message,
                }
                for fault in module.faults
            ]
            modules.append(
                {
                    "node_name": module.name,
                    "faults": rows,
                    "first_fault": rows[0],
                    "last_fault": rows[-1],
                }
            )

        return {
            "owner_nodes": owner_nodes,
            "fault_modules": modules,
            "total_faults": sum(len(module.faults) for module in plan.modules),
            "version": version,
        }
