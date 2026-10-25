# PER DaqCLI

DaqCLI is a command-line companion to DaqApp for offline DAQ log parsing. It runs
the same `daqcore::log_parse` pipeline as the app's Log Parser, so recorded `.log`
files can be parsed and correlated into CSV tables without opening the GUI, which
is handy for scripts, batch jobs, and CI.

## Usage

DaqCLI takes explicit paths as arguments, so it can be run from any working
directory:

```bash
cargo run -p daqcli -- \
    --logs-dir <dir with .log files> \
    --output-dir <dir for CSV output> \
    --superdbc <path to superdbc_*.json>
```

SuperDBC artifacts are firmware build outputs found in `monorepo/superdbc`.

## Options

- `--logs-dir` (required) - directory containing the raw `.log` files
- `--output-dir` (required) - directory to write CSV tables into
- `--superdbc` (required) - SuperDBC JSON artifact containing the bus definitions
- `--prefix` - output filename prefix (default: `out`)


Set `RUST_LOG` to control log verbosity (for example `RUST_LOG=debug`).
Defaults to `info`.
