#!/usr/bin/env python3
"""Build FIL with PGO and validate the real PER vehicle firmware without hardware."""

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
CONFIGS = Path(__file__).resolve().parent
FIL_SOURCE = ROOT / "tests/external/fil"
BUILD = FIL_SOURCE


@dataclass(frozen=True)
class NetworkSummary:
    duration_ms: int
    has_expectations: bool


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


def read_config(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def discover_networks() -> list[Path]:
    """Return every FIL network config in this directory, sorted by name."""
    networks = []
    for path in sorted(CONFIGS.glob("*.json")):
        data = read_config(path)
        if isinstance(data, dict) and "boards" in data and "buses" in data:
            networks.append(path)
    return networks


def require_firmware(board_configs: set[Path]) -> None:
    for config in sorted(board_configs):
        board = read_config(config)
        elf = (config.parent / board["elf"]).resolve()
        if not elf.is_file():
            raise FileNotFoundError(
                f"{elf}: run python3 firmware/build_firmware.py first"
            )


def event_end_ms(event: dict) -> int:
    end_ms = event.get("at_ms", 0)
    repeat = event.get("repeat")
    if repeat:
        interval_ms = repeat.get("every_ms", 0)
        count = repeat.get("count", 1)
        end_ms += interval_ms * (count - 1)
    return end_ms


def network_summary(path: Path, idle_ms: int, settle_ms: int) -> NetworkSummary:
    """Cover every scheduled event and expectation, plus time to settle."""
    network = read_config(path)
    end_ms = 0
    has_expectations = False

    for name in network.get("stimuli", []):
        stimulus = read_config(path.parent / name)
        for event in stimulus.get("events", []):
            end_ms = max(end_ms, event_end_ms(event))

        for expectation in stimulus.get("expect", []):
            has_expectations = True
            window_start_ms = expectation.get("at_ms", 0)
            window_end_ms = window_start_ms + expectation.get("window_ms", 0)
            end_ms = max(end_ms, window_end_ms)

    duration_ms = end_ms + settle_ms if end_ms else idle_ms
    return NetworkSummary(duration_ms, has_expectations)


def simulate(
    executable: Path,
    config: Path,
    duration_ms: int,
    *,
    network: bool = False,
    trace: Path | None = None,
) -> subprocess.CompletedProcess:
    command = [
        str(executable),
        "run-network" if network else "run",
        str(config),
        "--duration-ms", str(duration_ms),
        "--strict-mmio",
        "--adc-decimation", "1",
    ]
    if network:
        command.extend(["--quantum", "512", "--no-detect-spin"])
    if trace is not None:
        command.extend(["--trace", str(trace)])

    result = run(command, capture_output=True, text=True)
    print(result.stdout, end="")
    print(result.stderr, end="")

    if ": fail" in result.stdout or ": incomplete" in result.stdout:
        raise RuntimeError(f"{config.name} had unmet expectations")
    if "stop: time-budget" not in result.stdout.splitlines():
        raise RuntimeError(f"{config.name} did not reach the requested time budget")
    return result


def check_final_pcs(config: Path, output: str, expected: dict[str, str]) -> None:
    # The pinned FIL CLI exposes final registers only in its text summary.
    board_summary = r"^board (?P<name>[^:]+):.*\bpc=(?P<pc>0x[0-9a-fA-F]+)\b"
    actual = {
        match["name"]: int(match["pc"], 16)
        for match in re.finditer(board_summary, output, re.MULTILINE)
    }

    for name, address in expected.items():
        expected_pc = int(address, 0)
        actual_pc = actual.get(name)
        if actual_pc != expected_pc:
            observed = "missing" if actual_pc is None else f"0x{actual_pc:08X}"
            raise RuntimeError(
                f"{config.name}: {name} final PC was {observed}, "
                f"expected 0x{expected_pc:08X}"
            )


def check_network_traffic(
    events: list[dict], board_names: set[str], buses: list[str]
) -> None:
    for kind in ("can_tx", "can_rx"):
        firmware_traffic = any(
            event.get("type") == kind
            and event.get("bus") in buses
            and event.get("origin", "").split(".")[0] in board_names
            for event in events
        )
        if not firmware_traffic:
            raise RuntimeError(f"No firmware-originated shared-bus {kind} events")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="build FIL with PGO first")
    parser.add_argument("--fil", type=Path, default=BUILD / "build-pgo/fil")
    args = parser.parse_args()

    settings = read_config(CONFIGS / "runner.json")
    boards = [CONFIGS / name for name in settings["boards"]]
    networks = discover_networks()
    board_configs = set(boards)
    for network in networks:
        board_configs.update(
            network.parent / name for name in read_config(network)["boards"]
        )
    require_firmware(board_configs)

    final_pcs = settings["final_pcs"]
    missing_networks = final_pcs.keys() - {path.name for path in networks}
    if missing_networks:
        raise ValueError(f"Final-PC checks reference missing networks: {missing_networks}")

    if args.build:
        build()

    executable = args.fil.resolve()
    for board in boards:
        simulate(executable, board, settings["board_duration_ms"])

    board_names = {read_config(board)["name"] for board in boards}
    for network in networks:
        summary = network_summary(
            network,
            settings["network_idle_duration_ms"],
            settings["network_settle_ms"],
        )
        with tempfile.TemporaryDirectory(prefix="per-fil-") as directory:
            trace = None
            if not summary.has_expectations:
                trace = Path(directory) / f"{network.stem}.jsonl"

            result = simulate(
                executable, network, summary.duration_ms, network=True, trace=trace
            )
            check_final_pcs(network, result.stdout, final_pcs.get(network.name, {}))

            if trace is not None:
                events = [json.loads(line) for line in trace.read_text().splitlines()]
                check_network_traffic(events, board_names, settings["traffic_buses"])

    print("PER FIL tests passed")


if __name__ == "__main__":
    main()
