## Main Module
Main module is the master control unit of the car. It manages the global state of the car, checks SDC status, interfaces with inverters, and manages the final torque request.

The primary state machine is implemented in `vehicle_fsm.c` and the state diagram is as follows:

![main_FSM](vehicle_fsm/vehicle_FSM.drawio.png)
- Note that the definition of the `car_state_t` enum itself is located in `generators/configs/system/common_types.json` and generated into shared header files by the CANpiler.

## Hardware Block Diagram
![main_module](./main_module.drawio.png)
