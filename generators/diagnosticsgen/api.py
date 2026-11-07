from core.artifacts import Artifact
from core.contributions import DeclarationContribution, TxDeclaration
from core.declarations import CanDeclarations, MessageDeclaration, SignalDeclaration
from core.utils import get_jinja_env, render_template

def task_status_message(node_name: str) -> MessageDeclaration:
    return MessageDeclaration(
        message_name=f"{node_name.lower()}_diag_task",
        description="RTOS Task profiling status",
        priority=3,
        signals=[
            SignalDeclaration(
                signal_name="task_id",
                data_type="uint32_t",
            ),
            SignalDeclaration(
                signal_name="stack_hwm",
                data_type="uint16_t",
                description="Min free stack bytes",
            ),
            SignalDeclaration(
                signal_name="cpu_percent",
                data_type="uint8_t",
                description="CPU Usage Percentage, 255 is invalid",
            ),
            SignalDeclaration(
                signal_name="state",
                data_type="diagnostics_task_state_t",
                description="FreeRTOS task state",
            ),
        ],
    )
def board_summary_message(node_name: str) -> MessageDeclaration:
    return MessageDeclaration(
        message_name=f"{node_name.lower()}_diag_summary",
        description="Overall CPU and task status",
        priority=3,
        period_ms=1000,
        signals=[
            SignalDeclaration(
                signal_name="sample_count",
                data_type="uint32_t",
            ),
            SignalDeclaration(
                signal_name="task_count",
                data_type="uint16_t",
            ),
            SignalDeclaration(
                signal_name="cpu_percent",
                data_type="uint8_t",
                description="Whole number CPU pct. util.",
            ),
            SignalDeclaration(
                signal_name="capacity_exceeded",
                data_type="uint8_t",
                description="1 if task capacity exceeded",
            ),
        ], 
    )

def task_name_message(node_name: str) -> MessageDeclaration:
    return MessageDeclaration(
        message_name=f"{node_name.lower()}_diag_name",
        description="Task name character",
        priority=3,
        signals=[
            SignalDeclaration(
                signal_name="task_id",
                data_type="uint32_t",
            ),
            SignalDeclaration(
                signal_name="char_index",
                data_type="uint8_t",
            ),
            SignalDeclaration(
                signal_name="character",
                data_type="uint8_t",
            ),
        ],
    )


def diagnostics_targets(declarations: CanDeclarations) -> list[tuple[str, str]]:
    """Return each opted-in node and the bus its diagnostics messages go on."""
    targets = []
    for node in declarations.internal_nodes:
        bus_name = node.diagnostics_bus
        if bus_name is None:
            continue
        if bus_name not in node.busses:
            raise ValueError(
                f"Diagnostics node '{node.node_name}' sets diagnostics_bus "
                f"'{bus_name}' but is not attached to it; choose one of: "
                f"{', '.join(node.busses)}"
            )
        targets.append((node.node_name, bus_name))
    return targets


def diagnostics_contribution(declarations: CanDeclarations) -> DeclarationContribution:
    tx_messages = []
    for node_name, bus_name in diagnostics_targets(declarations):
        tx_messages.append(
            TxDeclaration(
                node_name=node_name,
                bus_name=bus_name,
                message=task_status_message(node_name),
            )
        )

        tx_messages.append(
            TxDeclaration(
                node_name=node_name,
                bus_name=bus_name,
                message=board_summary_message(node_name),
            )
        )

        tx_messages.append(
            TxDeclaration(
                node_name=node_name,
                bus_name=bus_name,
                message=task_name_message(node_name),
            )
        )
    return DeclarationContribution(
        tx_messages=tuple(tx_messages)
    )

def diagnostics_artifacts(declarations: CanDeclarations) -> list[Artifact]:
    env = get_jinja_env()
    node_names = [node_name for node_name, _ in diagnostics_targets(declarations)]
    return [
        Artifact(
            collection="generated",
            relative_path="diagnostics_can.h",
            content=render_template(env, "diagnostics_can.h.jinja", node_names=node_names),
        )
    ]
