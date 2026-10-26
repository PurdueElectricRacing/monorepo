"""
api.py

Author: Irving Wang (irvingw@purdue.edu)

FilGenerator emits the FIL board and network JSON files under
tests/fil/ as derived artifacts of the linked CAN graph.

The FIL network is a fixed projection of the PER vehicle network: only
the VCAN and MCAN logical buses participate, and the six vehicle boards
are built from the node peripheral mappings. DRIVELINE physically
splits into two boards (front and rear), so the board-to-node mapping
is explicit rather than derived.

Ready-to-drive stimuli are resolved from the hand-maintained scenario
file in tests/fil/scenarios/: logical bus and message names are mapped
to bus IDs and CAN IDs from the linked graph, and signal values are
packed exactly like the generated PACK/UNPACK macros.
"""

import json

from pydantic import ValidationError

from core.config import FIL_STIMULI_DIR

from canpiler.pipeline.models import CompiledMessage, LinkedCan, LinkedMessage
from core.artifacts import Artifact
from core.utils import print_as_ok, print_as_success
from .models import (
    FilBoard,
    FilBoardCanAttachment,
    FilBus,
    FilNetwork,
    FilScenario,
    FilScenarioCan,
    FilStimulus,
    FilStimulusCanEvent,
    FilStimulusCanExpect,
)


class FilScenarioError(ValueError):
    """A FIL scenario file is missing, malformed, or references unknown CAN names."""

# The logical buses that make up the FIL network, in the fixed board set.
# All other logical buses (e.g. DASHBOARD's SCAN, A_BOX's CCAN) are
# deliberately excluded from board and network emission.
FIL_BUS_NAMES = frozenset({"VCAN", "MCAN"})

# Board name -> node name, in network-list order. The DRIVELINE node is
# deliberately mapped to two boards because its physical hardware splits
# front and rear; both boards share its VCAN peripheral attachment.
# Every other board name is the lowercase form of its node name. A
# single mapping makes a names/mapping drift unrepresentable.
FIL_BOARD_TO_NODE = (
    ("dashboard", "DASHBOARD"),
    ("main_module", "MAIN_MODULE"),
    ("torque_vector", "TORQUE_VECTOR"),
    ("a_box", "A_BOX"),
    ("front_driveline", "DRIVELINE"),
    ("rear_driveline", "DRIVELINE"),
)

# Derived from FIL_BOARD_TO_NODE so the two can never drift apart.
FIL_BOARD_NAMES = tuple(board for board, _node in FIL_BOARD_TO_NODE)

FIL_SCHEMA_VERSION = 1
FIL_MCU_FILENAME = "stm32g474retx.json"
FIL_MCU_RELATIVE_PATH = f"../../external/fil/configs/mcus/{FIL_MCU_FILENAME}"
FIL_ELF_RELATIVE_TEMPLATE = "../../../firmware/output/{board}/{board}.elf"
FIL_VECTOR_BASE = "0x08000000"

VEHICLE_NETWORK_NAME = "per_vehicle"
READY_TO_DRIVE_NETWORK_NAME = "per_ready_to_drive"

VEHICLE_NETWORK_FILE = "network.json"
READY_TO_DRIVE_FILE = "ready_to_drive.json"
# The single ready-to-drive scenario; resolved to a stimuli file that
# ready_to_drive.json references.
READY_TO_DRIVE_SCENARIO_NAME = "ready_to_drive"
READY_TO_DRIVE_SCENARIO_FILE = f"{READY_TO_DRIVE_SCENARIO_NAME}.json"
READY_TO_DRIVE_STIMULI_FILE = (
    f"{FIL_STIMULI_DIR.name}/{READY_TO_DRIVE_SCENARIO_NAME}_stimuli.json"
)

JSON_INDENT = 2


