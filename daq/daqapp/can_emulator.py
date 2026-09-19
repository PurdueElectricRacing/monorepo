import os
import time
import random
import threading
import can
import cantools
from datetime import datetime

DBC_PATH = "per_dbc_VCAN.dbc"   # path to your DBC
CHANNEL = "vcan0"               # virtual CAN channel
BITRATE = 500000


def setup_vcan():
    """Create a virtual CAN interface if it doesn't exist."""
    os.system("sudo modprobe vcan")
    os.system(f"sudo ip link add dev {CHANNEL} type vcan || true")
    os.system(f"sudo ip link set up {CHANNEL}")


def timestamp_now():
    """Return current local time with milliseconds as a string."""
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def send_random_frames(dbc_path, bus, stop_event):
    """Continuously send randomized CAN frames using a DBC file."""
    db = cantools.database.load_file(dbc_path)
    messages = db.messages
    print(f"[sim] Loaded {len(messages)} messages from {dbc_path}")

    while not stop_event.is_set():
        msg = random.choice(messages)
        signal_values = {}

        for sig in msg.signals:
            if sig.minimum is not None and sig.maximum is not None:
                low, high = sig.minimum, sig.maximum
            else:
                if sig.is_signed:
                    low = -(2 ** (sig.length - 1))
                    high = (2 ** (sig.length - 1)) - 1
                else:
                    low = 0
                    high = (2 ** sig.length) - 1

            val = (
                random.uniform(low, high * 0.9999)
                if sig.is_float
                else random.randint(int(low), int(high))
            )
            signal_values[sig.name] = val

        try:
            data = msg.encode(signal_values)
            frame = can.Message(
                arbitration_id=msg.frame_id,
                data=data,
                is_extended_id=msg.is_extended_frame,
            )
            bus.send(frame)
            print(f"[{timestamp_now()}] [sim] {msg.name:<20} ID=0x{msg.frame_id:03X} {signal_values}")
        except Exception as e:
            print(f"[{timestamp_now()}] [sim] Send failed for {msg.name}: {e}")

        time.sleep(0.1)


def main():
    setup_vcan()

    print("[sim] Opening virtual CAN bus...")
    bus = can.Bus(channel=CHANNEL, interface="socketcan")

    stop_event = threading.Event()

    sender = threading.Thread(target=send_random_frames, args=(DBC_PATH, bus, stop_event), daemon=True)
    sender.start()

    print(f"[sim] Running on {CHANNEL}.")
    print("[sim] Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[sim] Shutting down...")
    finally:
        stop_event.set()
        os.system(f"sudo ip link del {CHANNEL} || true")
        print("[sim] Clean exit.")


if __name__ == "__main__":
    main()
