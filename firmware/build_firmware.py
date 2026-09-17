#!/usr/bin/env python3
"""Build all firmware targets and package the G4 application images."""

from pathlib import Path
import gzip
import json
import shutil
import subprocess
import tarfile


ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "build"
OUTPUT_DIR = ROOT / "output"
CAN_GENERATED_DIR = ROOT / "can_library" / "generated"
CAN_CONFIG_DIR = ROOT.parent / "generators" / "configs"
CAN_NODE_CONFIG_DIR = CAN_CONFIG_DIR / "nodes"
BOOTLOADER_UPDATER_CONFIG = CAN_CONFIG_DIR / "external_nodes" / "BOOTLOADER.json"

BOARD_TARGETS = [
    "main_module",
    "dashboard",
    "torque_vector",
    "a_box",
    "front_driveline",
    "rear_driveline",
]

PACKAGE_FORMAT = "per-firmware-package-v1"
STM32_CRC_INIT = 0xFFFFFFFF
BL_APP_ADDRESS = 0x08008000
BL_APP_SLOT_SIZE = 480 * 1024
ERASED_FLASH_BYTE = 0xFF


def firmware_ref() -> str:
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--exact-match"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    return result.stdout.strip().replace("/", "-")


def stm32_crc32_words(data: bytes) -> int:
    """Match PHAL_CRC_calculate() for little-endian STM32G4 words."""
    data += bytes([ERASED_FLASH_BYTE]) * ((-len(data)) % 4)
    lut = (
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
        0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
        0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
        0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
    )
    crc = STM32_CRC_INIT
    for offset in range(0, len(data), 4):
        crc ^= int.from_bytes(data[offset:offset + 4], "little")
        for _ in range(8):
            crc = ((crc << 4) & 0xFFFFFFFF) ^ lut[(crc >> 28) & 0xF]
    return crc


