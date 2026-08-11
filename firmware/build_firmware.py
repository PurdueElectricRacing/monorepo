#!/usr/bin/python3
"""Build firmware and optionally package G4 bootloader applications."""

# Wrapper for command line tools to build, clean, and debug firmware modules
from optparse import OptionParser
import gzip
import json
import os
import pathlib
import subprocess
import sys
import tarfile

# Logging helper functions
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def log_error(phrase):
    print(f"{bcolors.FAIL}ERROR: {phrase}{bcolors.ENDC}")

def log_success(phrase):
    print(f"{bcolors.OKGREEN}{phrase}{bcolors.ENDC}")

# Complete package order. Driveline source produces two physical board images.
BOARD_TARGETS = [
    "main_module",
    "dashboard",
    "torque_vector",
    "a_box",
    "front_driveline",
    "rear_driveline",
]

# Manifest copies of IDs defined by can_library/configs. DaqApp independently
# verifies every board/ID tuple, so configuration drift fails before CAN writes.
BOOTLOADER_PROTOCOL_IDS = {
    "main_module": {"command_id": "0x180", "data_id": "0x181", "response_id": "0x182"},
    "dashboard": {"command_id": "0x183", "data_id": "0x184", "response_id": "0x185"},
    "torque_vector": {"command_id": "0x186", "data_id": "0x187", "response_id": "0x188"},
    "a_box": {"command_id": "0x189", "data_id": "0x18A", "response_id": "0x18B"},
    "front_driveline": {"command_id": "0x18C", "data_id": "0x18D", "response_id": "0x18E"},
    "rear_driveline": {"command_id": "0x18F", "data_id": "0x190", "response_id": "0x191"},
}

PACKAGE_FORMAT = "per-firmware-package-v1"
STM32_CRC_INIT = 0xFFFFFFFF

# Resolve paths relative to this script, not the caller's working directory.
CWD = pathlib.Path(__file__).resolve().parent
BUILD_DIR = CWD / "build"
SOURCE_DIR = CWD
OUT_DIR = CWD / "output"
CAN_GEN_DIR = SOURCE_DIR / "can_library" / "generated"

# Setup cli arguments
parser = OptionParser()

parser.add_option("-t", "--target",
    type="string",
    help="Space-separated list of firmware targets to build"
)

parser.add_option("-l", "--list",
    action="store_true", default=False,
    help="List boards targets available to build"
)

parser.add_option("-b", "--bootloader",
    dest="bootloader",
    action="store_true", default=False,
    help="build bootloaders and use the bootloader application layout"
)

parser.add_option("-v", "--verbose",
    dest="verbose",
    action="store_true", default=False,
    help="verbose build command output"
)

parser.add_option("-p", "--package",
    dest="package",
    action="store_true", default=False,
    help="build six VCAN G4 applications and package them with STM32 CRCs"
)

parser.add_option("-c", "--check",
    dest="check",
    action="store_true", default=False,
    help="run cppcheck static analysis instead of building"
)

def print_available_targets():
    modules = [
        "main_module",
        "bootloader",
        "f4_testing",
        "g4_testing",
        "a_box",
        "torque_vector",
        "dashboard",
        "pdu",
        "daq",
        "driveline"
    ]
    modules_sorted = sorted(modules)
    print("Available targets to build:")
    for m in modules_sorted:
        print(f'\t{m}')

