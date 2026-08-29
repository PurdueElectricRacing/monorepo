## AMK Documentation
AMK helpers define message layout/conventions for inverter communication and document the related CAN ID mapping used by this repo.

## 2026 AMK CAN ID Assignment Scheme
AMK related messages have a CAN ID in the format `0xXYZ`, where:
- `X` is the priority level (0-5, following the [standard convention](../../generators/configs/README.md#message-priority)).
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
