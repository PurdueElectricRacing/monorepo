# Data Acquisition Board

This directory contains the firmware source code for the Data Acquisition (DAQ) board, responsible for data collection, storage, and streaming.

## Directory Structure
- [`main.c`](main.c) / [`main.h`](main.h) - Main entry point for DAQ firmware, responsible for initialization and thread management.
- `can_irq/` - CAN RX IRQ implementations, consuming CAN busses and feeding the SPMC
- `spmc/` - Custom lockless Single Producer Multiple Consumer queue implementation for high throughput data buffering between CAN IRQs and consumer threads (SD card writing, Ethernet streaming).
- `rtc_sync/` - Synchronization of the RTC peripheral with the reported GPS time
- `sd_card/` - SD logging and file management
- `ethernet/` - Real-time UDP streaming of CAN bus activity wirelessly

## Hardware Block Diagram
![daq](daq.drawio.png)

## Software Timing Diagram
![timing diagram](DAQ_timing_diagram.drawio.png)

## CANpiler Integration
DAQ uses CANpiler for its generated CAN configuration, fault library, and queued
CAN transmission. In particular, `sd_card_periodic()` reports
`DAQ_LOGGING_DISABLED` when the physical `LOG_ENABLE` switch is off. The generated
fault task sends the resulting `daq_fault_sync` message on VCAN; Dashboard's
generated receiver accepts it, with the configured label `SD Card Not Logging`.
This fault describes the switch position; it does not mean that an enabled SD card
is mounted, writable, or actively logging. Dashboard presentation still requires
on-hardware verification.

DAQ deliberately does not use CANpiler's normal receive task (`CAN_rx_update()`).
DAQ must retain every received raw frame, including frames without generated
message definitions, so the F4 FIFO0 interrupt entry points in `can_irq/can_irq.c`
provide strong definitions that override PHAL's weak default handlers. They
timestamp each frame and enqueue it in the SPMC queue for SD logging and Ethernet;
the VCAN GPS-time frame is additionally sent to the RTC synchronization queue.
Starting `CAN_rx_update()` would instead route FIFO0 frames through CANpiler's
bounded RX queue and lose DAQ's raw-frame logging path.

`main.c` consequently starts only CANpiler's `CAN_tx_update()` and
`fault_library_periodic()` tasks, rather than `DEFINE_CAN_TASKS()`. It still calls
`CAN_init()` to initialize generated filters, fault state, and TX queues, and calls
`CAN_enable_IRQs()` because queued transmission needs its TX interrupt enable. The
latter also enables FIFO0; this duplicates SPMC's FIFO0 enable but does not change
handler ownership. Do not remove the call or add `CAN_rx_update()` without
redesigning and validating DAQ's raw logging path.

## SPMC Queue
Specialized data structure designed specifically for DAQ.
- Lockless
- Priority aware
- High throughput
- DMA friendly

DAQ26 setup:
- Producer(s): CAN1 and CAN2 ISRs
- Master Consumer: SD thread
- Follower Consumer: Ethernet thread

Notes:
- Even though there are actually two producing ISRs (CAN1 and CAN2), they have the same priority and cannot preempt each other, so we can treat them as a single producer for the purposes of this data structure.
- Data is returned to the consumers in contiguous chunks to optimize for DMA transfers.
    - The total capacity of the buffer is sized to be a multiple of chunk size to prevent fragmentation.
- If the SD thread falls behind, data will be dropped until it catches up. These "overflows" are tracked in a counter.
    - Several buffer parameters can be tuned to reduce the likelihood of overflows.
- If the ETH thread falls far behind the SD thread, it will fast-forward its tail to the location of the SD thread. The "dropped frames" are also tracked in a counter.

## SD Card
![sd fsm](sd_card/SD_FSM.drawio.png)

## Ethernet
![ethernet fsm](ethernet/ethernet_FSM.drawio.png)
