"""
test_filgen.py

In-memory content pin and resolution/packing tests for filgen. The
pipeline fixture builds a real LinkedCan exactly the way
generators/generate.py does; the content test pins FilGenerator's
emitted stimulus against the expected document, and the unit tests
cover bus/id resolution and signal packing.
"""

import pytest
from pydantic import ValidationError

from canpiler.api import Canpiler
from canpiler.pipeline.models import LinkedCan, LinkedMessage
from core.declaration_loader import load_declarations
from faultgen.api import FaultGenerator
from filgen.api import (
    FilGenerator,
    FilScenarioError,
    READY_TO_DRIVE_SCENARIO_FILE,
    load_fil_scenario,
)
from filgen.models import FilScenario, FilScenarioCan


@pytest.fixture(scope="module")
def linked() -> LinkedCan:
    """Build the linked CAN graph the same way generators/generate.py does."""
    canpiler = Canpiler()
    faultgen = FaultGenerator()
    declarations = load_declarations()
    fault_plan = faultgen.plan(declarations)
    fault_contribution = faultgen.contribute(fault_plan)
    source = canpiler.assemble_source(declarations, [fault_contribution])
    compiled = canpiler.compile(source)
    return canpiler.link(compiled)


@pytest.fixture(scope="module")
def generator() -> FilGenerator:
    return FilGenerator()


@pytest.fixture(scope="module")
def artifacts(linked, generator) -> dict[str, str]:
    """Emit every FIL artifact once, keyed by relative path."""
    scenario = load_fil_scenario(READY_TO_DRIVE_SCENARIO_FILE)
    return {
        artifact.relative_path: artifact.content
        for artifact in generator.generate(linked, scenario)
    }


def _can_entry(**kwargs) -> FilScenarioCan:
    return FilScenarioCan(**kwargs)


def _linked_message(linked: LinkedCan, bus: str, name: str) -> LinkedMessage:
    """Find a tx message by (bus, name); the linked graph is a test input."""
    for tx in linked.tx_messages:
        if tx.bus_name == bus and tx.message.message_name == name:
            return tx.message
    raise AssertionError(f"{name} on {bus} not found in linked tx messages")


# ---------------------------------------------------------------------------
# In-memory content pin: the gitignored stimulus must match exactly.
# ---------------------------------------------------------------------------

def test_stimuli_match_expected_content(artifacts) -> None:
    # The stimulus file is gitignored (a derived artifact of the scenario),
    # so its correctness is pinned in-memory against the expected document
    # rather than against a committed file.
    relative_path = "stimuli/ready_to_drive_stimuli.json"
    expected = (
        '{\n'
        '  "schema_version": 1,\n'
        '  "name": "per_car_ready_to_drive",\n'
        '  "events": [\n'
        '    {"type": "gpio", "at_ms": 0, "board": "main_module", "pin": "PB9", "value": true},\n'
        '    {"type": "gpio", "at_ms": 0, "board": "main_module", "pin": "PC4", "value": false},\n'
        '    {"type": "gpio", "at_ms": 0, "board": "dashboard", "pin": "PB14", "value": true},\n'
        '\n'
        '    {"type": "adc", "at_ms": 0, "board": "dashboard", "instance": "ADC1", "channel": 8, "value": 200},\n'
        '    {"type": "adc", "at_ms": 0, "board": "dashboard", "instance": "ADC1", "channel": 7, "value": 775},\n'
        '    {"type": "adc", "at_ms": 0, "board": "dashboard", "instance": "ADC1", "channel": 2, "value": 2550},\n'
        '    {"type": "adc", "at_ms": 0, "board": "dashboard", "instance": "ADC1", "channel": 9, "value": 650},\n'
        '\n'
        '    {"type": "can", "at_ms": 500, "repeat": {"every_ms": 500, "count": 13}, "bus": "1", "id": "0x231", "data": [0, 41, 0, 0]},\n'
        '    {"type": "can", "at_ms": 510, "repeat": {"every_ms": 500, "count": 13}, "bus": "1", "id": "0x232", "data": [0, 41, 0, 0]},\n'
        '    {"type": "can", "at_ms": 520, "repeat": {"every_ms": 500, "count": 13}, "bus": "1", "id": "0x233", "data": [0, 41, 0, 0]},\n'
        '    {"type": "can", "at_ms": 530, "repeat": {"every_ms": 500, "count": 13}, "bus": "1", "id": "0x234", "data": [0, 41, 0, 0]},\n'
        '\n'
        '    {"type": "gpio", "at_ms": 2500, "board": "dashboard", "pin": "PB14", "value": false},\n'
        '    {"type": "gpio", "at_ms": 2600, "board": "dashboard", "pin": "PB14", "value": true}\n'
        '  ],\n'
        '  "expect": [\n'
        '    {"type": "can", "bus": "1", "id": "0x111", "data": [0, 0, 0, 0, 0, 0, 0, 0], "at_ms": 90, "window_ms": 30},\n'
        '    {"type": "can", "bus": "1", "id": "0x112", "data": [0, 0, 0, 0, 0, 0, 0, 0], "at_ms": 90, "window_ms": 30},\n'
        '    {"type": "can", "bus": "1", "id": "0x113", "data": [0, 0, 0, 0, 0, 0, 0, 0], "at_ms": 90, "window_ms": 30},\n'
        '    {"type": "can", "bus": "1", "id": "0x114", "data": [0, 0, 0, 0, 0, 0, 0, 0], "at_ms": 90, "window_ms": 30},\n'
        '    {"type": "can", "bus": "1", "id": "0x111", "data": [0, 7, 0, 0, 92, 8, 12, 254], "at_ms": 5000, "window_ms": 200},\n'
        '    {"type": "can", "bus": "1", "id": "0x112", "data": [0, 7, 0, 0, 92, 8, 12, 254], "at_ms": 5000, "window_ms": 200},\n'
        '    {"type": "can", "bus": "1", "id": "0x113", "data": [0, 7, 0, 0, 92, 8, 12, 254], "at_ms": 5000, "window_ms": 200},\n'
        '    {"type": "can", "bus": "1", "id": "0x114", "data": [0, 7, 0, 0, 92, 8, 12, 254], "at_ms": 5000, "window_ms": 200},\n'
        '    {"type": "can", "bus": "0", "id": "0x46", "data": [5], "at_ms": 5000, "window_ms": 200}\n'
        '  ]\n'
        '}\n'
    )
    assert artifacts[relative_path] == expected, (
        f"{relative_path} drifted from the expected stimulus content; regenerate "
        "with generators/generate.py"
    )


