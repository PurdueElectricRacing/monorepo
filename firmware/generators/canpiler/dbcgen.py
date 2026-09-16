"""
dbcgen.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from typing import Optional
from collections import OrderedDict
from .ir import BuildMetadata, LinkedCan, MessageKey
from cantools import database
from cantools.database.conversion import BaseConversion
from cantools.database.can.signal import NamedSignalValue
from core.artifacts import Artifact
from core.utils import print_as_success, print_as_ok

def generate_dbcs(
    context: LinkedCan,
    metadata: BuildMetadata,
) -> list[Artifact]:
    """
    Generates DBC files for each bus in the system.
    """
    print("Generating DBCs...")

    git_hash = metadata.version
    artifacts = []

    bus_names = sorted({
        bus_name for node in context.nodes for bus_name in node.busses
    })
    for bus_name in bus_names:
        can_db = database.can.Database()
        
        # Add nodes
        for node_name in sorted(
            node.name for node in context.nodes if bus_name in node.busses
        ):
            can_db.nodes.append(database.can.Node(name=node_name, comment=""))

        # Add messages
        messages = sorted(
            (
                message for key, message in context.messages.items()
                if key.bus_name == bus_name
            ),
            key=lambda message: (message.final_id, message.message_name),
        )
        for msg in messages:
            signals = []
            
            # Sort signals by bit offset for deterministic output
            sorted_signals = sorted(msg.signals, key=lambda x: x.bit_offset)
            
            for sig in sorted_signals:
                # Resolve choices (enums) for VAL_ table in DBC
                choices: Optional[OrderedDict[int, str | NamedSignalValue]] = None
                if sig.choices:
                    choices = OrderedDict((i, c) for i, c in enumerate(sig.choices))
                elif sig.data_type in context.custom_types:
                    type_info = context.custom_types[sig.data_type]
                    if type_info.choices:
                        choices = OrderedDict((i, c) for i, c in enumerate(type_info.choices))
                elif sig.data_type == 'bool':
                    choices = OrderedDict({0: "OFF", 1: "ON"})

                conversion = BaseConversion.factory(
                    scale=sig.scale if sig.scale is not None else 1.0,
                    offset=sig.offset if sig.offset is not None else 0.0,
                    choices=choices,
                    is_float=(sig.data_type == 'float')
                )

                signals.append(database.can.Signal(
                    name=sig.signal_name,
                    start=sig.bit_offset,
                    length=sig.length,
                    byte_order=sig.byte_order,
                    is_signed=sig.is_signed,
                    conversion=conversion,
                    minimum=sig.min,
                    maximum=sig.max,
                    unit=sig.unit if sig.unit else "",
                    comment=sig.description
                ))
            
            # Use pre-calculated sender mapping
            sender = context.transmitters.get(
                MessageKey(bus_name, msg.message_name), "Vector__XXX"
            )

            can_db.messages.append(database.can.Message(
                frame_id=msg.final_id,
                name=msg.message_name,
                length=msg.dlc,
                signals=signals,
                comment=msg.description,
                is_extended_frame=msg.is_extended,
                senders=[sender],
                strict=True
            ))
        
        filename = f"{bus_name}_{git_hash}.dbc"
        artifacts.append(Artifact("dbc", filename, can_db.as_dbc_string()))
        
        print_as_ok(f"Generated {filename}")

    print_as_success("Successfully generated DBC files")
    return artifacts
