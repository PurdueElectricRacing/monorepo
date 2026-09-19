import copy
import hashlib
import json
from dataclasses import replace

import cantools
import pytest
from pydantic import ValidationError

import generate
from canpiler.api import Canpiler
from canpiler.compiler import compile_message
from canpiler.export_models import SignalExport, SystemExport, system_json_schema
from canpiler.system_json_generator import content_hash, generate_system_json
from canpiler.system_json_generator import generate_schema
from canpiler.pipeline_models import LinkedMessage
from core.declaration_loader import load_declarations
from core.declarations import CustomTypeDeclaration, MessageDeclaration, SignalDeclaration
from faultgen.api import FaultGenerator


@pytest.fixture(scope="module")
def linked():
    declarations = load_declarations()
    faults = FaultGenerator()
    compiler = Canpiler()
    source = compiler.assemble_source(declarations, [faults.contribute(faults.plan(declarations))])
    return compiler.link(compiler.compile(source))


@pytest.fixture
def document(linked):
    return json.loads(generate_system_json(linked, "abcdef0").content)


def test_schema_matches_models():
    artifact = generate_schema()
    assert artifact.collection == "dbc"
    assert artifact.relative_path == "system_schema.json"
    assert json.loads(artifact.content) == system_json_schema()


def test_real_generation_and_dbc_parity(linked):
    artifacts = Canpiler().generate(linked, "abcdef0")
    json_artifacts = [a for a in artifacts if a.relative_path.endswith(".json")]
    assert len(json_artifacts) == 2
    schemas = [a for a in json_artifacts if a.relative_path == "system_schema.json"]
    assert schemas == [generate_schema()]
    systems = [a for a in json_artifacts if a.relative_path.startswith("system_") and a.relative_path != "system_schema.json"]
    assert len(systems) == 1
    artifact = systems[0]
    assert artifact.relative_path == "system_abcdef0.json"
    assert artifact.content.endswith("\n")
    document = SystemExport.model_validate_json(artifact.content)
    assert set(document.buses) == set(linked.bus_configs)
    for name, bus in document.buses.items():
        assert bus.baud_rate == linked.bus_configs[name].baud_rate
        assert {n.name: n.is_external for n in bus.nodes} == {
            n.name: n.is_external for n in linked.nodes if name in n.busses
        }
    dbcs = [a for a in artifacts if a.relative_path.endswith(".dbc")]
    assert len(dbcs) == len({bus for node in linked.nodes for bus in node.busses})
    for artifact in dbcs:
        bus_name = artifact.relative_path.removesuffix("_abcdef0.dbc")
        bus = document.buses[bus_name]
        dbc = cantools.database.load_string(artifact.content, database_format="dbc")
        assert {n.name for n in dbc.nodes} == {n.name for n in bus.nodes}
        assert len(bus.messages) == len(dbc.messages)
        for message in bus.messages:
            old = dbc.get_message_by_name(message.message_name)
            assert (message.id, message.is_extended_id, message.length_bytes) == (
                old.frame_id, old.is_extended_frame, old.length
            )
            assert old.senders == [message.transmitter]
            assert message.description == (old.comment or "")
            for signal in message.signals:
                previous = old.get_signal_by_name(signal.signal_name)
                assert (signal.start_bit, signal.bit_length, signal.byte_order) == (
                    previous.start, previous.length, previous.byte_order
                )
                assert signal.scale == previous.scale
                assert signal.offset == previous.offset
                assert signal.unit == (previous.unit or "")
                assert signal.description == (previous.comment or "")
                assert (signal.raw_type == "signed") == previous.is_signed
                assert (signal.raw_type == "float32") == previous.is_float
                assert signal.choices == {str(k): str(v) for k, v in (previous.choices or {}).items()}
                # DBC serializes missing bounds as zero, whereas JSON preserves absence.
                if signal.minimum is not None:
                    assert signal.minimum == previous.minimum
                if signal.maximum is not None:
                    assert signal.maximum == previous.maximum
    for placed in linked.tx_messages:
        bus = document.buses[placed.bus_name]
        message = next(m for m in bus.messages if m.message_name == placed.message.message_name)
        assert message.nominal_period_ms == (placed.message.period_ms or None)
        assert message.priority == placed.message.priority
        assert message.receivers == sorted({
            rx.node_name for rx in linked.subscriptions
            if (rx.bus_name, rx.message_name) == (placed.bus_name, message.message_name)
        })
        for original in placed.message.signals:
            signal = next(s for s in message.signals if s.signal_name == original.signal_name)
            assert signal.minimum == original.min
            assert signal.maximum == original.max
    fault_signals = [s for b in document.buses.values() for m in b.messages
                     for s in m.signals if s.data_type == "fault_id_t"]
    assert fault_signals and all(s.choices for s in fault_signals)