# ---------------------------------------------------------------------------
# Resolution unit tests
# ---------------------------------------------------------------------------

def test_bus_name_resolves_to_fil_bus_id(linked, generator) -> None:
    assert generator._bus_id(linked, "MCAN") == "1"
    assert generator._bus_id(linked, "VCAN") == "0"


def test_message_id_resolution(linked, generator) -> None:
    # INVA_INFO on MCAN has a pinned id_override of 0x231.
    inva_info = _linked_message(linked, "MCAN", "INVA_INFO")
    assert f"0x{inva_info.final_id:x}" == "0x231"

    # main_hb on VCAN gets 0x46 from the dynamic link (priority ordering).
    main_hb = _linked_message(linked, "VCAN", "main_hb")
    assert f"0x{main_hb.final_id:x}" == "0x46"

    for name, expected in (("INVA_INFO", "0x231"), ("main_hb", "0x46")):
        entry = _can_entry(bus="MCAN" if name == "INVA_INFO" else "VCAN", message=name, at_ms=0)
        message = generator._message(linked, entry)
        assert f"0x{message.final_id:x}" == expected


# ---------------------------------------------------------------------------
# Packing unit tests
# ---------------------------------------------------------------------------

def test_enum_choice_resolves_by_name(linked, generator) -> None:
    # main_hb is dlc 1; car_state_t's 'ready2drive' choice is raw value 5.
    message = _linked_message(linked, "VCAN", "main_hb")
    data = generator._data(
        linked, message, _can_entry(bus="VCAN", message="main_hb", at_ms=0, values={"car_state": "ready2drive"})
    )
    assert data == [5]


def test_scale_one_signal_packing(linked, generator) -> None:
    # AMK_DCBusVoltage: uint16 at bit_offset 16, length 16, scale None (1.0),
    # little-endian: raw 41 lands in payload bits 16-31 -> bytes [0, 0, 41, 0].
    message = _linked_message(linked, "MCAN", "INVA_INFO")
    data = generator._data(
        linked, message, _can_entry(bus="MCAN", message="INVA_INFO", at_ms=0, values={"AMK_DCBusVoltage": 41})
    )
    assert data == [0, 0, 41, 0]


def test_scaled_signal_packing(linked, generator) -> None:
    # AMK_TorqueSetpoint: int16 at bit_offset 16, scale 0.1 (raw = value / 0.1).
    # 2.0 Nm -> raw 20 (0x14) at bits 16-31 -> bytes [0, 0, 20, 0, ...] of 8.
    message = _linked_message(linked, "MCAN", "INVA_SET")
    data = generator._data(
        linked, message, _can_entry(bus="MCAN", message="INVA_SET", at_ms=0, values={"AMK_TorqueSetpoint": 2.0})
    )
    assert data == [0, 0, 20, 0, 0, 0, 0, 0]


