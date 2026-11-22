"""
test_compiler.py

Author: Irving Wang (irvingw@purdue.edu)
"""

import json
from pathlib import Path

from pydantic import ValidationError
import pytest

from canpiler.pipeline.compiler import assemble_source, compile_message
from core.declaration_loader import DeclarationValidationError, load_declarations
from core.declarations import (
    BusAttachmentDeclaration,
    BusDeclaration,
    CanDeclarations,
    CommonMessageDeclaration,
    CommonMessageDeclarations,
    CommonMessageReference,
    CustomTypeDeclaration,
    ExternalBusDeclaration,
    ExternalNodeDeclaration,
    MessageDeclaration,
    NodeDeclaration,
    SignalDeclaration,
)


def test_reserved_enum_suffix_rejected() -> None:
    message = MessageDeclaration(
        message_name="reserved_suffix_message",
        description="signal name uses reserved suffix",
        priority=1,
        signals=[SignalDeclaration(signal_name="mode_enum", data_type="uint8_t")],
    )

    with pytest.raises(ValueError, match="_enum"):
        compile_message(declaration=message, is_extended=False, custom_types={})


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
    ("uint8_t", 2, 4), ("int8_t", 2, 2),
])
def test_enum_capacity_boundary(base_type, length, labels):
    def custom(count):
        return CustomTypeDeclaration(name="test_t", base_type=base_type,
                                     choices=[{"name": str(i), "value": i} for i in range(count)])
    def message(choices=None):
        kwargs = {} if choices is None else {"choices": choices}
        return MessageDeclaration(
            message_name="test", description="", priority=0,
            signals=[SignalDeclaration(signal_name="value", data_type="test_t", length=length, **kwargs)],
        )
    compile_message(message(), False, {"test_t": custom(labels)})
    types = {"test_t": custom(labels + 1)}
    with pytest.raises(ValueError, match="enum values do not fit"):
        compile_message(message(), False, types)
    with pytest.raises(ValueError, match="cannot override"):
        compile_message(message(["override"]), False, types)
    with pytest.raises(ValueError, match="enum values do not fit"):
        compile_message(message([]), False, types)


def test_enum_limits_inferred():
    types = {"test_t": CustomTypeDeclaration(name="test_t", base_type="uint8_t",
                                             choices=[{"name": "A", "value": 2}, {"name": "B", "value": 7}])}
    message = MessageDeclaration(
        message_name="test", description="", priority=0,
        signals=[
            SignalDeclaration(signal_name="custom", data_type="test_t"),
            SignalDeclaration(signal_name="inline", data_type="uint8_t", choices=["A", "B", "C"]),
            SignalDeclaration(signal_name="explicit", data_type="uint8_t", choices=["A", "B"], min=0, max=5),
            SignalDeclaration(signal_name="plain", data_type="uint8_t"),
        ],
    )
    limits = [(signal.min, signal.max) for signal in compile_message(message, False, types).signals]
    assert limits == [(2, 7), (0, 2), (0, 5), (None, None)]


def test_float_enum_rejected():
    with pytest.raises(ValueError, match="float cannot have enum choices"):
        CustomTypeDeclaration(name="test_t", base_type="float",
                              choices=[{"name": "invalid", "value": 0}])
    message = MessageDeclaration(
        message_name="test", description="", priority=0,
        signals=[SignalDeclaration(signal_name="value", data_type="float", choices=["invalid"])],
    )
    with pytest.raises(ValueError, match="float requires 32 bits and no choices"):
        compile_message(message, False, {})


@pytest.mark.parametrize("byte_order,offsets,orders", [
    ("little_endian", [0, 8, 24], ["little_endian"] * 3),
    ("big_endian", [0, 15, 24], ["little_endian", "big_endian", "little_endian"]),
])
def test_mixed_width_bit_placement(byte_order, offsets, orders):
    message = MessageDeclaration(
        message_name="mixed", description="", priority=0, byte_order=byte_order,
        signals=[
            SignalDeclaration(signal_name="first", data_type="uint8_t"),
            SignalDeclaration(signal_name="middle", data_type="int16_t"),
            SignalDeclaration(signal_name="last", data_type="uint8_t"),
        ],
    )
    compiled = compile_message(message, False, {})
    assert compiled.dlc == 4
    assert [signal.bit_shift for signal in compiled.signals] == [0, 8, 24]
    assert [signal.bit_offset for signal in compiled.signals] == offsets
    assert [signal.byte_order for signal in compiled.signals] == orders
    assert [signal.mask for signal in compiled.signals] == [0xFF, 0xFFFF, 0xFF]
    assert [signal.is_signed for signal in compiled.signals] == [False, True, False]


@pytest.mark.parametrize("prefix_bits,value_bits", [(1, 16), (0, 24)])
def test_big_endian_invalid_placement(prefix_bits, value_bits):
    signals = [SignalDeclaration(signal_name="value", data_type="uint32_t", length=value_bits)]
    if prefix_bits:
        signals.insert(0, SignalDeclaration(signal_name="prefix", data_type="bool"))
    message = MessageDeclaration(
        message_name="bad_layout", description="", priority=0,
        byte_order="big_endian", signals=signals,
    )
    with pytest.raises(ValueError, match="Signal 'value'.*must be byte-aligned"):
        compile_message(message, False, {})


