"""
test_compiler.py

Author: Irving Wang (irvingw@purdue.edu)
"""

import pytest

from canpiler.pipeline.compiler import compile_message
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

@pytest.mark.parametrize("base_type", ["float", "uint8_t", "int8_t"])
@pytest.mark.parametrize("custom", [False, True])
def test_resolved_signal_semantics(base_type, custom):
    from core.declarations import CustomTypeDeclaration

    types = {"test_t": CustomTypeDeclaration(name="test_t", base_type=base_type)} if custom else {}
    data_type = "test_t" if custom else base_type
    bad_length = 16 if base_type == "float" else 2
    # Unsigned two-bit enums allow four values, signed allow two nonnegative values.
    choices = [] if base_type == "float" else [str(i) for i in range(5)]
    message = MessageDeclaration(
        message_name="invalid", description="", priority=0,
        signals=[SignalDeclaration(
            signal_name="value", data_type=data_type, length=bad_length, choices=choices,
        )],
    )
    with pytest.raises(ValueError, match="Signal 'value'.*message 'invalid'"):
        compile_message(message, False, types)


@pytest.mark.parametrize("base_type,length,labels", [
    ("uint8_t", 2, 4), ("int8_t", 2, 2), ("bool", 1, 2),
])
def test_enum_capacity_boundary(base_type, length, labels):
    from core.declarations import CustomTypeDeclaration

    types = {"test_t": CustomTypeDeclaration(
        name="test_t", base_type=base_type, choices=[str(i) for i in range(labels)],
    )}
    def message(choices=None):
        kwargs = {} if choices is None else {"choices": choices}
        return MessageDeclaration(
            message_name="test", description="", priority=0,
            signals=[SignalDeclaration(signal_name="value", data_type="test_t", length=length, **kwargs)],
        )
    compile_message(message(), False, types)
    types["test_t"] = CustomTypeDeclaration(
        name="test_t", base_type=base_type, choices=[str(i) for i in range(labels + 1)],
    )
    with pytest.raises(ValueError, match="enum values do not fit"):
        compile_message(message(), False, types)
    # Explicit choices take precedence over an oversized custom-type enum.
    compile_message(message(["override"]), False, types)
    # Empty signal choices retain the existing custom-type fallback.
    with pytest.raises(ValueError, match="enum values do not fit"):
        compile_message(message([]), False, types)


@pytest.mark.parametrize("custom", [False, True])
def test_float_enum_rejected(custom):
    from core.declarations import CustomTypeDeclaration

    types = {"test_t": CustomTypeDeclaration(
        name="test_t", base_type="float", choices=["invalid"],
    )} if custom else {}
    kwargs = {} if custom else {"choices": ["invalid"]}
    message = MessageDeclaration(
        message_name="test", description="", priority=0,
        signals=[SignalDeclaration(
            signal_name="value", data_type="test_t" if custom else "float", **kwargs,
        )],
    )
    with pytest.raises(ValueError, match="float requires 32 bits and no choices"):
        compile_message(message, False, types)