def test_hash_determinism(linked, document):
    encoded = json.dumps(
        {"schema_version": 1, "buses": document["buses"]},
        sort_keys=True, separators=(",", ":"), ensure_ascii=False, allow_nan=False,
    ).encode("utf-8")
    assert document["content_hash"] == hashlib.sha256(encoded).hexdigest()
    reordered = replace(
        linked,
        nodes=tuple(reversed(linked.nodes)),
        tx_messages=tuple(replace(tx, message=replace(tx.message, signals=tuple(reversed(tx.message.signals))))
                          for tx in reversed(linked.tx_messages)),
        subscriptions=tuple(reversed(linked.subscriptions)),
        bus_configs=dict(reversed(list(linked.bus_configs.items()))),
    )
    second = json.loads(generate_system_json(reordered, "1234567").content)
    assert second["content_hash"] == document["content_hash"]
    assert second["buses"] == document["buses"]
    assert second["versions"]["hash"] != document["versions"]["hash"]
    assert content_hash(document["buses"], 2) != document["content_hash"]
    second["buses"]["VCAN"]["baud_rate"] += 1
    assert content_hash(second["buses"]) != document["content_hash"]


def test_empty_bus(linked):
    configs = dict(linked.bus_configs)
    configs["EMPTY"] = configs["VCAN"].model_copy(update={"name": "EMPTY"})
    result = json.loads(generate_system_json(replace(linked, bus_configs=configs), "test").content)
    assert result["buses"]["EMPTY"] == {"baud_rate": configs["EMPTY"].baud_rate, "nodes": [], "messages": []}


@pytest.mark.parametrize("byte_order", ["little_endian", "big_endian"])
@pytest.mark.parametrize("data_type,raw_type,length", [
    ("uint16_t", "unsigned", 16), ("int16_t", "signed", 16),
    ("float", "float32", 32), ("bool", "unsigned", 1),
    ("car_state_t", "unsigned", 8),
])
def test_signal_types_defaults_and_zero(linked, byte_order, data_type, raw_type, length):
    declaration = MessageDeclaration(
        message_name="test", description="test", priority=0, byte_order=byte_order,
        signals=[SignalDeclaration(signal_name="value", data_type=data_type, min=0, max=0)],
    )
    compiled = compile_message(declaration, False, linked.custom_types)
    message = LinkedMessage(**compiled.__dict__, final_id=123)
    placed = replace(linked.tx_messages[0], message=message)
    context = replace(linked, tx_messages=(placed,), subscriptions=())
    doc = json.loads(generate_system_json(context, "test").content)
    exported = doc["buses"][placed.bus_name]["messages"][0]
    signal = exported["signals"][0]
    assert exported["nominal_period_ms"] is None
    assert (signal["raw_type"], signal["bit_length"]) == (raw_type, length)
    assert signal["minimum"] == signal["maximum"] == 0
    assert signal["unit"] == ""
    assert signal["scale"] == 1 and signal["offset"] == 0
    assert bool(signal["choices"]) == (data_type in ("bool", "car_state_t"))
    updated = replace(message, signals=(replace(message.signals[0], min=None, max=None, scale=1.0),))
    other = json.loads(generate_system_json(replace(context, tx_messages=(replace(placed, message=updated),)), "test").content)
    other_signal = other["buses"][placed.bus_name]["messages"][0]["signals"][0]
    assert other_signal["minimum"] is None and other_signal["maximum"] is None
    same = replace(updated, signals=(replace(updated.signals[0], scale=1),))
    assert other["content_hash"] == json.loads(generate_system_json(
        replace(context, tx_messages=(replace(placed, message=same),)), "test"
    ).content)["content_hash"]


@pytest.mark.parametrize("change", [
    {"bit_length": 0}, {"scale": float("inf")}, {"offset": float("nan")},
    {"choices": {"01": "bad"}}, {"raw_type": "unknown"},
])
def test_invalid_signal_metadata(document, change):
    signal = copy.deepcopy(document["buses"]["VCAN"]["messages"][0]["signals"][0])
    signal.update(change)
    with pytest.raises(ValidationError):
        SignalExport.model_validate(signal)


def test_required_and_unknown_fields(document):
    signal = document["buses"]["VCAN"]["messages"][0]["signals"][0]
    for field in ("choices", "unit", "minimum", "maximum"):
        missing = {key: value for key, value in signal.items() if key != field}
        with pytest.raises(ValidationError, match="Field required"):
            SignalExport.model_validate(missing)
    with pytest.raises(ValidationError, match="Extra inputs"):
        SystemExport.model_validate({**document, "extra": True})


def test_unsupported_schema_version(document):
    document["versions"]["schema_version"] = 2
    with pytest.raises(ValidationError):
        SystemExport.model_validate(document)