def test_common_message_declarations_reject_duplicates():
    template = CommonMessageDeclaration(
        name="heartbeat_msg",
        description="Shared heartbeat",
        priority=4,
        period_ms=1000,
        signals=[SignalDeclaration(signal_name="counter", data_type="uint32_t")],
    )
    with pytest.raises(ValidationError, match="duplicate common message name"):
        CommonMessageDeclarations(messages=[template, template])

    with pytest.raises(ValidationError, match="duplicate signal name"):
        CommonMessageDeclaration(
            name="dup_sigs",
            description="Duplicate signal test",
            priority=4,
            signals=[
                SignalDeclaration(signal_name="x", data_type="uint8_t"),
                SignalDeclaration(signal_name="x", data_type="uint8_t"),
            ],
        )


def test_common_message_reference_discriminator_rejects_inline_fields():
    with pytest.raises(ValidationError):
        BusAttachmentDeclaration.model_validate(
            {
                "peripheral": "FDCAN1",
                "tx": [
                    {
                        "common_message": "node_init",
                        "message_name": "a_init",
                        "signals": [{"signal_name": "csr", "data_type": "uint32_t"}],
                    }
                ],
            }
        )


def test_assemble_source_expands_common_messages_with_overrides():
    common = {
        "node_init": CommonMessageDeclaration(
            name="node_init",
            description="Node initialized",
            priority=0,
            signals=[SignalDeclaration(signal_name="csr", data_type="uint32_t")],
        ),
        "ext_cmd": CommonMessageDeclaration(
            name="ext_cmd",
            description="External command",
            priority=1,
            period_ms=50,
            signals=[SignalDeclaration(signal_name="cmd", data_type="uint16_t")],
        ),
    }
    declarations = CanDeclarations(
        buses={
            "VCAN": BusDeclaration(
                name="VCAN",
                bus_id=0,
                baud_rate=500000,
                is_extended_id=False,
                host_fault_library=True,
            ),
        },
        custom_types={},
        common_messages=common,
        internal_nodes=[
            NodeDeclaration(
                node_name="NODE_A",
                busses={
                    "VCAN": BusAttachmentDeclaration(
                        peripheral="FDCAN1",
                        tx=[
                            CommonMessageReference(
                                common_message="node_init",
                                message_name="node_a_init",
                            ),
                            CommonMessageReference(
                                common_message="node_init",
                                message_name="node_a_alt_init",
                                description="Custom desc",
                                priority=3,
                                period_ms=100,
                                id_override="0x123",
                                byte_order="big_endian",
                            ),
                        ],
                    )
                },
                fault_library_enabled=False,
            )
        ],
        external_nodes=[
            ExternalNodeDeclaration(
                node_name="EXT_B",
                bus_name="VCAN",
                tx=[
                    CommonMessageReference(
                        common_message="ext_cmd",
                        message_name="ext_b_cmd",
                        id_override="0x200",
                    )
                ],
            )
        ],
    )

    source = assemble_source(declarations, ())
    by_name = {item.message.message_name: item.message for item in source.tx_messages}

    assert set(by_name) == {"node_a_init", "node_a_alt_init", "ext_b_cmd"}
    assert by_name["node_a_init"].description == "Node initialized"
    assert by_name["node_a_init"].priority == 0
    assert by_name["node_a_init"].period_ms == 0
    assert by_name["node_a_init"].signals == common["node_init"].signals

    assert by_name["node_a_alt_init"].description == "Custom desc"
    assert by_name["node_a_alt_init"].priority == 3
    assert by_name["node_a_alt_init"].period_ms == 100
    assert by_name["node_a_alt_init"].id_override == "0x123"
    assert by_name["node_a_alt_init"].byte_order == "big_endian"

    assert by_name["ext_b_cmd"].priority == 1
    assert by_name["ext_b_cmd"].period_ms == 50
    assert by_name["ext_b_cmd"].id_override == "0x200"


def test_load_declarations_rejects_unknown_common_message(tmp_path: Path):
    (tmp_path / "system").mkdir()
    (tmp_path / "nodes").mkdir()
    (tmp_path / "external_nodes").mkdir()

    (tmp_path / "system" / "bus_configs.json").write_text(
        json.dumps(
            {
                "busses": [
                    {
                        "name": "VCAN",
                        "bus_id": 0,
                        "baud_rate": 500000,
                        "is_extended_id": False,
                        "host_fault_library": True,
                    }
                ]
            }
        )
    )
    (tmp_path / "system" / "common_types.json").write_text(json.dumps({"types": []}))
    (tmp_path / "system" / "common_messages.json").write_text(json.dumps({"messages": []}))
    (tmp_path / "nodes" / "NODE_A.json").write_text(
        json.dumps(
            {
                "node_name": "NODE_A",
                "busses": {
                    "VCAN": {
                        "peripheral": "FDCAN1",
                        "tx": [
                            {
                                "common_message": "missing_template",
                                "message_name": "node_a_missing",
                            }
                        ],
                    }
                },
                "fault_library_enabled": False,
            }
        )
    )

    with pytest.raises(DeclarationValidationError):
        load_declarations(tmp_path)
