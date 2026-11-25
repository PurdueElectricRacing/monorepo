#!/usr/bin/env python3
"""Build FIL and validate the real PER vehicle firmware without hardware."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
CONFIGS = Path(__file__).resolve().parent
FIL_SOURCE = ROOT / "tests" / "external" / "fil"
DEFAULT_FIL = FIL_SOURCE / "build" / "fil"
BUILD_JOBS = os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or str(os.cpu_count() or 1)

BOARDS = (
    "dashboard",
    "main_module",
    "torque_vector",
    "a_box",
    "front_driveline",
    "rear_driveline",
)
SHARED_CAN_BUSES = ("0", "1")


def print_section(name: str) -> None:
    """Print a visible heading before a test phase."""
    print(f"\n{'=' * 80}\n{name}\n{'=' * 80}", flush=True)


def simulate(
    executable: Path,
    config: Path,
    duration_ms: int,
    *,
    network: bool = False,
    trace: Path | None = None,
) -> None:
    """Run one FIL simulation."""
    command = [
        str(executable),
        "run",
        str(config),
        "--duration",
        f"{duration_ms}ms",
        "--strict-mmio",
    ]
    if network:
        command.extend(["--quantum", "512"])
    if trace is not None:
        command.extend(["--trace", str(trace)])

    subprocess.run(command, cwd=ROOT, check=True, timeout=600)


def verify_shared_can_traffic(trace: Path) -> None:
    """Assert firmware-originated CAN TX and RX occurred on shared vehicle buses."""
    events = [json.loads(line) for line in trace.read_text().splitlines()]
    for kind in ("can_tx", "can_rx"):
        has_event = any(
            event.get("type") == kind
            and event.get("bus") in SHARED_CAN_BUSES
            and event.get("origin", "").split(".")[0] in BOARDS
            for event in events
        )
        if not has_event:
            raise RuntimeError(f"No firmware-originated shared-bus {kind} events")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="build FIL before running tests")
    parser.add_argument("--fil", type=Path, default=DEFAULT_FIL, help="path to the FIL executable")
    args = parser.parse_args()

    if args.build:
        print_section("Build FIL")
        subprocess.run(
            [
                "make",
                "-C",
                str(FIL_SOURCE),
                "build",
                "TESTS=OFF",
                "DOCS=OFF",
                f"JOBS={BUILD_JOBS}",
            ],
            cwd=ROOT,
            check=True,
        )

    fil = args.fil.resolve()

    print_section("Single-board boot tests (10 ms)")
    for board in BOARDS:
        simulate(fil, CONFIGS / "boards" / f"{board}.json", 10)

    print_section("Network CAN smoke test (20 ms)")
    with tempfile.TemporaryDirectory(prefix="per-fil-") as directory:
        trace = Path(directory) / "network.jsonl"
        simulate(fil, CONFIGS / "network.json", 20, network=True, trace=trace)
        verify_shared_can_traffic(trace)

    print_section("Ready-to-drive integration test (6 s)")
    simulate(fil, CONFIGS / "ready_to_drive.json", 6000, network=True)

    print("\nPER FIL tests passed")


if __name__ == "__main__":
    main()
