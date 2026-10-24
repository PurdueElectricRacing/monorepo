# PER DaqCLI

DaqCLI is a command-line companion to DaqApp for offline DAQ log parsing. It uses
`daqcore::log_parse` to decode recorded `.log` files and correlate timestamps,
exporting CSV tables or a Parquet dataset.

Using DaqCLI is equivelent to using the CSV Export and Parquet Export widgets in DaqApp.

## Usage

Select Parquet explicitly and choose a dataset directory that does not exist yet:

```bash
cargo run -p daqcli -- \
    --logs-dir <dir with .log files> \
    --output-dir <dir for CSV output> \
    --superdbc <path to superdbc_*.json>
    --format <'csv' or 'parquet'>
```

SuperDBC artifacts are firmware build outputs found in `monorepo/dbc`.

## Options

- `--logs-dir` (required) - directory containing the raw `.log` files
- `--output-dir` (required) - CSV output directory, or the new Parquet dataset directory
- `--superdbc` (required) - SuperDBC JSON artifact containing the bus definitions
- `--format` - `csv` or `parquet` (default: `csv`)
- `--prefix` - CSV output filename prefix (default: `out`); ignored for Parquet

Set `RUST_LOG` to control log verbosity (for example `RUST_LOG=debug`).
Defaults to `info`.