@pytest.mark.parametrize("byte_order", ["little_endian", "big_endian"])
def test_known_payload_against_cantools(linked, byte_order):
    from canpiler.dbc_generator import generate_dbcs

    declaration = MessageDeclaration(
        message_name="payload_test", description="test", priority=0, byte_order=byte_order,
        signals=[
            SignalDeclaration(signal_name="temperature", data_type="int16_t", scale=0.5, offset=10, unit="degC"),
            SignalDeclaration(signal_name="state", data_type="car_state_t"),
            SignalDeclaration(signal_name="reserved_0", data_type="uint8_t"),
        ],
    )
    compiled = compile_message(declaration, False, linked.custom_types)
    placed = replace(linked.tx_messages[0], message=LinkedMessage(**compiled.__dict__, final_id=123))
    context = replace(linked, tx_messages=(placed,), subscriptions=())
    doc = json.loads(generate_system_json(context, "test").content)
    signals = doc["buses"][placed.bus_name]["messages"][0]["signals"]
    payload = ((-20).to_bytes(2, "little" if byte_order == "little_endian" else "big", signed=True)
               + bytes([3, 0]))
    artifact = next(a for a in generate_dbcs(context, "test")
                    if a.relative_path == f"{placed.bus_name}_test.dbc")
    db = cantools.database.load_string(artifact.content, database_format="dbc")
    numeric = db.decode_message(123, payload, decode_choices=False)
    labeled = db.decode_message(123, payload, decode_choices=True)
    for signal in signals:
        model = SignalExport.model_validate(signal)
        bits = []
        bit = model.start_bit
        for _ in range(model.bit_length):
            bits.append(bit)
            if model.byte_order == "little_endian":
                bit += 1
            else:
                bit = bit + 15 if bit % 8 == 0 else bit - 1
        if model.byte_order == "big_endian":
            bits.reverse()
        raw = sum(((payload[bit // 8] >> (bit % 8)) & 1) << index
                  for index, bit in enumerate(bits))
        if model.raw_type == "signed" and raw & (1 << (model.bit_length - 1)):
            raw -= 1 << model.bit_length
        assert raw * model.scale + model.offset == numeric[model.signal_name]
        if str(raw) in model.choices:
            assert model.choices[str(raw)] == str(labeled[model.signal_name])
    assert numeric["temperature"] == 0
    assert str(labeled["state"]) == "energized"


def test_custom_float_and_enum_override(linked):
    from canpiler.system_json_generator import _signal

    custom_types = {**linked.custom_types, "measurement_t": CustomTypeDeclaration(
        name="measurement_t", base_type="float"
    )}
    context = replace(linked, custom_types=custom_types)
    message = compile_message(MessageDeclaration(
        message_name="test", description="", priority=0,
        signals=[
            SignalDeclaration(signal_name="measurement", data_type="measurement_t"),
            SignalDeclaration(signal_name="state", data_type="car_state_t", choices=["OVERRIDE"]),
        ],
    ), False, custom_types)
    assert _signal(message.signals[0], context).raw_type == "float32"
    assert _signal(message.signals[1], context).choices == {"0": "OVERRIDE"}


def test_cleanup_and_validation_before_cleanup(tmp_path, monkeypatch):
    dbc = tmp_path / "dbc"
    generated = tmp_path / "generated"
    dbc.mkdir()
    for name in ("old.dbc", "system_old.json", "unrelated.json"):
        (dbc / name).write_text("sentinel")
    monkeypatch.setattr(generate, "DBC_DIR", dbc)
    monkeypatch.setattr(generate, "GENERATED_DIR", generated)
    monkeypatch.setattr(generate, "get_git_hash", lambda: "test")
    generate.generate()
    assert not (dbc / "system_old.json").exists()
    assert not (dbc / "old.dbc").exists()
    assert (dbc / "unrelated.json").read_text() == "sentinel"
    assert len(list(dbc.glob("system_*.json"))) == 2
    before = {p: p.read_bytes() for p in tmp_path.rglob("*") if p.is_file()}
    original_load = generate.load_declarations

    def invalid_declarations():
        declarations = original_load()
        # Valid declaration shape, invalid resolved float width: compilation
        # must reject it before any exporter or cleanup runs.
        for node in declarations.internal_nodes:
            for attachment in node.busses.values():
                if attachment.tx:
                    attachment.tx[0] = MessageDeclaration(
                        message_name=attachment.tx[0].message_name,
                        description="invalid float", priority=0,
                        signals=[SignalDeclaration(
                            signal_name="bad_float", data_type="float", length=16,
                        )],
                    )
                    return declarations
        raise AssertionError("fixture needs a transmitting node")

    monkeypatch.setattr(generate, "load_declarations", invalid_declarations)
    with pytest.raises(ValueError, match="bad_float.*float requires 32 bits"):
        generate.generate()
    assert before == {p: p.read_bytes() for p in tmp_path.rglob("*") if p.is_file()}