def test_bool_and_signed_limit_packing(linked, generator) -> None:
    # The ready_to_drive 5000 ms INVA_SET expect: control bits, positive and
    # negative torque limits. Pins the bool, scaled, and signed packing paths.
    # Control word: InverterOn(bit 8) | DcOn(bit 9) | Enable(bit 10) =
    # 0b0111 = 7 -> byte 1. AMK_PositiveTorqueLimit: 214.0 / 0.1 = 2140 =
    # 0x085C little-endian -> bytes [92, 8]. AMK_NegativeTorqueLimit:
    # -50.0 / 0.1 = -500 -> int16 two's complement 65536 - 500 = 0xFE0C
    # little-endian -> bytes [12, 254]. Zero-valued signals stay 0.
    message = _linked_message(linked, "MCAN", "INVA_SET")
    data = generator._data(
        linked,
        message,
        _can_entry(
            bus="MCAN",
            message="INVA_SET",
            at_ms=0,
            values={
                "AMK_Control_bInverterOn": True,
                "AMK_Control_bDcOn": True,
                "AMK_Control_bEnable": True,
                "AMK_PositiveTorqueLimit": 214.0,
                "AMK_NegativeTorqueLimit": -50.0,
            },
        ),
    )
    assert data == [0, 7, 0, 0, 92, 8, 12, 254]


def test_default_zero_frame(linked, generator) -> None:
    # Empty values packs every signal to raw 0.
    message = _linked_message(linked, "MCAN", "INVA_SET")
    data = generator._data(
        linked, message, _can_entry(bus="MCAN", message="INVA_SET", at_ms=0, values={})
    )
    assert data == [0] * message.dlc


def test_raw_payload_passes_through(linked, generator) -> None:
    raw = [0, 7, 0, 0, 92, 8, 12, 254]
    message = _linked_message(linked, "MCAN", "INVA_SET")
    data = generator._data(
        linked, message, _can_entry(bus="MCAN", message="INVA_SET", at_ms=0, raw=raw)
    )
    assert data == raw


def test_signed_value_wraps_to_twos_complement(linked, generator) -> None:
    # AMK_TorqueSetpoint is a signed int16 at bit_offset 16, scale 0.1.
    # -1.0 Nm -> raw -10 -> two's complement 65536 - 10 = 65526 = 0xFFF6,
    # so bytes 2 and 3 (little-endian) are 0xF6 and 0xFF.
    message = _linked_message(linked, "MCAN", "INVA_SET")
    data = generator._data(
        linked, message, _can_entry(bus="MCAN", message="INVA_SET", at_ms=0, values={"AMK_TorqueSetpoint": -1.0})
    )
    assert data == [0, 0, 0xF6, 0xFF, 0, 0, 0, 0]


# ---------------------------------------------------------------------------
# Error paths
# ---------------------------------------------------------------------------

def test_unknown_message_raises(linked, generator) -> None:
    entry = _can_entry(bus="MCAN", message="NO_SUCH_MSG", at_ms=0)
    with pytest.raises(FilScenarioError, match="unknown CAN message 'NO_SUCH_MSG'"):
        generator._message(linked, entry)


def test_unknown_bus_raises(linked, generator) -> None:
    with pytest.raises(FilScenarioError, match="unknown bus 'NOPE'"):
        generator._bus_id(linked, "NOPE")


def test_unknown_enum_choice_raises(linked, generator) -> None:
    message = _linked_message(linked, "VCAN", "main_hb")
    entry = _can_entry(bus="VCAN", message="main_hb", at_ms=0, values={"car_state": "not_a_state"})
    with pytest.raises(FilScenarioError, match="unknown enum choice 'not_a_state'"):
        generator._data(linked, message, entry)


def test_unknown_signal_raises(linked, generator) -> None:
    message = _linked_message(linked, "MCAN", "INVA_INFO")
    entry = _can_entry(bus="MCAN", message="INVA_INFO", at_ms=0, values={"NO_SUCH_SIG": 1})
    with pytest.raises(FilScenarioError, match="no signal named 'NO_SUCH_SIG'"):
        generator._data(linked, message, entry)


def test_raw_and_values_together_raise(linked, generator) -> None:
    message = _linked_message(linked, "MCAN", "INVA_INFO")
    entry = _can_entry(
        bus="MCAN", message="INVA_INFO", at_ms=0, values={"AMK_DCBusVoltage": 41}, raw=[0] * 4
    )
    with pytest.raises(FilScenarioError, match="exactly one is allowed"):
        generator._data(linked, message, entry)


def test_scenario_rejects_extra_json_keys() -> None:
    payload = {
        "schema_version": 1,
        "name": "bogus",
        "events": [],
        "expect": [],
        "not_a_scenario_field": True,
    }
    with pytest.raises(ValidationError, match="not_a_scenario_field"):
        FilScenario.model_validate(payload)
