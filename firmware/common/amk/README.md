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
    - `7` PHASE_1
- `Z` is the node identifier (A = 1, B = 2, C = 3, D = 4).

For example, `0x114` is a priority 1 SET message for node D.

## Available feedback and research findings

These notes describe documented capabilities, not the signals currently enabled
in the node configs. The racing-kit FSE CAN parameter list is narrower than the
general AMK parameter catalog. A parameter's presence in a general manual does
not establish that our FSE firmware can transmit it over CAN. The kit manual
also warns that FSE behavior can differ from the general parameter descriptions
([racing-kit manual][kit], p. 34). Page numbers below are printed page numbers.

### 1. Temperature feedback

The kit confirms three distinct temperature quantities: motor winding, inverter
cold plate, and IGBT. The first two use KTY sensors; the IGBT value is calculated
by a thermal model ([kit manual][kit], pp. 73, 83–84).

| Source | Meaning | Format | Extra independent temperature? |
| --- | --- | --- | --- |
| Special 7 | Motor temperature | Signed 16-bit, 0.1 °C/count | Existing motor quantity |
| Special 8 | Cold-plate temperature | Signed 16-bit, 0.1 °C/count | Existing cold-plate quantity |
| Special 27 / ID34215 | Modeled IGBT temperature | Signed 16-bit, 0.1 °C/count | Existing IGBT quantity |
| ID33116, "Temperature internal" | Cold-plate temperature | Signed 16-bit, 0.1 °C/count | Alternate access to cold plate |
| ID33117, "Temperature external" | Connected temperature sensor, normally motor KTY | Signed 16-bit, 0.1 °C/count | Normally alternate access to motor sensor |

The two SERCOS temperature parameters are in the FSE CAN list ([kit][kit],
p. 76); their meanings are clarified in the [AMK parameter descriptions][kw17],
p. 190. ID33117 is not a valid numerical temperature measurement with a PTC
sensor; the connected sensor type is selected by ID34166.

Other AMK hardware documents ID32810, enclosure-air temperature near the IGBT,
at 0.1 °C/count ([ihX parameter manual][ihx], p. 142). It is absent from both
the kit's CAN list and the KW-R25 parameter manual. No additional independent
temperature channel is confirmed for this racing kit by the reviewed manuals.

### 2. Position feedback during torque or speed control

Rotor-position feedback is used for commutation in torque and speed operation;
observing position does not require commanding position-control mode. The kit's
motor has a single-turn absolute EnDat 2.2 light encoder. Encoder alignment is
stored in the encoder, and speed is derived from its feedback ([kit][kit],
pp. 36, 85).

The FSE CAN list exposes ID33104, position feedback modulo, and ID32831,
commutation angle ([kit][kit], pp. 75–76). ID33104 is in increments and wraps
to zero at its modulo boundary. ID32800 bit 13 selects encoder-derived modulo
or ID103's configured modulus; position-source selection applies across modes.
Do not assume the boundary equals one revolution or 65,536 increments. ID32831
is a separate unsigned 16-bit commutation-angle value, not an accumulated turn
count ([KW-R25 manual][kw25], pp. 74, 143, 152, 181).

Documentation discrepancy: the FSE table calls ID33104 SGN32, while the general
parameter definition describes an unsigned, nonnegative 32-bit value. Confirm
the active firmware's representation and position scaling before implementing
decoding or wrap handling ([kit][kit], p. 76; [KW-R25][kw25], p. 181).

For an accumulated position, the receiver must count wraps. A standard
nearest-wrap calculation for modulus `M` is:

```text
delta = ((position_now - position_previous + M/2) mod M) - M/2
accumulated_position += delta
```

This calculation assumes mathematical, nonnegative modulo and movement of less
than half a modulo period per sample. Sampling implication: with one mechanical
revolution per modulo period, 100 Hz supports unambiguous nearest-wrap tracking
only below 3,000 rpm. Faster motion needs a faster sample rate or additional
information to resolve missed revolutions. A single-turn absolute encoder does
not independently retain a multi-turn travel count through power loss.

### 3. Phase voltages

The FSE CAN list documents DC-bus voltage ID32836 but no individual U/V/W
phase-voltage feedback ([kit][kit], pp. 75–77).

The general catalog has ID33183/ID33184, stator-coordinate voltage components
Ua/Ub: signed 32-bit, 0.1 V/count ([AMK parameter descriptions][kw17], p. 196).
They are coordinate components, not three separately documented phase-voltage
measurements. Neither is in the FSE CAN list, so availability on our firmware
needs investigation through AIPEX or AMK support. The reviewed descriptions
also do not establish whether these values are independently measured terminal
voltages or internally reconstructed values.

ID34203/ID34204/ID34205, "Voltage at 25/75/125 degrees", are temperature-sensor
calibration points, not phase-voltage feedback ([KW-R25][kw25], p. 224).

### 4. D/Q voltages

The general KW-R25 manual defines Vq as ID34212 and Vd as ID34213: signed
16-bit, 0.1 V/count, current-controller output voltages described as effective
values ([KW-R25][kw25], p. 225). Neither appears in the FSE CAN list
([kit][kit], pp. 75–77). They are candidates for AIPEX/scope investigation;
CAN transmission on our racing firmware is not confirmed.

### 5. Electromagnetic torque and actual torque

Special 19, actual torque, is the closest confirmed FSE feedback. It uses
signed 16-bit values at 0.1% of nominal motor torque per count:

```text
T_est[Nm] = torque_raw * nominal_motor_torque[Nm] / 1000
```

Use the motor's nominal torque corresponding to ID32771, not its peak torque
([kit][kit], pp. 73, 81).

