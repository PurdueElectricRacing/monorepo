"""
dbcgen.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from typing import Optional
from collections import OrderedDict
from parser import SystemContext
from cantools import database
from cantools.database.conversion import BaseConversion
from cantools.database.can.signal import NamedSignalValue
from utils import DBC_DIR, print_as_success, print_as_ok

def generate_dbcs(context: SystemContext):
    """
    Generates DBC files for each bus in the system.
    """
    print("Generating DBCs...")

    # Ensure output directory exists and is clean
    if DBC_DIR.exists():
        for found_dbc_path in DBC_DIR.glob("*.dbc"):
            found_dbc_path.unlink()
    DBC_DIR.mkdir(exist_ok=True)

    git_hash = context.version

    for bus_name, view in context.busses.items():
        can_db = database.can.Database()
        
        # Add nodes
        for node_name in sorted(view.nodes):
            can_db.nodes.append(database.can.Node(name=node_name, comment=""))

        # Add messages
        for msg in view.messages:
            signals = []
            
            # Sort signals by bit offset for deterministic output
            sorted_signals = sorted(msg.signals, key=lambda x: x.bit_offset)
            
            for sig in sorted_signals:
                # Resolve choices (enums) for VAL_ table in DBC
                choices: Optional[OrderedDict[int, str | NamedSignalValue]] = None
                if sig.choices:
                    choices = OrderedDict((i, c) for i, c in enumerate(sig.choices))
                elif sig.datatype in context.custom_types:
                    type_info = context.custom_types[sig.datatype]
                    if 'choices' in type_info:
                        choices = OrderedDict((i, c) for i, c in enumerate(type_info['choices']))
                elif sig.datatype == 'bool':
                    choices = OrderedDict({0: "OFF", 1: "ON"})

                conversion = BaseConversion.factory(
                    scale=sig.scale if sig.scale is not None else 1.0,
                    offset=sig.offset if sig.offset is not None else 0.0,
                    choices=choices,
                    is_float=(sig.datatype == 'float')
                )

                signals.append(database.can.Signal(
                    name=sig.name,
                    start=sig.bit_offset,
                    length=sig.length,
                    byte_order=sig.byte_order,
                    is_signed=sig.is_signed,
                    conversion=conversion,
                    minimum=sig.min_val,
                    maximum=sig.max_val,
                    unit=sig.unit if sig.unit else "",
                    comment=sig.desc
                ))
            
            # Use pre-calculated sender mapping
            sender = view.sender_map.get(msg.name, "Vector__XXX")

            can_db.messages.append(database.can.Message(
                frame_id=msg.final_id,
                name=msg.name,
                length=msg.get_dlc(context.custom_types),
                signals=signals,
                comment=msg.desc,
                is_extended_frame=msg.is_extended,
                senders=[sender],
                strict=True
            ))
        
        filename = f"{bus_name}_{git_hash}.dbc"
        output_path = DBC_DIR / filename
        with open(output_path, 'w', newline='\n') as f:
            f.write(can_db.as_dbc_string())
        
        print_as_ok(f"Generated {filename}")

    print_as_success("Successfully generated DBC files")