def run_cppcheck():
    compile_db = BUILD_DIR/"compile_commands.json"
    if not compile_db.exists():
        log_error(f"compile_commands.json not found at {compile_db}. Please run the build first.")
        sys.exit(1)

    cppcheck_build_dir = SOURCE_DIR/".cache"/"cppcheck"
    cppcheck_build_dir.mkdir(parents=True, exist_ok=True)

    cppcheck_command = [
        "cppcheck",
        f"--project={compile_db}",
        "--enable=warning,style,performance,portability",
        "--check-level=exhaustive",
        "--max-ctu-depth=4",
        "--safety",
        f"--cppcheck-build-dir={cppcheck_build_dir}",
        "-D__GNUC__",
        "--platform=unix32",
        "--funsigned-char",
        "--file-filter=*/firmware/source/*",
        "--file-filter=*/firmware/common/*",
        "--file-filter=*/firmware/can_library/*",
        "--suppress=*:*/external/*",
        "--suppress=*:*/source/torque_vector/vcu/*",
        "--suppress=*:*/common/phal_F4/*", # it sucks but purposely ignoring
        "--suppress=preprocessorErrorDirective",
        "--inline-suppr",
        "--quiet",
        f"-j{os.cpu_count() or 1}",
        f"--error-exitcode=1"
    ]

    print(f"Running cppcheck command: {' '.join(cppcheck_command)}")
    result = subprocess.run(cppcheck_command)
    if result.returncode != 0:
        log_error("cppcheck found issues in the code. Please review the output above.")
        sys.exit(result.returncode)
    log_success("cppcheck completed successfully.")

(options, _) = parser.parse_args()
if options.package:
    if options.target:
        parser.error("--package does not accept --target")
    options.bootloader = True
if options.list:
    # User ran `-t` with no argument: print available targets
    print_available_targets()
    sys.exit(0)

# Map physical driveline targets back to their shared CMake source module.
if options.target:
    target_list = options.target.split()
    if options.bootloader and any(target not in BOARD_TARGETS for target in target_list):
        parser.error("bootloader builds support only G4 VCAN applications")
    cmake_modules = [
        "driveline" if target in {"front_driveline", "rear_driveline"} else target
        for target in target_list
    ]
    cmake_modules_str = ";".join(dict.fromkeys(cmake_modules))
    ninja_targets = [f"{target}.elf" for target in target_list]
elif options.bootloader:
    cmake_modules_str = "main_module;dashboard;torque_vector;a_box;driveline"
    ninja_targets = [f"{board}.elf" for board in BOARD_TARGETS]
    if not options.package:
        ninja_targets.append("bootloader.elf")
else:
    cmake_modules_str = ""
    ninja_targets = ["all"]

# Always clean for a reproducible package/build environment. In particular,
# stale HEX files must not be mistaken for a board selected by --package.
subprocess.run(
    ["cmake", "-E", "rm", "-rf", str(BUILD_DIR), str(OUT_DIR), str(CAN_GEN_DIR)],
    check=True,
)
print("Build, output, and generated CAN directories clean.")

# Configure and Build
CMAKE_OPTIONS = [
    "-S", str(SOURCE_DIR),
    "-B", str(BUILD_DIR),
    "-G", "Ninja",
    f"-DBOOTLOADER_BUILD={'ON' if options.bootloader else 'OFF'}",
    f"-DMODULES={cmake_modules_str}"
]

NINJA_OPTIONS = ["-C", str(BUILD_DIR)] + ninja_targets
NINJA_COMMAND = ["ninja"] + NINJA_OPTIONS

try:
    subprocess.run(["cmake"] + CMAKE_OPTIONS, check=True)
except subprocess.CalledProcessError:
    log_error("Unable to configure CMake, see the CMake output above.")
    sys.exit(1)

log_success("Sucessfully generated build files.")

if options.check:
    run_cppcheck()
    sys.exit(0)

print(f"Running Build command {' '.join(NINJA_COMMAND)}")

try:
    ninja_build = subprocess.run(NINJA_COMMAND)
except subprocess.CalledProcessError:
    log_error("Unable to configure compile sources, see the Ninja output above.")
    sys.exit(1)

if ninja_build.returncode != 0:
    log_error("Unable to generate targets.")
    sys.exit(1)
else:
    log_success("Sucessfully built targets.")

# Package helpers.
def get_git_hash_or_tag():
    """Return a stable package suffix: exact tag, otherwise short commit ID."""
    try:
        # Check if current commit has a tag
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--exact-match"],
            stderr=subprocess.DEVNULL
        ).strip().decode()
        return tag
    except subprocess.CalledProcessError:
        # No tag on this commit, fallback to short hash
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"]
        ).strip().decode()

