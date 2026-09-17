"""
test_compiler.py

Author: Irving Wang (irvingw@purdue.edu)
"""

import pytest

from canpiler.compiler import compile_message
from core.declarations import (
    MessageDeclaration,
    SignalDeclaration,
)


def test_compiles_message() -> None:
    message = MessageDeclaration(
        message_name="good_message",
        description="valid test message",
        priority=1,
        signals=[
            SignalDeclaration(
                signal_name="state",
                data_type="uint8_t",
            ),
            SignalDeclaration(
                signal_name="value",
                data_type="uint16_t",
            ),
        ],
    )
    compiled_message = compile_message(
        declaration=message,
        is_extended=False,
        custom_types={},
    )

    assert compiled_message.dlc == 3
    assert [signal.signal_name for signal in compiled_message.signals] == [
        "state",
        "value",
    ]
    assert [signal.data_type for signal in compiled_message.signals] == [
        "uint8_t",
        "uint16_t",
    ]
    assert [signal.length for signal in compiled_message.signals] == [8, 16]
    assert [signal.bit_offset for signal in compiled_message.signals] == [0, 8]


def test_oversized_message() -> None:
    message = MessageDeclaration(
        message_name="big_message",
        description="should fail with oversized message",
        priority=1,
        signals=[
            SignalDeclaration(
                signal_name="state",
                data_type="uint64_t",
            ),
            SignalDeclaration(
                signal_name="value",
                data_type="uint32_t",
            ),
        ],
    )

    with pytest.raises(ValueError):
        compile_message(
            declaration=message,
            is_extended=False,
            custom_types={},
        )

def test_unknown_signal_type() -> None:
    message = MessageDeclaration(
        message_name="bad_type_message",
        description="contains an unknown signal type",
        priority=1,
        signals=[
            SignalDeclaration(
                signal_name="value",
                data_type="uint24_t",
            ),
        ],
    )

    with pytest.raises(ValueError):
        compile_message(
            declaration=message,
            is_extended=False,
            custom_types={},
        )

def test_override_out_of_range() -> None:
    message = MessageDeclaration(
        message_name="bad_id_message",
        description="uses an invalid standard CAN ID",
        priority=1,
        id_override="0x800",
        signals=[],
    )

    with pytest.raises(ValueError):
        compile_message(
            declaration=message,
            is_extended=False,
            custom_types={},
        )