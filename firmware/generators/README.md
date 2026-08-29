# Firmware generators

`generate.py` composes independent generators. CANpiler compiles the CAN model;
faultgen contributes fault CAN messages and renders fault artifacts. Both depend
only on contracts in `core/` and return artifacts without choosing output paths.
Configuration and schemas are owned here under `configs/` and `schema/`.

```text
generate.py -> canpiler -> core
            -> faultgen -> core
```