def stm32_crc32_words(data: bytes) -> int:
    """Match ``PHAL_CRC_calculate()`` on STM32G4 exactly.

    The hardware consumes 32-bit words from memory. Package bytes therefore
    use little-endian word interpretation, the MPEG-2 polynomial, an all-one
    initial value, and no reflection/final XOR. Padding is erased-flash ``0xFF``
    and is part of the CRC when the input is not word aligned.
    """
    if len(data) % 4:
        data += b"\xff" * (4 - len(data) % 4)

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


def selected_boards() -> list[str]:
    """Require the complete six-board package produced by the package build."""
    missing = [
        board for board in BOARD_TARGETS
        if not (OUT_DIR / board / f"{board}.bin").exists()
    ]
    if missing:
        raise RuntimeError(f"missing application binaries: {', '.join(missing)}")
    return BOARD_TARGETS


def build_manifest(boards: list[str]) -> pathlib.Path:
    """Copy CMake-generated binaries and write their validated manifest."""
    images_dir = OUT_DIR / "images"
    images_dir.mkdir(parents=True, exist_ok=True)
    manifest_boards = []

    for board in boards:
        source = OUT_DIR / board / f"{board}.bin"
        data = source.read_bytes()
        if not data or len(data) % 4 or len(data) > 160 * 1024:
            raise RuntimeError(f"invalid bootloader image size for {board}: {len(data)}")
        destination = images_dir / f"{board}.bin"
        destination.write_bytes(data)
        crc = stm32_crc32_words(data)
        (OUT_DIR / board / f"{board}.crc").write_text(
            f"0x{crc:08X}\n", encoding="ascii"
        )

        manifest_boards.append({
            "name": board,
            "binary": f"images/{board}.bin",
            "size_bytes": len(data),
            "crc32": f"0x{crc:08X}",
            "application_address": "0x08008000",
            "can_bus": "VCAN",
            **BOOTLOADER_PROTOCOL_IDS[board],
        })
        log_success(f"Packaged {board}: {len(data)} bytes, STM32 CRC 0x{crc:08X}")

    manifest = {
        "format": PACKAGE_FORMAT,
        "protocol_version": 1,
        "crc_algorithm": "STM32_CRC32_MPEG2_WORD_LE",
        "can_bus": "VCAN",
        "boards": manifest_boards,
    }
    manifest_path = OUT_DIR / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    log_success(f"Manifest written: {manifest_path}")
    return manifest_path


def _deterministic_tarinfo(tarinfo: tarfile.TarInfo) -> tarfile.TarInfo:
    """Remove host ownership and timestamps from one archive entry."""
    tarinfo.uid = 0
    tarinfo.gid = 0
    tarinfo.uname = ""
    tarinfo.gname = ""
    tarinfo.mtime = 0
    return tarinfo


def create_tarball(manifest_path: pathlib.Path, boards: list[str]) -> pathlib.Path:
    """Create a reproducible archive with normalized tar and gzip metadata."""
    git_hash = get_git_hash_or_tag()
    tarball_name = OUT_DIR / f"firmware_{git_hash}.tar.gz"
    with tarball_name.open("wb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as tar:
                tar.add(manifest_path, arcname="manifest.json", filter=_deterministic_tarinfo)
                for board in boards:
                    binary_path = OUT_DIR / "images" / f"{board}.bin"
                    tar.add(binary_path, arcname=f"images/{board}.bin", filter=_deterministic_tarinfo)
                    hex_path = OUT_DIR / board / f"{board}.hex"
                    tar.add(hex_path, arcname=f"hex/{board}.hex", filter=_deterministic_tarinfo)
    log_success(f"Tarball created: {tarball_name}")
    return tarball_name

# Package output if requested
if options.package:
    log_success("Packaging firmware...")
    package_boards = selected_boards()
    package_manifest = build_manifest(package_boards)
    create_tarball(package_manifest, package_boards)
