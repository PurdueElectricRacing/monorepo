# CAN Configs

## Bus Definition (`configs/system/bus_configs.json`)
Validated by `bus.schema.json`. Describes each logical CAN bus (not how a node attaches to it).

- `name`: Logical bus name (referenced by nodes and external nodes).
- `baud_rate`: CAN bitrate for this bus.
- `is_extended_id`: Boolean. Mixed-ID buses are not supported; all messages on a bus share this framing.
- `is_flexible_data_rate`: Boolean. Whether this bus uses CAN FD.
- `host_fault_library`: Boolean. If true, this bus is the primary uplink for fault communication (usually VCAN).

## Node-to-Bus Mapping (per node JSON, under `busses`)
Validated by `node.schema.json`. Maps a firmware node onto hardware peripherals and message lists.

- `peripheral`: Hardware peripheral identifier. Allowed values: `CAN1`, `CAN2`, `FDCAN1`, `FDCAN2`, `FDCAN3`.
- `accept_all_messages`: Boolean. If true, disables hardware filter optimization (promiscuous mode).
- `tx`: Array of TX message definitions for this node on this bus.
- `rx`: Array of RX message definitions for this node on this bus.

## Node Attributes
- `node_name`: Name of the node. Must be unique.
- `fault_library_enabled`: Required boolean. If true, this node participates in the FIDR fault sync system.
- `faults`: (Optional) Array of fault definitions specific to this node.

## CAN Message Attributes (TX)
- `msg_name`: Unique identifier.
- `msg_desc`: Short description of message purpose.
- `msg_priority`: [0-5]. See priority convention below.
- `msg_period`: Optional. Transmission period in ms. Omit or use `0` for event-triggered (non-periodic) messages.
- `byte_order`: Optional. `little_endian` (default) or `big_endian` for multi-byte signal packing within the frame.
- `msg_id_override`: Optional. Manual ID (supports hex `0x...`). Overrides automatic linking. Must respect the bus framing limits.

## RX Message Attributes
- `msg_name`: Name of the message to receive (must exist on the bus).
- `callback`: Boolean. If true, the generated driver declares and calls `<msg_name>_CALLBACK()` after RX unpacking. Application code must define this function or the firmware link will fail.

## Signal Attributes
- `sig_name`: Signal name. Must be unique within the message.
- `sig_desc`: (Optional) Short description of signal purpose.
- `type`: C-type (`uint8_t`...`float`) or a custom type defined in `configs/system/common_types.json`.
- `length`: (Optional for standard types) Bit-length. Required for custom packing or sub-byte types.
- `unit`: (Optional) Physical unit label for DBC generation (e.g., `V`, `Amps`, `C`).
- `scale`: Multiplier for physical value conversion. Default 1.0.
- `offset`: Optional. Linear offset applied during physical conversion. Default 0.0.
- `min`: (Optional) Minimum theoretical value.
- `max`: (Optional) Maximum theoretical value.
- `choices`: (Optional) List of strings for enum-like labels in DBC.

> [!NOTE]
> If `scale` is present, `unit` is required by the schema.

> [!NOTE]
> Signals named `reserved`, `reserved1`, etc. remain part of the message layout, RX data structs, and DBC output. For local TX helpers, reserved signals are omitted from the `CAN_SEND_*` arguments and automatically packed as zero.

> [!TIP]
> **Scaling Constants:** Node headers generate directional `static constexpr float` constants for every local signal with a scale != 1.0.
> Use `PACK_COEFF_<MSG>_<SIG>` before passing physical values to local `CAN_SEND_*` TX helpers, and use `UNPACK_COEFF_<MSG>_<SIG>` when interpreting raw `can_data` values from local RX messages.

> [!TIP]
> **Offset Constants:** Node headers generate a `static constexpr float OFFSET_<MSG>_<SIG>` constant for every local signal with an offset. When the signal is also scaled, apply the offset alongside the coefficient:
> - RX: `physical = raw * UNPACK_COEFF_<MSG>_<SIG> + OFFSET_<MSG>_<SIG>`
> - TX: `raw = (physical - OFFSET_<MSG>_<SIG>) * PACK_COEFF_<MSG>_<SIG>`
>
> For an offset-only signal (no scale), no coefficient is generated, so apply the offset directly:
> - RX: `physical = raw + OFFSET_<MSG>_<SIG>`
> - TX: `raw = physical - OFFSET_<MSG>_<SIG>`


### Message Priority
Lower = higher priority.
Range: [0-5].
PER vehicle convention:

0.  event based, safety-critical
	- fault events, charge commands
1. periodic safety-critical vehicle operation
	- Motor commands, fault sync, charging
2. periodic torque path relevant
	- throttle, torque vectoring commands, steering angle
3. non torque path vehicle operation
	- cooling commands, daq log enable
4. Low frequency (<5HZ) periodic telemetry
	- battery voltage
5. High frequency (>5HZ) "best effort" telemetry
	- IMU raw data, shock pots, battery current

## External Nodes
External nodes (non-PER devices such as chargers or inverters) are defined in `configs/external_nodes/` and validated by `external_node.schema.json`.

### Attributes
- `node_name`: Name of the external device. Must be unique.
- `bus_name`: The logical bus this device sits on (must match a bus in `bus_configs.json`).
- `tx`: Array of messages this device sends toward PER nodes.
- `rx`: Array of messages this device expects to receive.

## Custom Types
Define reusable signal types (enums and aliases) in `configs/system/common_types.json`. Validated by `type_registry.schema.json`.

### Type Attributes
- `name`: Type name. Must end in `_t` (e.g. `car_state_t`).
- `base_type`: One of `uint8_t` ... `uint64_t`, `int8_t` ... `int64_t`, `bool`.
- `choices`: (Optional) Enum value list, generated as a C `enum`.

## Fault Configuration
Node-specific faults are defined directly in the node JSON under the `"faults"` key. The library automatically generates bitfield-sync and event messages for fault communication based on these definitions.

### Node Level
- `generate_fault_messages`: Boolean. If true, generates LCD string arrays for the node (usually only true for nodes with displays like the Dashboard).

### Fault Attributes
- `fault_name`: Unique name within the node.
- `min`: Minimum healthy value. Triggers fault if value < min.
- `max`: Maximum healthy value. Triggers fault if value >= max.
- `priority`: Impact of the fault (`warning`, `error`, `fatal`).
- `time_to_latch`: Time in ms the condition must persist before the fault is latched.
- `time_to_unlatch`: Time in ms the condition must be healthy before the fault is cleared.
- `lcd_message`: String text for display on the dashboard or log.
