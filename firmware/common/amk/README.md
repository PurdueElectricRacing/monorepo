## AMK Documentation
AMK helpers define message layout/conventions for inverter communication and document the related CAN ID mapping used by this repo.

## 2026 AMK CAN ID Assignment Scheme
AMK related messages have a CAN ID in the format `0xXYZ`, where:
- `X` is the priority level (0-5, following the [standard convention](../../../generators/configs/README.md#message-priority)).
- `Y` is the message number:
    - `1` SET
    - `2` CRIT
    - `3` INFO
    - `4` ERR_1
    - `5` ERR_2
    - `6` TEMPS
    - `7` unused (former PHASE_I)
    - `8` POWER
    - `9` unused (former REACTIVE_POWER)
- `Z` is the node identifier (A = 1, B = 2, C = 3, D = 4).

For example, `0x114` is a priority 1 SET message for node D.

## 2026 inverter telemetry

The source configurations are
[`INVERTER_A.json`](../../../generators/configs/external_nodes/INVERTER_A.json)
through `INVERTER_D.json`. All four inverters use the same signal order, widths,
and rates. The table below uses node A's IDs; nodes B, C, and D use final hex
digits 2, 3, and 4. Payload offsets are zero-based byte offsets, in Intel
(little-endian) order.

| Message | ID (A) | Period | Payload bytes | Payload in byte order |
| --- | --- | --- | --- | --- |
| CRIT | `0x121` | 10 ms (100 Hz) | 8 | speed @0, torque @2, inverter overload @4, motor overload @6 |
| INFO | `0x231` | 500 ms (2 Hz) | 2 | complete status word @0 |
| ERR_1 | `0x441` | 500 ms (2 Hz) | 8 | diagnostic number @0, error info 1 @4 |
| ERR_2 | `0x451` | 500 ms (2 Hz) | 8 | error info 2 @0, error info 3 @4 |
| TEMPS | `0x561` | 100 ms (10 Hz) | 6 | motor @0, cold plate @2, IGBT @4 |
| POWER | `0x581` | 10 ms (100 Hz) | 6 | electrical active power @0, DC bus voltage @4 |

POWER combines power and voltage in one frame. The fast signals occupy 14 bytes,
so two classical CAN frames are the minimum at their native widths. The three
temperatures occupy 6 bytes at 10 Hz. The complete status word and four 32-bit
diagnostic values occupy 18 bytes at 2 Hz, requiring three more frames. This gives
six message definitions and **216 transmitted frames/s per inverter**, or
**864 frames/s for four inverters**, excluding commands and other MCAN traffic.
Removing PHASE_I and REACTIVE_POWER saves 800 frames/s across the four inverters
(48.1% of the previous inverter telemetry frame count).
These counts preserve each requested rate exactly, without narrowing signals or
transmitting slower signals at a higher rate. Use the exact DLCs above to avoid
unnecessary padding on the bus. The generator estimates total MCAN utilization
at 28.37% at 500 kbit/s, including the other configured traffic and its 1.2
bit-stuffing factor; this is an estimate, not a measured bus load.

### Signal definitions and sources

Physical values equal raw counts multiplied by the scale below; all offsets are
zero. Torque percentages refer to nominal motor torque, not peak torque.

| Signal | AMK source | Wire type | Physical unit | Scale |
| --- | --- | --- | --- | --- |
| Actual speed | Special 5 | signed 16-bit | rpm | 1 |
| Actual torque | Special 19 | signed 16-bit | % of nominal torque (ID32771) | 0.1 |
| Inverter overload | SERCOS ID33101 | unsigned 16-bit | % (I²t utilization) | 0.1 |
| Motor overload | SERCOS ID33102 | unsigned 16-bit | % (I²t utilization) | 0.1 |
| Electrical active power | SERCOS ID33171 | signed 32-bit | W | 1 |
| DC bus voltage | SERCOS ID32836 | unsigned 16-bit | V | 1 |
| Motor / cold plate / IGBT temperature | Special 7 / 8 / 27 | signed 16-bit each | °C | 0.1 |
| Status word | Special 3 | unsigned 16-bit | flags | 1 |
| Diagnostic number / error info 1 / 2 / 3 | Special 21 / 22 / 23 / 24 | unsigned 32-bit each | code / context | 1 |

Status bits retain AMK's layout: bits 0–7 reserved, then system ready, error,
warning, quit DC on, DC on, quit inverter on, inverter on, and derating at bits
8–15 respectively. The JSON represents the eight meaningful bits as booleans;
AIPEX maps this as one 16-bit Special 3 signal, rather than nine separate AMK
signals.

Repository references reviewed:

- [AMK datasheet](../../../docs/datasheets/AMK_datasheet.pdf), printed pp. 64–70:
  free CAN configuration, eight-byte limit, exact DLC, bit lengths, signedness,
  Intel byte order, and cyclic periods in 1 ms steps. Printed pp. 73–77 specify
  special signals and allowed SERCOS parameters; p. 81 specifies torque units.
- [AMK derating manual](../../../docs/datasheets/AMK_derating.pdf), pp. 2–7:
  overload is I²t utilization, with warnings and shutdown thresholds; it is not
  instantaneous torque utilization. Motor I²t monitoring requires activation of
  ID32773 bit 14 (p. 9).
- [AMK diagnostics manual](../../../docs/datasheets/AMK_diagnostics.pdf), pp.
  18–19 and the FSE entries on pp. 254–255: diagnostic context and CAN errors
  3586/3587. Preserve all three additional-information values; their meanings
  depend on the diagnostic number.

The repository datasheet lists SERCOS types but does not give scales for every
parameter. Supplementary AMK parameter references confirm
[electrical active power (ID33171)](https://www.amk-motion.com/amk-dokucd/dokucd/en/content/resources/pdf-dateien/pdk_205907_parameter_kw-r17_en_.pdf)
as signed 32-bit, 1 W/count (printed p. 193),
[overload (ID33101)](https://doku.amk-motion.com/en/Subsystems/PDK_205914_Parameter_ihX_en/Content/Projekt/Parameters/AMKASYN/PDK_203704_Parameter_KW-R06/Parameterbeschreibungen/ID33101.htm)
as unsigned 16-bit, 0.1 %/count, and
[DC bus voltage (ID32836)](https://doku.amk-motion.com/en/Subsystems/PDK_203704_Parameter_KW-R06_iX_A5_KE_en/Content/Projekt/Parameters/AMKASYN/PDK_203704_Parameter_KW-R06/Parameterbeschreibungen/ID32836.htm)
as unsigned 16-bit, 1 V/count. The KW-R25/R26 datasheet's allowed-parameter table
confirms these parameter IDs are available on the racing inverter.

[Motor overload (ID33102)](https://doku.amk-motion.com/en/Subsystems/PDK_205914_Parameter_ihX_en/Content/Projekt/Parameters/AMKASYN/PDK_203704_Parameter_KW-R06/Parameterbeschreibungen/ID33102.htm)
also uses 0.1 %/count. That general parameter reference lists signedness as
hardware-dependent (YES / NO); the racing-kit datasheet explicitly specifies
UNS16 for ID33102, which is the type used here.

ID33171 is positive during motoring and negative during generation. ID33100
('Actual power value') and ID34199 ('Actual power value bipolar') must not be
substituted for electrical active power. Electrical motor power excludes inverter
losses, so it is not
a direct battery-terminal power measurement.

### Applying the configuration

Run `python generators/generate.py` from the repository root to regenerate CAN
headers and DBCs. These JSON files describe the external inverter's wire
protocol; they do not program its firmware. Configure each inverter's AIPEX
ID34091 user list with the same IDs, cyclic periods, source indices, widths,
signedness, start bits, and DLCs before using this layout. Existing `.aipex`
archives have not been rewritten.

TEMPS moves from `0x46Z` to `0x56Z` to follow the high-frequency telemetry
priority convention. DC bus voltage moves out of INFO and into POWER; INFO now
has DLC 2. POWER uses `0x58Z`. PHASE_I (`0x57Z`) and REACTIVE_POWER (`0x59Z`)
have been removed; disable these transmissions in AIPEX as well. Phase U
current, Iq, Id, and reactive power are no longer configured. The remaining
telemetry IDs, layouts, periods, and command messages are unchanged.
Configure receiver subscriptions separately if an application needs to consume
POWER; adding a DBC definition does not add an
application RX subscription.

### Other useful data (not configured)

- **Higher-resolution speed (Special 20, signed 32-bit, 0.0001 rpm/count):** could
  improve low-speed wheel-slip estimation for TV/TC. Sensor noise, acquisition
  filtering, and end-to-end latency still need assessment.
- **Torque setpoint and positive/negative torque limits (Special 17/13/14,
  signed 16-bit, 0.1% of nominal torque):** inverter-side command feedback could
  help TV/TC distinguish requested torque from applied limits.
- **Torque filter input/output (SERCOS ID34301/34302, signed 16-bit):** could help
  measure command filtering and explain torque-response lag (datasheet p. 77).
- **Phase V/W currents (SERCOS ID32829/32830, signed 32-bit, 0.01 A/count):** could
  support phase-imbalance checks with suitable sampling; 100 Hz samples alone
  do not resolve phase-current waveforms at high motor speed.

The existing derating flag is useful when correlating lost torque with thermal,
I²t, or battery-voltage limits, but does not identify which limit is responsible
(datasheet pp. 83–84). The requested 2 Hz status rate also means status-driven
state transitions can wait up to one 500 ms reporting interval.
