"""
bus_load_analyzer.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from .pipeline_models import LinkedCan
from core.utils import bcolors, print_as_warning, print_as_ok

# CAN 2.0 Base Overhead (including 3-bit Inter-Frame Space)
# Standard: SOF(1), ID(11), RTR(1), IDE(1), r0(1), DLC(4), CRC(15), ACK(2), EOF(7), IFS(3) = 47
# Extended: SOF(1), ID(11), SRR(1), IDE(1), ID(18), RTR(1), r1(1), r0(1), DLC(4), CRC(15), ACK(2), EOF(7), IFS(3) = 67
STANDARD_OVERHEAD = 47
EXTENDED_OVERHEAD = 67

# Typical bit-stuffing factor (varies by data, but 1.2 is a safe average)
STUFFING_FACTOR = 1.2

def calculate_bus_load(context: LinkedCan) -> None:
    """
    Analyzes all busses in the system context and prints estimated bus load.
    Load calculation: (Total Bits Per Second / Baud Rate) * 100
    """
    print("Bus Load Analysis:")

    bus_names = sorted({
        bus_name for node in context.nodes for bus_name in node.busses
    })
    for bus_name in bus_names:
        total_bits_per_sec = 0.0
        baud_rate = context.bus_configs[bus_name].baud_rate

        for item in context.tx_messages:
            if item.bus_name != bus_name:
                continue
            msg = item.message
            # 1. Calculate frames per second
            # Treat asynchronous messages (period=0) as 1Hz for estimation
            fps = 1000.0 / msg.period_ms if msg.period_ms > 0 else 1.0

            # 2. Bits per frame (Overhead + Data)
            overhead = EXTENDED_OVERHEAD if msg.is_extended else STANDARD_OVERHEAD
            dlc = msg.dlc
            bits_per_frame = overhead + (dlc * 8)

            # 3. Apply bit-stuffing factor
            stuffed_bits = bits_per_frame * STUFFING_FACTOR

            total_bits_per_sec += (stuffed_bits * fps)

        # Calculate final percentage
        load_percent = (total_bits_per_sec / baud_rate) * 100

        # Determine color formatting
        color = bcolors.GREEN
        if load_percent >= 70:
            color = bcolors.RED
        elif load_percent >= 50:
            color = bcolors.ORANGE

        if color != bcolors.GREEN:
            print_as_warning(f"{bus_name}: {color}{load_percent:.2f}%{bcolors.ENDC}")
        else:
            print_as_ok(f"{bus_name}: {color}{load_percent:.2f}%{bcolors.ENDC}")
