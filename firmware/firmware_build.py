#!/usr/bin/python3

# Wrapper for command line tools to build, clean, and debug firmware modules
from optparse import OptionParser
import os
import pathlib
import subprocess
import sys
import tarfile
import zlib

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

def log_warning(phrase):
    print(f"{bcolors.WARNING}WARNING: {phrase}{bcolors.ENDC}")

def log_success(phrase):
    print(f"{bcolors.OKGREEN}{phrase}{bcolors.ENDC}")

BOARD_TARGETS = [
        "main_module",
        "a_box",
        "torque_vector",
        "dashboard",
        "pdu",
        "daq",
        "front_driveline",
        "rear_driveline"
    ]


# Get build directory path
CWD = pathlib.Path.cwd()
BUILD_DIR = CWD/"build"
SOURCE_DIR = CWD
OUT_DIR = CWD/"output"
CAN_GEN_DIR = SOURCE_DIR/"can_library"/"generated"

# Setup cli arguments
parser = OptionParser()

parser.add_option("-t", "--target",
    type="string",
    help="Space-separated list of boards targets to build"
)

parser.add_option("-l", "--list",
    action="store_true", default=False,
    help="List boards targets available to build"
)

parser.add_option("-b", "--bootloader",
    dest="bootloader",
    action="store_true", default=False,
    help="build bootloader components"
)

parser.add_option("-v", "--verbose",
    dest="verbose",
    action="store_true", default=False,
    help="verbose build command output"
)

parser.add_option("-p", "--package",
    dest="package",
    action="store_true", default=False,
    help="package build output into tarball with CRCs, suffixed by Git hash"
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

    cppcheck_command = [
        "cppcheck",
        f"--project={compile_db}",
        "--enable=warning,style,performance,portability",
        "--file-filter=*/firmware/source/*",
        "--file-filter=*/firmware/common/*",
        "--suppress=*:*/firmware/source/torque_vector/vcu/*",
        "--suppress=preprocessorErrorDirective",
        "--inline-suppr",
        f"-j{os.cpu_count() or 1}",
        f"--error-exitcode=1"
    ]

    print(f"Running cppcheck command: {' '.join(cppcheck_command)}")
    result = subprocess.run(cppcheck_command)
    if result.returncode != 0:
        log_error("cppcheck found issues in the code. Please review the output above.")
        sys.exit(result.returncode)
    log_success("cppcheck completed successfully.")

(options, args) = parser.parse_args()
if options.list:
    # User ran `-t` with no argument: print available targets
    print_available_targets()
    sys.exit(0)

VERBOSE = "--verbose" if options.verbose else ""

# Prepare MODULES string for CMake
if options.target:
    target_list = options.target.split()
    cmake_modules_str = ";".join(target_list)
    ninja_targets = [t + ".elf" for t in target_list]
else:
    cmake_modules_str = ""
    ninja_targets = ["all"]

# Always clean for a fresh build environment
subprocess.run(["cmake", "-E", "rm", "-rf", str(BUILD_DIR), str(OUT_DIR), str(CAN_GEN_DIR)])
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
except subprocess.CalledProcessError as e:
    log_error("Unable to configure CMake, see the CMake output above.")
    sys.exit(1)

log_success("Sucessfully generated build files.")

if options.check:
    run_cppcheck()
    sys.exit(0)

print(f"Running Build command {' '.join(NINJA_COMMAND)}")

try:
    ninja_build = subprocess.run(NINJA_COMMAND)
except subprocess.CalledProcessError as e:
    log_error("Unable to configure compile sources, see the Ninja output above.")
    sys.exit(1)

if ninja_build.returncode != 0:
    log_error("Unable to generate targets.")
    sys.exit(1)
else:
    log_success("Sucessfully built targets.")

# package logic
def get_git_hash_or_tag():
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

def add_crc_to_files():
    if not OUT_DIR.exists():
        log_error(f"Output directory does not exist: {OUT_DIR}")
        sys.exit(1)
    for board in BOARD_TARGETS:
        hex_path = OUT_DIR / board / f"{board}.hex"
        if hex_path.exists():
            with open(hex_path, "rb") as f:
                data = f.read()
                crc = format(zlib.crc32(data) & 0xFFFFFFFF, '08X')
            crc_file = hex_path.with_suffix(".crc")
            with open(crc_file, "w") as cf:
                cf.write(crc + "\n")
            log_success(f"CRC written for {hex_path.name}: {crc}")
        else:
            print(f"[WARNING] HEX not found for {board}: {hex_path}")

def create_tarball():
    git_hash = get_git_hash_or_tag()
    tarball_name = OUT_DIR / f"firmware_{git_hash}.tar.gz"

    with tarfile.open(tarball_name, "w:gz") as tar:
        for board in BOARD_TARGETS:
            hex_path = OUT_DIR / board / f"{board}.hex"
            crc_path = OUT_DIR / board / f"{board}.crc"

            if hex_path.exists():
                tar.add(hex_path, arcname=f"{board}.hex")
                log_success(f"Added {hex_path.name} to tarball.")

            if crc_path.exists():
                tar.add(crc_path, arcname=f"{board}.crc")
                log_success(f"Added {crc_path.name} to tarball.")

    log_success(f"Tarball created: {tarball_name}")
    return tarball_name

# Package output if requested
if options.package:
    log_success("Packaging firmware...")
    add_crc_to_files()
    create_tarball()
