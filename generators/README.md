# CANpiler

![canpiler](canpiler.drawio.png)

## Diagnostics

`diagnosticsgen` adds CAN messages for task stats, CPU usage, and task names.

`DIAGNOSTICS_NODES` in `diagnosticsgen/api.py` lists each node's preferred bus.
If the node doesn't use that bus, the generator uses its only configured bus
instead. If there are several buses to choose from, it reports an error.
Update the list when adding a node or changing its preferred bus.