class FilGenerator:
    """Generates the FIL board and network config artifacts."""

    def generate(
        self, linked: LinkedCan, scenario: FilScenario
    ) -> list[Artifact]:
        nodes = {node.name: node for node in linked.nodes}

        for _board, node_name in FIL_BOARD_TO_NODE:
            if node_name not in nodes:
                raise ValueError(
                    f"FIL board node '{node_name}' missing from linked CAN"
                )

        board_to_node = dict(FIL_BOARD_TO_NODE)
        artifacts = [
            Artifact(
                "fil",
                f"boards/{board}.json",
                _dump_json(self._board(board, nodes[board_to_node[board]], linked)),
            )
            for board in FIL_BOARD_NAMES
        ]

        network_buses = self._network_buses(linked)
        network_boards = [f"boards/{board}.json" for board in FIL_BOARD_NAMES]

        artifacts.append(
            Artifact(
                "fil",
                VEHICLE_NETWORK_FILE,
                _dump_json(
                    self._network(
                        VEHICLE_NETWORK_NAME,
                        [],
                        network_buses,
                        network_boards,
                    )
                ),
            )
        )
        artifacts.append(
            Artifact(
                "fil",
                READY_TO_DRIVE_FILE,
                _dump_json(
                    self._network(
                        READY_TO_DRIVE_NETWORK_NAME,
                        [READY_TO_DRIVE_STIMULI_FILE],
                        network_buses,
                        network_boards,
                    )
                ),
            )
        )

        artifacts.append(
            Artifact(
                "fil",
                READY_TO_DRIVE_STIMULI_FILE,
                _dump_stimulus(self._stimulus(linked, scenario)),
            )
        )

        for artifact in artifacts:
            print_as_ok(f"Generated {artifact.relative_path}")
        print_as_success("FIL board and network files generated")
        return artifacts

    def _stimulus(self, linked: LinkedCan, scenario: FilScenario) -> FilStimulus:
        """Resolve the scenario's gpio/adc/can entries into a FIL stimulus."""
        return FilStimulus(
            schema_version=FIL_SCHEMA_VERSION,
            name=scenario.name,
            events=[self._event(linked, entry) for entry in scenario.events],
            expect=[self._expectation(linked, entry) for entry in scenario.expect],
        )

    def _event(self, linked: LinkedCan, entry) -> dict:
        if isinstance(entry, FilScenarioCan):
            bus_id = self._bus_id(linked, entry.bus)
            message = self._message(linked, entry)
            event = FilStimulusCanEvent(
                at_ms=entry.at_ms,
                repeat=entry.repeat,
                bus=bus_id,
                id=f"0x{message.final_id:x}",
                data=self._data(linked, message, entry),
            )
            dump = event.model_dump()
            if dump["repeat"] is None:
                del dump["repeat"]
            return dump
        return entry.model_dump()

    def _expectation(self, linked: LinkedCan, entry) -> dict:
        bus_id = self._bus_id(linked, entry.bus)
        message = self._message(linked, entry)
        expect = FilStimulusCanExpect(
            bus=bus_id,
            id=f"0x{message.final_id:x}",
            data=self._data(linked, message, entry),
            at_ms=entry.at_ms,
            window_ms=entry.window_ms,
        )
        return expect.model_dump()

    def _bus_id(self, linked: LinkedCan, bus_name: str) -> str:
        config = linked.bus_configs.get(bus_name)
        if config is None:
            raise FilScenarioError(
                f"scenario references unknown bus '{bus_name}' "
                f"(known: {sorted(linked.bus_configs)})"
            )
        return str(config.bus_id)

    def _message(self, linked: LinkedCan, entry: FilScenarioCan) -> LinkedMessage:
        """Resolve (bus, message) to its linked message via tx, then subscriptions."""
        tx = linked.tx_messages
        for candidate in tx:
            if candidate.bus_name == entry.bus and candidate.message.message_name == entry.message:
                return candidate.message
        for candidate in linked.subscriptions:
            if candidate.bus_name == entry.bus and candidate.message_name == entry.message:
                return candidate.resolved_message
        raise FilScenarioError(
            f"scenario references unknown CAN message '{entry.message}' on bus '{entry.bus}'"
        )

    def _data(self, linked: LinkedCan, message: LinkedMessage, entry: FilScenarioCan) -> list[int]:
        if entry.raw is not None and entry.values is not None:
            raise FilScenarioError(
                f"CAN entry for '{entry.message}' sets both 'values' and 'raw'; "
                "exactly one is allowed"
            )
        if entry.raw is not None:
            return list(entry.raw)
        payload = _pack_message(linked, message, entry.values or {})
        return list(payload.to_bytes(message.dlc, "little"))

    def _board(self, board: str, node, linked: LinkedCan) -> FilBoard:
        """Build a board config from the node's FIL bus attachments."""
        can = {
            attachment.peripheral: FilBoardCanAttachment(bus=str(bus_id))
            for bus_id, attachment in sorted(
                (
                    (linked.bus_configs[bus_name].bus_id, attachment)
                    for bus_name, attachment in node.busses.items()
                    if bus_name in FIL_BUS_NAMES
                ),
                key=lambda pair: pair[0],
            )
        }

        return FilBoard(
            schema_version=FIL_SCHEMA_VERSION,
            name=board,
            mcu=FIL_MCU_RELATIVE_PATH,
            elf=FIL_ELF_RELATIVE_TEMPLATE.format(board=board),
            vector_base=FIL_VECTOR_BASE,
            can=can,
        )

    def _network(
        self,
        name: str,
        stimuli: list[str],
        buses: dict[str, FilBus],
        boards: list[str],
    ) -> FilNetwork:
        return FilNetwork(
            schema_version=FIL_SCHEMA_VERSION,
            name=name,
            buses=buses,
            boards=boards,
            stimuli=stimuli,
        )

    def _network_buses(self, linked: LinkedCan) -> dict[str, FilBus]:
        """Project the linked bus configs onto the FIL network buses."""
        return {
            str(config.bus_id): FilBus(type="can", bitrate=config.baud_rate)
            for config in linked.bus_configs.values()
            if config.name in FIL_BUS_NAMES
        }


