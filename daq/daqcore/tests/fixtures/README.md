# Frozen CANpiler compatibility fixtures

These files were generated together at revision `4d93f8e`. The SuperDBC JSON is
compact but otherwise preserves the generated content. Keeping fixtures here
makes the decoder tests independent of ignored/rebuilt files in the root `dbc/`.

The five DBCs are used only by dev tests/benchmarks; production builds do not
link can_decode or can-dbc. The differential test checks all 145 messages using
zero, all-one, and deterministic pseudorandom payloads. Boolean OFF/ON labels are
explicit in SuperDBC and are not supplied by the matching DBC generator.
