"""
api.py

FilGenerator emits the FIL board and network JSON files under
tests/fil/ as derived artifacts of the linked CAN graph.

The FIL network is a fixed projection of the PER vehicle network: only
the VCAN and MCAN logical buses participate, and the six vehicle boards
are built from the node peripheral mappings. DRIVELINE physically
splits into two boards (front and rear), so the board-to-node mapping
is explicit rather than derived.
"""

import json

from canpiler.pipeline.models import LinkedCan
from core.artifacts import Artifact
from core.utils import print_as_ok, print_as_success
from .models import FilBoard, FilBoardCanAttachment, FilBus, FilNetwork

# The logical buses that make up the FIL network, in the fixed board set.
# All other logical buses (e.g. DASHBOARD's SCAN, A_BOX's CCAN) are
# deliberately excluded from board and network emission.
FIL_BUS_NAMES = frozenset({"VCAN", "MCAN"})

# The six PER vehicle boards emitted to FIL, in network-list order.
# DRIVELINE drives both the front and rear driveline boards; every other
# board name is the lowercase form of its node name.
FIL_BOARD_NAMES = (
    "dashboard",
    "main_module",
    "torque_vector",
    "a_box",
    "front_driveline",
    "rear_driveline",
)

# Board name -> node name. The DRIVELINE node is deliberately mapped to
# two boards because its physical hardware splits front and rear; both
# boards share its VCAN peripheral attachment.
FIL_BOARD_TO_NODE = {
    "dashboard": "DASHBOARD",
    "main_module": "MAIN_MODULE",
    "torque_vector": "TORQUE_VECTOR",
    "a_box": "A_BOX",
    "front_driveline": "DRIVELINE",
    "rear_driveline": "DRIVELINE",
}

FIL_SCHEMA_VERSION = 1
FIL_MCU_FILENAME = "stm32g474retx.json"
FIL_MCU_RELATIVE_PATH = f"../../external/fil/configs/mcus/{FIL_MCU_FILENAME}"
FIL_ELF_RELATIVE_TEMPLATE = "../../../firmware/output/{board}/{board}.elf"
FIL_VECTOR_BASE = "0x08000000"

VEHICLE_NETWORK_NAME = "per_vehicle"
READY_TO_DRIVE_NETWORK_NAME = "per_ready_to_drive"

VEHICLE_NETWORK_FILE = "network.json"
READY_TO_DRIVE_FILE = "ready_to_drive.json"
# Filgen does not yet resolve stimuli from scenarios; ready_to_drive.json
# keeps referencing the checked-in stimuli file until that lands.
READY_TO_DRIVE_STIMULI_FILE = "ready_to_drive_stimuli.json"

JSON_INDENT = 2


class FilGenerator:
    """Generates the FIL board and network config artifacts."""

    def generate(self, linked: LinkedCan, version: str) -> list[Artifact]:
        nodes = {node.name: node for node in linked.nodes}

        for node_name in FIL_BOARD_TO_NODE.values():
            if node_name not in nodes:
                raise ValueError(
                    f"FIL board node '{node_name}' missing from linked CAN"
                )

        artifacts = [
            Artifact(
                "fil",
                f"boards/{board}.json",
                _dump_json(self._board(board, nodes[FIL_BOARD_TO_NODE[board]], linked)),
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

        for artifact in artifacts:
            print_as_ok(f"Generated {artifact.relative_path}")
        print_as_success("FIL board and network files generated")
        return artifacts

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


def _dump_json(model) -> str:
    """Serialize a FIL model with the golden files' 2-space layout."""
    return json.dumps(model.model_dump(), indent=JSON_INDENT) + "\n"