def load_fil_scenario(path) -> FilScenario:
    """Load and validate a hand-maintained FIL scenario file."""
    from core.config import FIL_SCENARIO_DIR

    path = FIL_SCENARIO_DIR / path
    try:
        contents = path.read_bytes()
    except OSError as error:
        raise FilScenarioError(f"could not read FIL scenario {path}: {error}") from error
    try:
        return FilScenario.model_validate_json(contents)
    except ValidationError as error:
        raise FilScenarioError(
            f"invalid FIL scenario {path.name}: {error.errors(include_url=False)}"
        ) from error


def _pack_message(linked: LinkedCan, message: LinkedMessage, values: dict) -> int:
    """Pack physical signal values into a little-endian payload bitfield."""
    known = {signal.signal_name for signal in message.signals}
    for name in values:
        if name not in known:
            raise FilScenarioError(
                f"message '{message.message_name}' has no signal named '{name}'"
            )
    payload = 0
    for signal in message.signals:
        raw = 0
        if signal.signal_name in values:
            raw = _raw_signal(linked, message, signal, values)
        payload |= raw << signal.bit_shift
    return payload


def _raw_signal(linked: LinkedCan, message: LinkedMessage, signal, values: dict) -> int:
    """Pack one signal's physical value, mirroring the generated PACK macros."""
    value = values[signal.signal_name]
    if isinstance(value, str):
        value = _choice_value(linked, message, signal, value)
    offset = signal.offset or 0.0
    scale = signal.scale or 1.0
    pack_coeff = 1.0 / scale  # mirrors PACK_COEFF_<MSG>_<SIG>
    raw = int((value - offset) * pack_coeff)  # C-style truncating cast
    if signal.is_signed and raw < 0:
        raw += 1 << signal.length
    raw &= signal.mask
    if signal.byte_order == "big_endian" and signal.length in (16, 32, 64):
        raw = int.from_bytes(raw.to_bytes(signal.length // 8, "little"), "big")
    return raw


def _choice_value(linked: LinkedCan, message: LinkedMessage, signal, name: str) -> int:
    """Resolve an enum choice name to its raw value via the custom type."""
    custom_type = linked.custom_types.get(signal.data_type)
    if custom_type is None or not custom_type.choices:
        raise FilScenarioError(
            f"signal '{signal.signal_name}' in message '{message.message_name}' "
            f"has data type '{signal.data_type}', which has no enum choices"
        )
    for choice in custom_type.choices:
        if choice.name == name:
            return choice.value
    available = [choice.name for choice in custom_type.choices]
    raise FilScenarioError(
        f"unknown enum choice '{name}' for signal '{signal.signal_name}' "
        f"in message '{message.message_name}' (choices: {available})"
    )


def _dump_json(model) -> str:
    """Serialize a FIL model with the golden files' 2-space layout."""
    return json.dumps(model.model_dump(), indent=JSON_INDENT) + "\n"


def _dump_stimulus(stimulus: FilStimulus) -> str:
    """Serialize the resolved stimulus with the golden file's exact layout.

    The golden groups events by type with a blank line between groups;
    plain json.dumps cannot express that, so events are emitted line-by-line.
    """
    lines = ["{", f'  "schema_version": {stimulus.schema_version},']
    lines.append(f'  "name": {json.dumps(stimulus.name)},')
    lines.append('  "events": [')
    event_lines = [json.dumps(event, indent=None) for event in stimulus.events]
    previous_type = None
    for i, line in enumerate(event_lines):
        entry_type = json.loads(line)["type"]
        if previous_type is not None and entry_type != previous_type:
            lines.append("")
        lines.append(f"    {line},")
        previous_type = entry_type
        if i == len(event_lines) - 1:
            lines[-1] = lines[-1].rstrip(",")
    lines.append("  ],")
    lines.append('  "expect": [')
    expect_lines = [json.dumps(expect) for expect in stimulus.expect]
    lines.extend(f"    {line}," for line in expect_lines[:-1])
    lines.append(f"    {expect_lines[-1]}")
    lines.extend(["  ]", "}", ""])
    return "\n".join(lines)
