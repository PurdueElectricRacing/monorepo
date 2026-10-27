#!/usr/bin/env python3
"""Build FIL with PGO and validate the real PER vehicle firmware without hardware.

The FIL configs (boards/, network.json, ready_to_drive.json, and the
stimulus) are generated; run `python3 generators/generate.py` after
changing tests/fil/scenarios/ or generators/.
"""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
CONFIGS = Path(__file__).resolve().parent
FIL_SOURCE = ROOT / "tests/external/fil"
BUILD = FIL_SOURCE
BOARDS = (
    "dashboard", "main_module", "torque_vector", "a_box",
    "front_driveline", "rear_driveline",
)


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print(f"$ {shlex.join(command)}", flush=True)
    try:
        return subprocess.run(command, cwd=ROOT, check=True, timeout=600, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end="")
        if error.stderr:
            print(error.stderr, end="")
        raise


def build() -> None:
    """Use FIL's complete instrument/train/merge/build/test workflow."""
    training = shlex.join([
        "run-network", str(CONFIGS / "network.json"),
        "--duration-ms", "1000", "--max-instructions", "50000000",
        "--quantum", "1024", "--strict-mmio", "--adc-decimation", "1",
    ])
    run([
        "make", "-C", str(FIL_SOURCE), "pgo", "DOCS=OFF",
        f"PGO_GENERATE_BUILD_DIR={BUILD / 'build-pgo-generate'}",
        f"PGO_BUILD_DIR={BUILD / 'build-pgo'}",
        f"PGO_TRAIN_ARGS={training}",
        f"JOBS={os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL') or os.cpu_count() or 1}",
    ])


def simulate(executable: Path, config: Path, duration_ms: int,
             trace: Path | None = None) -> None:
    command = [
        str(executable), "run-network" if config.parent == CONFIGS else "run",
        str(config), "--duration-ms", str(duration_ms),
        "--strict-mmio", "--adc-decimation", "1",
    ]
    if config.parent == CONFIGS:
        command.extend(["--quantum", "512", "--no-detect-spin"])
    if trace is not None:
        command.extend(["--trace", str(trace)])
    result = run(command, capture_output=True, text=True)
    print(result.stdout, end="")
    print(result.stderr, end="")
    if "stop: time-budget" not in result.stdout:
        raise RuntimeError(f"{config.name} did not reach the requested time budget")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="build FIL with PGO first")
    parser.add_argument("--fil", type=Path, default=BUILD / "build-pgo/fil")
    args = parser.parse_args()
    for board in BOARDS:
        elf = ROOT / f"firmware/output/{board}/{board}.elf"
        if not elf.is_file():
            raise FileNotFoundError(f"{elf}: run python3 firmware/build_firmware.py first")
    if args.build:
        build()
    for board in BOARDS:
        simulate(args.fil.resolve(), CONFIGS / f"boards/{board}.json", 10)
    with tempfile.TemporaryDirectory(prefix="per-fil-") as directory:
        trace = Path(directory) / "network.jsonl"
        simulate(args.fil.resolve(), CONFIGS / "network.json", 20, trace)
        events = [json.loads(line) for line in trace.read_text().splitlines()]
        for kind in ("can_tx", "can_rx"):
            if not any(event.get("type") == kind and
                       event.get("bus") in ("0", "1") and
                       event.get("origin", "").split(".")[0] in BOARDS for event in events):
                raise RuntimeError(f"No firmware-originated shared-bus {kind} events")
    # FIL returns failure if the stimulus expectations are unmet.
    simulate(args.fil.resolve(), CONFIGS / "ready_to_drive.json", 6000)
    print("PER FIL tests passed")


if __name__ == "__main__":
    main()
