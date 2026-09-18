"""Message identity and RX resolution belong to linking, not export."""

from dataclasses import replace

import pytest

from canpiler.compiler import compile_message
from canpiler.linker import link_can
from canpiler.pipeline_models import CompiledCan, CompiledRxSubscription, CompiledTxMessage
from core.declarations import MessageDeclaration


def compiled_context():
    message = compile_message(MessageDeclaration(
        message_name="status", description="", priority=0, signals=[],
    ), False, {})
    return CompiledCan(
        nodes=(),
        tx_messages=(CompiledTxMessage("TX", "BUS_A", message),),
        rx_subscriptions=(CompiledRxSubscription("RX", "BUS_A", "status"),),
        bus_definitions={}, custom_types={},
    )


def test_cross_bus_ids_and_receivers():
    compiled = compiled_context()
    tx = compiled.tx_messages[0]
    compiled = replace(compiled, tx_messages=(tx, replace(tx, bus_name="BUS_B")))
    linked = link_can(compiled)
    assert [tx.message.final_id for tx in linked.tx_messages] == [1, 1]
    assert linked.subscriptions[0].resolved_message == linked.tx_messages[0].message


@pytest.mark.parametrize("different_node", [False, True])
def test_duplicate_transmitter(different_node):
    compiled = compiled_context()
    tx = compiled.tx_messages[0]
    duplicate = replace(tx, node_name="OTHER") if different_node else tx
    with pytest.raises(ValueError, match="duplicate TX|transmitted by both"):
        link_can(replace(compiled, tx_messages=(tx, duplicate)))


def test_duplicate_id_override():
    compiled = compiled_context()
    tx = compiled.tx_messages[0]
    first = replace(tx, message=replace(tx.message, id_override=12))
    second = replace(first, message=replace(first.message, message_name="other"))
    with pytest.raises(ValueError, match="duplicate ID override"):
        link_can(replace(compiled, tx_messages=(first, second)))


@pytest.mark.parametrize("node,message,error", [
    ("RX", "missing", "no transmitter"), ("TX", "status", "self-RX"),
])
def test_invalid_rx(node, message, error):
    compiled = compiled_context()
    rx = CompiledRxSubscription(node, "BUS_A", message)
    with pytest.raises(ValueError, match=error):
        link_can(replace(compiled, rx_subscriptions=(rx,)))
