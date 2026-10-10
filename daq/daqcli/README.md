# PER DaqCLI

DaqCLI is a command-line companion to DaqApp for offline DAQ log parsing. It runs
the same `daqcore::log_parse` pipeline as the app's Log Parser, so recorded `.log`
files can be parsed and correlated into CSV tables without opening the GUI, which
is handy for scripts, batch jobs, and CI.

## What it does

- Reads raw `.log` files from a directory and decodes them with per-bus DBCs
- Time-correlates the two CAN buses (VCAN and MCAN) into combined CSV tables
- Writes the tables to an output directory with a configurable filename prefix

## Getting started

Install Rust/Cargo and the platform prerequisites described in the repository
[setup guide](../../docs/setup.md). Linux users also likely need `libudev-dev` and
`pkg-config`, since DaqCLI builds against `daqcore`, which pulls in serial support.

### To Build

From the repository root:

```bash
cargo build --manifest-path daq/daqcli/Cargo.toml --locked
```

From the `daq/daqcli/` directory, you can also build with Cargo directly:

```bash
cargo build --locked
```

### To Run

Unlike DaqApp, DaqCLI takes explicit paths as arguments, so it can be run from any
working directory:

```bash
cargo run -p daqcli -- \
    --logs-dir <dir with .log files> \
    --output-dir <dir for CSV output> \
    --bus0-dbc <path to VCAN .dbc> \
    --bus1-dbc <path to MCAN .dbc>
```

DBCs are an output of the firmware build process and are found in `monorepo/dbc`.

## Options

- `--logs-dir` (required) - directory containing the raw `.log` files
- `--output-dir` (required) - directory to write CSV tables into
- `--bus0-dbc` (required) - DBC file for bus 0 (VCAN)
- `--bus1-dbc` (required) - DBC file for bus 1 (MCAN)
- `--prefix` - output filename prefix (default: `out`)
- `--bus0-name` - column label for bus 0 (default: `VCAN`)
- `--bus1-name` - column label for bus 1 (default: `MCAN`)

Set `RUST_LOG` to control log verbosity (for example `RUST_LOG=debug`); it defaults
to `info`.