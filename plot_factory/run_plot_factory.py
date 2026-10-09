"""Command-line entrypoint for telemetry analysis."""

import argparse
import sys
from pathlib import Path


if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = "plot_factory"

from .pipeline import DATA_DIRECTORY, run_analysis


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate telemetry plots from a CSV folder."
    )
    parser.add_argument(
        "data_directory",
        nargs="?",
        type=Path,
        default=DATA_DIRECTORY,
        help="folder containing telemetry CSV files (default: plot_factory/data/)",
    )

    arguments = parser.parse_args(argv)

    try:
        result = run_analysis(arguments.data_directory)
    except Exception as error:
        print(f"Analysis failed: {error}", file=sys.stderr)
        return 1

    for output_path in result.successful_plots:
        print(f"Saved plot to {output_path}")

    for failure in result.failed_plots:
        print(f"FAILED {failure.configuration_path.name}: {failure.message}")

    print(
        f"Analysis complete: {len(result.successful_plots)} succeeded, "
        f"{len(result.failed_plots)} failed"
    )

    return 1 if result.failed_plots else 0


if __name__ == "__main__":
    raise SystemExit(main())