AMK describes ID84 torque feedback as proportional to actual current and notes
possible deviations up to 3% of nominal torque due to measurement/component
tolerances ([ID84 parameter description][torque]). Engineering interpretation:
treat this as a current-derived estimate of developed/electromagnetic torque,
not an independent shaft-torque or wheel-torque measurement. The reviewed FSE
signal list does not identify a separate "electromagnetic torque" channel.

### 6. Data useful for developing a custom inverter

The following are research priorities, not additions to the CAN configuration.
The listed sources are present in the FSE CAN tables ([kit][kit], pp. 73–77).

| Data | Sources | Intended use |
| --- | --- | --- |
| Motor model | ID34045/34046 Ld/Lq, ID34233 phase resistance, ID34234 Ke, ID32775 pole count | Initialize and validate the motor-control model |
| Encoder configuration | ID32953 encoder type, ID34243 commutation offset; encoder database | Interpret rotor feedback and reproduce alignment |
| Current response | ID32828–32830 phase currents; Special 25/26 Iq/Id; ID34281/34282 current setpoints | Compare commanded and delivered current |
| Current-controller tuning | ID34151/34152 D/Q proportional gains; ID34050/34052 integral times | Understand the existing controller's tuning |
| Torque response and limits | Actual torque, Special 13/14 limits, ID34301/34302 filter input/output, derating flag | Separate controller behavior from limiting |
| Thermal/protection behavior | Temperatures, ID33101/33102 I²t utilization, diagnostic number and all three information words | Characterize overload and protection behavior |

Special 25/26 are normalized signed 16-bit counts. Their conversion is
`I[A] = raw * ID110[A] / 16384`, using the active converter's peak current
([kit][kit], pp. 73, 81). ID34281/34282 are explicitly listed by FSE, but
their physical scaling is not specified there and must be confirmed.

Use the [derating manual][derating], pp. 2–7 and 9, for I²t behavior and motor
monitor activation. Preserve complete diagnostic context because error/warning
behavior and additional-information meanings depend on the diagnostic number
([diagnostics manual][diagnostics], pp. 18–19, 254–255).

For transient measurements, investigate AIPEX's internal oscilloscope before
relying on 100 Hz CAN data. The kit documents scope-based controller tuning
and configurable recording memory ([kit][kit], pp. 92–98). Engineering
recommendation: collect synchronized feedback at a rate appropriate to the
electrical/current-loop dynamics; 100 Hz operational logging alone cannot
characterize switching waveforms or fast current-loop response.

### 7. Efficiency percentage

**No live efficiency-percent signal was found in the documented FSE CAN list
or the KW-R25 parameter catalog.** The kit's approximately 98% inverter
efficiency is a specification, not telemetry ([kit][kit], p. 16). The manual
instead refers to a motor efficiency chart for operating-point efficiency
(p. 25); AMK links that chart from its [FSE documentation page][fse].

Available power-related sources include ID33171 electrical active power
(signed 32-bit, 1 W/count) and ID33100, power calculated from torque and speed;
ID34199 is the signed actual-power variant ([KW-R25][kw25], pp. 180, 184, 223).
All three appear in the FSE CAN list ([kit][kit], p. 76). They are not direct
efficiency measurements.

An application can estimate a conversion-efficiency percentage from torque,
speed, and electrical active power:

```text
P_mech_est[W] = T_est[Nm] * speed[rpm] * 2*pi / 60

Motoring:   efficiency_est[%] = 100 * abs(P_mech_est) / abs(P_elec)
Generating: efficiency_est[%] = 100 * abs(P_elec) / abs(P_mech_est)
```

These are engineering calculations, not AMK-provided percentage signals.
Validate ID33171's electrical measurement boundary before interpreting the
ratio: the reviewed parameter definition does not establish that boundary
precisely enough to label it inverter-only or complete-drive efficiency.
The torque-based mechanical-power estimate is also not an independent shaft
power measurement. Use synchronized, sufficiently steady measurements above a
meaningful power threshold; low power, transients, and torque-estimate error
can make the ratio misleading.

For measured inverter-only efficiency, compare DC input power with motor-side
AC active output power (reverse the input/output roles during regeneration).
For complete inverter-plus-motor efficiency, compare DC electrical power with
measured shaft power. The FSE CAN list does not document per-inverter DC-input
current; DC-bus voltage or motor phase current alone is insufficient to supply
that measurement. External, appropriately located power measurements are
needed to establish these boundaries. Reactive power or power factor must not
be presented as efficiency.

[kit]: ../../../docs/datasheets/AMK_datasheet.pdf
[derating]: ../../../docs/datasheets/AMK_derating.pdf
[diagnostics]: ../../../docs/datasheets/AMK_diagnostics.pdf
[kw25]: https://doku.amk-motion.com/en/Subsystems/PDK_205909_Parameter_KW-R25_en/Content/Resources/PDF-Dateien/PDK_205909_Parameter_KW-R25_en_.pdf
[kw17]: https://doku.amk-motion.com/en/content/resources/pdf-dateien/pdk_205907_parameter_kw-r17_en_.pdf
[ihx]: https://doku.amk-motion.com/en/content/resources/pdf-dateien/pdk_205914_parameter_ihx_en_.pdf
[torque]: https://doku.amk-motion.com/en/Subsystems/PDK_205914_Parameter_ihX_en/Content/Projekt/Parameters/AMKASYN/PDK_203704_Parameter_KW-R06/Parameterbeschreibungen/ID00084.htm?TocPath=Parameter+descriptions%7C_____54
[fse]: https://doku.amk-motion.com/en/content/projekt/doku-cd_html5/topics/fse_2025.htm
