#!/usr/bin/env python3
"""Build FIL with PGO and validate the real PER vehicle firmware without hardware."""

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
UPDATE_ARTIFACTS = ("bootloader_dashboard",)


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


def discover_networks() -> list[Path]:
    """Return every FIL network config in this directory, sorted by name."""
    networks = []
    for path in sorted(CONFIGS.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(data, dict) and "boards" in data and "buses" in data:
            networks.append(path)
    return networks


def network_summary(path: Path) -> tuple[int, bool]:
    """Return a duration covering every scheduled event and the presence of expectations.

    The duration is derived from the network's stimuli so a scenario only needs
    to declare its events and expectation windows.
    """
    network = json.loads(path.read_text(encoding="utf-8"))
    end_ms = 0
    has_expectation = False
    for name in network.get("stimuli", []):
        stimulus = json.loads((path.parent / name).read_text(encoding="utf-8"))
        for event in stimulus.get("events", []):
            at_ms = event.get("at_ms", 0)
            repeat = event.get("repeat")
            if repeat:
                at_ms += repeat.get("every_ms", 0) * (repeat.get("count", 1) - 1)
            end_ms = max(end_ms, at_ms)
        for expectation in stimulus.get("expect", []):
            has_expectation = True
            end_ms = max(end_ms, expectation.get("at_ms", 0) + expectation.get("window_ms", 0))
    if end_ms == 0:
        return 20, has_expectation
    return end_ms + 500, has_expectation


def simulate_board(executable: Path, config: Path, duration_ms: int) -> None:
    command = [
        str(executable), "run", str(config), "--duration-ms", str(duration_ms),
        "--strict-mmio", "--adc-decimation", "1",
    ]
    result = run(command, capture_output=True, text=True)
    print(result.stdout, end="")
    print(result.stderr, end="")
    if "stop: time-budget" not in result.stdout:
        raise RuntimeError(f"{config.name} did not reach the requested time budget")


def simulate_network(executable: Path, config: Path, duration_ms: int,
                     trace: Path | None = None) -> subprocess.CompletedProcess:
    command = [
        str(executable), "run-network", str(config), "--duration-ms", str(duration_ms),
        "--strict-mmio", "--adc-decimation", "1",
        "--quantum", "512", "--no-detect-spin",
    ]
    if trace is not None:
        command.extend(["--trace", str(trace)])
    print(f"$ {shlex.join(command)}", flush=True)
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=600)
    print(result.stdout, end="")
    print(result.stderr, end="")
    if ": fail" in result.stdout or ": incomplete" in result.stdout:
        raise RuntimeError(f"{config.name} had unmet expectations")
    result.check_returncode()
    if "stop: time-budget" not in result.stdout.splitlines():
        raise RuntimeError(f"{config.name} did not reach the requested time budget")
    return result


def check_network_traffic(events: list[dict]) -> None:
    for kind in ("can_tx", "can_rx"):
        if not any(event.get("type") == kind and
                   event.get("bus") in ("0", "1") and
                   event.get("origin", "").split(".")[0] in BOARDS for event in events):
            raise RuntimeError(f"No firmware-originated shared-bus {kind} events")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="build FIL with PGO first")
    parser.add_argument("--fil", type=Path, default=BUILD / "build-pgo/fil")
    args = parser.parse_args()
    for board in BOARDS:
        elf = ROOT / f"firmware/output/{board}/{board}.elf"
        if not elf.is_file():
            raise FileNotFoundError(f"{elf}: run python3 firmware/build_firmware.py first")
    for artifact in UPDATE_ARTIFACTS:
        elf = ROOT / f"firmware/output/{artifact}/{artifact}.elf"
        if not elf.is_file():
            raise FileNotFoundError(f"{elf}: run python3 firmware/build_firmware.py first")
    if args.build:
        build()
    for board in BOARDS:
        simulate_board(args.fil.resolve(), CONFIGS / f"boards/{board}.json", 10)
    for network in discover_networks():
        duration_ms, has_expectation = network_summary(network)
        if has_expectation:
            simulate_network(args.fil.resolve(), network, duration_ms)
            continue
        # A scenario without expectations is a smoke test; require the network
        # to carry firmware-originated traffic on the shared buses.
        with tempfile.TemporaryDirectory(prefix="per-fil-") as directory:
            trace = Path(directory) / f"{network.stem}.jsonl"
            simulate_network(args.fil.resolve(), network, duration_ms, trace)
            events = [json.loads(line) for line in trace.read_text().splitlines()]
            check_network_traffic(events)
    print("PER FIL tests passed")


if __name__ == "__main__":
    main()
