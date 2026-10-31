from core.declarations import MessageDeclaration, SignalDeclaration
from core.contributions import DeclarationContribution, TxDeclaration
from core.artifacts import Artifact

DIAGNOSTICS_NODES = (
    ("A_BOX", "VCAN"),
    ("DASHBOARD", "VCAN"),
    ("DRIVELINE", "VCAN"),
    ("MAIN_MODULE", "VCAN"),
    ("PDU", "VCAN"),
    ("TORQUE_VECTOR", "VCAN"),
    ("G4_TESTING", "TEST_CAN"),
)

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
                data_type="uint8_t",
                description="0: Running, 1: Ready, 2: Blocked, 3: Suspended, 4: Deleted, 5: Invalid",
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


def diagnostics_contribution() -> DeclarationContribution:
    tx_messages = []
    for node_name, bus_name in DIAGNOSTICS_NODES:
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

def diagnostics_artifacts() -> list[Artifact]:
    lines = [
        "#ifndef DIAGNOSTICS_CAN_H",
        "#define DIAGNOSTICS_CAN_H",
        "",
        '#include "can_library/generated/can_router.h"',
        "",
    ]

    for index, (node_name, _) in enumerate(DIAGNOSTICS_NODES):
        directive = "#if" if index == 0 else '#elif'

        lines.append(f"{directive} defined(CAN_NODE_{node_name})")
        lines.append(
            f"#define DIAGNOSTICS_SEND_TASK "
            f"CAN_SEND_{node_name.lower()}_diag_task"
        )

        lines.append(
            f"#define DIAGNOSTICS_SEND_SUMMARY "
            f"CAN_SEND_{node_name.lower()}_diag_summary"
        )

        lines.append(
            f"#define DIAGNOSTICS_SEND_NAME "
            f"CAN_SEND_{node_name.lower()}_diag_name"
        )

    lines.extend([
        "#else",
        '#error "Diagnostics CAN is not configured for this node"',
        "#endif",
        "",
        "#endif // DIAGNOSTICS_CAN_H",
        "",
    ])
    return [Artifact(collection="generated", relative_path="diagnostics_can.h",content="\n".join(lines),)]