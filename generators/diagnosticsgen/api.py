from core.declarations import MessageDeclaration, SignalDeclaration
from core.contributions import DeclarationContribution, TxDeclaration

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
def diagnostics_contribution() -> DeclarationContribution:
    return DeclarationContribution(
        tx_messages=(
            TxDeclaration(
                node_name="G4_TESTING",
                bus_name="TEST_CAN",
                message=task_status_message("G4_TESTING"),
            ),
        ),
    )