def verify_application_vector_address(elf_path: Path) -> None:
    objdump = shutil.which("arm-none-eabi-objdump")
    if objdump is None:
        raise RuntimeError("arm-none-eabi-objdump is required for package validation")

    result = subprocess.run(
        [objdump, "-h", "--wide", str(elf_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        details = result.stderr.strip() or result.stdout.strip() or "no diagnostic"
        raise RuntimeError(f"cannot inspect {elf_path}: {details}")

    rows = [
        line.split()
        for line in result.stdout.splitlines()
        if len(line.split()) > 1 and line.split()[1] == ".isr_vector"
    ]
    if len(rows) != 1 or len(rows[0]) < 7:
        raise RuntimeError(f"expected one valid .isr_vector section in {elf_path}")

    fields = rows[0]
    try:
        vector_vma = int(fields[3], 16)
        vector_lma = int(fields[4], 16)
    except ValueError as error:
        raise RuntimeError(f"malformed .isr_vector row in {elf_path}") from error
    if vector_vma != BL_APP_ADDRESS or vector_lma != BL_APP_ADDRESS:
        raise RuntimeError(
            f"{elf_path} .isr_vector starts at 0x{vector_vma:08X}/0x{vector_lma:08X}; "
            f"expected 0x{BL_APP_ADDRESS:08X}"
        )


def configured_bootloader_protocol() -> dict[str, dict[str, str]]:
    updater = json.loads(BOOTLOADER_UPDATER_CONFIG.read_text(encoding="utf-8"))
    updater_buses = updater.get("busses", {})
    protocol = {}

    for board in BOARD_TARGETS:
        node_path = CAN_NODE_CONFIG_DIR / f"BL_{board.upper()}.json"
        node = json.loads(node_path.read_text(encoding="utf-8"))
        node_buses = node.get("busses", {})
        if len(node_buses) != 1:
            raise RuntimeError(f"bootloader config for {board} must define one bus")

        bus, node_bus = next(iter(node_buses.items()))
        if bus not in updater_buses:
            raise RuntimeError(f"updater config does not define bus {bus} for {board}")

        updater_messages = {
            message["message_name"]: message
            for message in updater_buses[bus].get("tx", [])
        }
        node_messages = {
            message["message_name"]: message for message in node_bus.get("tx", [])
        }
        message_sources = {
            "start_id": (updater_messages, f"bl_{board}_start"),
            "crc_id": (updater_messages, f"bl_{board}_crc"),
            "jump_id": (updater_messages, f"bl_{board}_jump"),
            "data_id": (updater_messages, f"bl_{board}_data"),
            "response_id": (node_messages, f"bl_{board}_resp"),
        }

        values = {"can_bus": bus}
        for field, (messages, name) in message_sources.items():
            message = messages.get(name)
            if message is None or "id_override" not in message:
                raise RuntimeError(f"missing explicit ID for Canpiler message {name}")
            try:
                can_id = int(message["id_override"], 0)
            except (TypeError, ValueError) as error:
                raise RuntimeError(f"invalid ID for Canpiler message {name}") from error
            if not 0 <= can_id <= 0x7FF:
                raise RuntimeError(f"Canpiler message {name} must use a standard CAN ID")
            values[field] = f"0x{can_id:X}"
        protocol[board] = values

    return protocol


def build_manifest() -> Path:
    images_dir = OUTPUT_DIR / "images"
    images_dir.mkdir(parents=True, exist_ok=True)
    board_protocol = configured_bootloader_protocol()
    manifest_boards = []

    for board in BOARD_TARGETS:
        elf = OUTPUT_DIR / board / f"{board}.elf"
        verify_application_vector_address(elf)
        source = OUTPUT_DIR / board / f"{board}.bin"
        source_data = source.read_bytes()
        if not source_data or len(source_data) > BL_APP_SLOT_SIZE:
            raise RuntimeError(f"invalid application image size for {board}")

        data = source_data + bytes([ERASED_FLASH_BYTE]) * ((-len(source_data)) % 4)
        if len(data) > BL_APP_SLOT_SIZE:
            raise RuntimeError(f"invalid packaged image size for {board}")

        (images_dir / f"{board}.bin").write_bytes(data)
        crc = stm32_crc32_words(data)
        (images_dir / f"{board}.crc").write_text(f"0x{crc:08X}\n", encoding="ascii")
        manifest_boards.append({
            "name": board,
            "binary": f"images/{board}.bin",
            "size_bytes": len(data),
            "crc32": f"0x{crc:08X}",
            "application_address": f"0x{BL_APP_ADDRESS:08X}",
            **board_protocol[board],
        })

    manifest = {
        "format": PACKAGE_FORMAT,
        "protocol_version": 1,
        "crc_algorithm": "STM32_CRC32_MPEG2_WORD_LE",
        "can_bus": (
            next(iter({values["can_bus"] for values in board_protocol.values()}))
            if len({values["can_bus"] for values in board_protocol.values()}) == 1
            else "MIXED"
        ),
        "boards": manifest_boards,
    }
    path = OUTPUT_DIR / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return path


def normalized_tarinfo(tarinfo: tarfile.TarInfo) -> tarfile.TarInfo:
    tarinfo.uid = 0
    tarinfo.gid = 0
    tarinfo.uname = ""
    tarinfo.gname = ""
    tarinfo.mtime = 0
    return tarinfo


def create_tarball(manifest_path: Path) -> Path:
    tarball = OUTPUT_DIR / f"firmware_{firmware_ref()}.tar.gz"
    with tarball.open("wb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                archive.add(
                    manifest_path,
                    arcname="manifest.json",
                    filter=normalized_tarinfo,
                )
                for board in BOARD_TARGETS:
                    archive.add(
                        OUTPUT_DIR / "images" / f"{board}.bin",
                        arcname=f"images/{board}.bin",
                        filter=normalized_tarinfo,
                    )
                    archive.add(
                        OUTPUT_DIR / "images" / f"{board}.crc",
                        arcname=f"crc/{board}.crc",
                        filter=normalized_tarinfo,
                    )
                    archive.add(
                        OUTPUT_DIR / board / f"{board}.hex",
                        arcname=f"hex/{board}.hex",
                        filter=normalized_tarinfo,
                    )
    return tarball


def build() -> None:
    subprocess.run(
        [
            "cmake",
            "-E",
            "rm",
            "-rf",
            str(BUILD_DIR),
            str(OUTPUT_DIR),
            str(CAN_GENERATED_DIR),
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(BUILD_DIR),
            "-G",
            "Ninja",
            "-DBOOTLOADER_BUILD=ON",
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(["ninja", "-C", str(BUILD_DIR), "all"], cwd=ROOT, check=True)
    create_tarball(build_manifest())


if __name__ == "__main__":
    build()
