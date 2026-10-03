# daqcli

Headless use of the same CAN worker and Session as daqapp. Default features are
serial and loopback. Enable `udp` or `simulated` for those sources.

```sh
cargo run --manifest-path daq/Cargo.toml -p daqcli -- connect --source serial --port /dev/ttyACM0 --bitrate 500
cargo run --manifest-path daq/Cargo.toml -p daqcli --features simulated -- watch --source simulated --dbc dbc/vehicle.dbc --id 0x123
```

`connect` reports connection status; `watch` also prints frames, with optional
ID filtering. Ctrl-C requests cleanup and joins the worker. `--help` lists options.
