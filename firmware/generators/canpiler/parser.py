"""
parser.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, List, Optional, Dict, Set, Literal
from core.config_models import (
    BusAttachmentConfig,
    BusConfig,
    ConfigBundle,
    CustomTypeConfig,
    ExternalNodeConfig,
    InternalNodeConfig,
    RxMessageConfig,
    SignalConfig,
    TxMessageConfig,
)
from core.utils import (
    CTYPE_SIZES,
    print_as_error, print_as_ok, print_as_success, to_macro_name, 
    get_git_hash, get_layout_hash, print_as_warning
)

if TYPE_CHECKING:
    from .mapper import NodeMapping

@dataclass
class Signal:
    name: str
    datatype: str
    desc: str = ""
    length: int = 0
    unit: Optional[str] = None
    choices: Optional[List[str]] = None
    scale: Optional[float] = None
    offset: Optional[float] = None
    min_val: Optional[float] = None
    max_val: Optional[float] = None
    byte_order: Literal["little_endian", "big_endian"] = "little_endian"
    
    # Resolved during parsing/linking
    bit_offset: int = 0
    bit_shift: int = 0
    is_signed: bool = False
    mask: int = 0

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)

    @property
    def c_type(self) -> str:
        return self.datatype

    @property
    def is_floating_point(self) -> bool:
        return self.datatype == 'float'

    @property
    def is_reserved(self) -> bool:
        return self.name.startswith('reserved')

    def get_bit_length(
        self, custom_types: Optional[Dict[str, CustomTypeConfig]] = None
    ) -> int:
        if self.length > 0:
            return self.length
        
        if self.datatype in CTYPE_SIZES:
            return CTYPE_SIZES[self.datatype]
        
        if custom_types and self.datatype in custom_types:
            base = custom_types[self.datatype].base_type
            return CTYPE_SIZES.get(base, 0)
            
        return 0

@dataclass
class Message:
    name: str
    desc: str = ""
    signals: List[Signal] = field(default_factory=list)
    priority: int = 0
    period: int = 0
    id_override: Optional[str] = None
    is_extended: bool = False
    byte_order: Literal["little_endian", "big_endian"] = "little_endian"
    final_id: int = 0
    dlc: int = 0
    layout_hash: str = ""

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)

    def resolve_layout(self, custom_types: Dict[str, CustomTypeConfig]) -> None:
        """
        Calculate bit offsets, shifts, and masks for all signals.
        This is intrinsic to the message definition.

        For big-endian messages, generated firmware treats multi-byte signals as
        byte-wise MSB-first fields located at byte boundaries. DBC output uses
        the corresponding Motorola start bit, while C packing keeps the LSB
        shift into the host-order uint64_t staging buffer.
        """
        current_offset = 0
        for sig in self.signals:
            length = sig.get_bit_length(custom_types)
            sig.length = length
            
            if self.byte_order == 'big_endian':
                if length > 8 and (current_offset % 8 != 0 or length not in (16, 32, 64)):
                    print_as_error(
                        f"Signal '{sig.name}' in big-endian message '{self.name}' "
                        "must be byte-aligned with a 16, 32, or 64-bit length. "
                        "Arbitrary Motorola bitfields are not supported."
                    )
                    raise ValueError("Unsupported big-endian signal layout")

                msb_byte = current_offset // 8
                msb_bit_in_byte = 7 - (current_offset % 8)
                lsb_pos = current_offset + length - 1
                lsb_byte = lsb_pos // 8

                if msb_byte == lsb_byte:
                    # Sub-byte signal, no byte boundary crossed:
                    # Intel (little-endian) in DBC using LSB position
                    sig.bit_offset = current_offset # LSB position, Intel numbering
                    sig.bit_shift = current_offset
                    sig.byte_order = "little_endian"
                else:
                    # Multi-byte Motorola signal: use sawtooth MSB start bit
                    sig.bit_offset = msb_byte * 8 + msb_bit_in_byte
                    sig.bit_shift = current_offset
                    sig.byte_order = "big_endian"
            else:
                sig.bit_offset = current_offset
                sig.bit_shift = current_offset
                sig.byte_order = "little_endian"
                
            sig.mask = (1 << length) - 1
            
            # Resolve signedness
            base_type = sig.datatype
            if custom_types and sig.datatype in custom_types:
                base_type = custom_types[sig.datatype].base_type
            sig.is_signed = base_type.startswith('int')
            
            current_offset += length
        
        self.dlc = (current_offset + 7) // 8
        self.layout_hash = get_layout_hash(self)

    def validate_semantics(self, custom_types: Dict[str, CustomTypeConfig]) -> None:
        """
        Perform semantic checks that require external context (like custom types).
        Raises ValueError if invalid.
        """
        total_length = sum(sig.get_bit_length(custom_types) for sig in self.signals)
        signal_names: Set[str] = set()

        for sig in self.signals:
            if sig.name in signal_names:
                print_as_error(f"Message '{self.name}' has duplicate signal name '{sig.name}'")
                raise ValueError(f"Duplicate signal name '{sig.name}' in message '{self.name}'")
            signal_names.add(sig.name)

            if sig.datatype not in CTYPE_SIZES and sig.datatype not in custom_types:
                print_as_error(f"Signal '{sig.name}' in message '{self.name}' has unknown type '{sig.datatype}'")
                raise ValueError("Unknown signal type")

        if total_length > 64:
            print_as_error(f"Message '{self.name}' exceeds 64 bits (has {total_length})")
            raise ValueError("Message too long")
        
        if self.id_override:
            try:
                raw_id = int(self.id_override, 0)
            except ValueError:
                print_as_error(f"Message '{self.name}' has invalid override ID format: '{self.id_override}'")
                raise ValueError(f"Invalid ID override format for {self.name}")

            if not self.is_extended and raw_id > 0x7FF:
                print_as_error(f"Message '{self.name}' has override ID {hex(raw_id)} which exceeds 11-bit limit (0x7FF) for standard message.")
                raise ValueError(f"ID override too large for standard message: {self.name}")
            
            if self.is_extended and raw_id > 0x1FFFFFFF:
                print_as_error(f"Message '{self.name}' has override ID {hex(raw_id)} which exceeds 29-bit limit (0x1FFFFFFF) for extended message.")
                raise ValueError(f"ID override too large for extended message: {self.name}")

    def get_total_bit_length(
        self, custom_types: Optional[Dict[str, CustomTypeConfig]] = None
    ) -> int:
        return sum(sig.get_bit_length(custom_types) for sig in self.signals)

    def get_dlc(self, custom_types: Dict[str, CustomTypeConfig]) -> int:
        """Calculate the Data Length Code (DLC) in bytes."""
        total_bits = self.get_total_bit_length(custom_types)
        return (total_bits + 7) // 8

@dataclass
class RxMessage:
    name: str
    callback: bool = False
    resolved_message: Optional[Message] = None # Resolved during linking stage

@dataclass
class Bus:
    name: str
    peripheral: str
    tx_messages: List[Message] = field(default_factory=list)
    rx_messages: List[RxMessage] = field(default_factory=list)
    accept_all_messages: bool = False

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)

@dataclass
class Node:
    name: str
    busses: Dict[str, Bus] = field(default_factory=dict)
    is_external: bool = False

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)

    @property
    def messages(self) -> List[Message]:
        """Returns all TX messages across all busses for this node."""
        all_msgs = []
        for bus in self.busses.values():
            all_msgs.extend(bus.tx_messages)
        return all_msgs

@dataclass
class BusView:
    name: str
    messages: List[Message] = field(default_factory=list)
    nodes: Set[str] = field(default_factory=set)
    sender_map: Dict[str, str] = field(default_factory=dict) # message_name -> sender_node_name

@dataclass
class SystemContext:
    nodes: List[Node] = field(default_factory=list)
    busses: Dict[str, BusView] = field(default_factory=dict)
    mappings: Dict[str, 'NodeMapping'] = field(default_factory=dict)
    bus_configs: Dict[str, BusConfig] = field(default_factory=dict)
    custom_types: Dict[str, CustomTypeConfig] = field(default_factory=dict)
    version: str = ""

def create_system_context(
    nodes: List[Node],
    mappings: Dict[str, 'NodeMapping'],
    bus_configs: Dict[str, BusConfig],
    custom_types: Dict[str, CustomTypeConfig],
) -> SystemContext:
    """Aggregates all system data into a single context object and performs final validation."""
    
    context = SystemContext(
        nodes=nodes, 
        mappings=mappings,
        bus_configs=bus_configs,
        custom_types=custom_types,
        version=get_git_hash()
    )
    
    # Identify all busses across all nodes
    all_bus_names = set()
    for node in nodes:
        all_bus_names.update(node.busses)
    
    for bus_name in sorted(all_bus_names):
        view = BusView(name=bus_name)
        
        for node in nodes:
            if bus_name in node.busses:
                view.nodes.add(node.name)
                for msg in node.busses[bus_name].tx_messages:
                    view.messages.append(msg)
                    view.sender_map[msg.name] = node.name
        
        # Deterministic sorting for downstream generators
        view.messages.sort(key=lambda x: (x.final_id or 0, x.name))
        context.busses[bus_name] = view
        
    return context

# --- Parsing Logic ---

def parse_all(config: ConfigBundle) -> List[Node]:
    """
    Parse all configuration files into Node objects.
    Performs semantic validation during parsing.
    """

    print("Parsing configs and performing semantic validation...")

    nodes = []
    custom_types = config.custom_types
    bus_configs = config.buses

    # Parse Internal Nodes
    for node_config in config.internal_nodes:
        node = parse_internal_node(node_config, bus_configs)
        validate_node(node, custom_types)
        nodes.append(node)
        print_as_ok(f"Parsed {node.name}")

    # Parse External Nodes
    for node_config in config.external_nodes:
        node = parse_external_node(node_config, bus_configs)
        validate_node(node, custom_types)
        nodes.append(node)
        print_as_ok(f"Parsed {node.name}")
    
    print_as_success("All nodes parsed successfully");
    return nodes

def validate_node(node: Node, custom_types: Dict[str, CustomTypeConfig]) -> None:
    """Run semantic validation and resolve bit layouts for a node"""
    for bus in node.busses.values():
        for msg in bus.tx_messages:
            msg.validate_semantics(custom_types)
            msg.resolve_layout(custom_types)
            warn_priority_period_convention(node, bus, msg)


def _priority_period_convention_warning_reason(msg: Message) -> Optional[str]:
    """Return the priority-period convention warning reason, if one applies."""
    if msg.priority == 0 and msg.period > 0:
        return (
            "priority 0 is event-based, but period_ms is positive "
            "(use 0 or omit period_ms for event-triggered messages)"
        )

    if msg.priority in (1, 2) and msg.period == 0:
        return (
            f"priority {msg.priority} is periodic by convention, but period_ms is 0 "
            "(set a positive period_ms for periodic messages)"
        )

    if msg.priority == 4 and msg.period <= 200:
        return (
            "priority 4 is low-frequency periodic telemetry (<5 Hz), but period_ms "
            "is not greater than 200 ms"
        )

    if msg.priority == 5 and (msg.period == 0 or msg.period >= 200):
        return (
            "priority 5 is high-frequency telemetry (>5 Hz), but period_ms is not "
            "between 1 and 199 ms"
        )

    return None


def warn_priority_period_convention(node: Node, bus: Bus, msg: Message) -> None:
    """Warn when a message's period conflicts with the README priority convention."""
    reason = _priority_period_convention_warning_reason(msg)
    if reason is None:
        return

    print_as_warning(
        f"Node '{node.name}': Bus '{bus.name}': Message '{msg.name}' violates "
        f"priority-period convention: priority={msg.priority}, "
        f"period={msg.period} ms. {reason}."
    )

def parse_signal(data: SignalConfig) -> Signal:
    return Signal(
        name=data.signal_name,
        datatype=data.data_type,
        desc=data.description,
        length=data.length or 0,
        unit=data.unit,
        choices=data.choices,
        scale=data.scale,
        offset=data.offset,
        min_val=data.min,
        max_val=data.max,
    )

def parse_message(
    data: TxMessageConfig, bus_config: BusConfig
) -> Message:
    # Single source of truth: the bus configuration
    is_extended = bus_config.is_extended_id

    return Message(
        name=data.message_name,
        desc=data.description,
        signals=[parse_signal(signal) for signal in data.signals],
        priority=data.priority,
        period=data.period_ms,
        id_override=data.id_override,
        byte_order=data.byte_order,
        is_extended=is_extended,
    )

def parse_rx_message(data: RxMessageConfig) -> RxMessage:
    return RxMessage(
        name=data.message_name,
        callback=data.callback,
    )

def parse_bus(
    name: str,
    data: BusAttachmentConfig,
    bus_config: BusConfig,
) -> Bus:
    return Bus(
        name=name,
        peripheral=data.peripheral,
        tx_messages=[parse_message(message, bus_config) for message in data.tx],
        rx_messages=[parse_rx_message(message) for message in data.rx],
        accept_all_messages=data.accept_all_messages,
    )

def parse_internal_node(
    data: InternalNodeConfig, bus_configs: Dict[str, BusConfig]
) -> Node:
    return Node(
        name=data.node_name,
        busses={
            bus_name: parse_bus(bus_name, bus_data, bus_configs[bus_name])
            for bus_name, bus_data in data.busses.items()
        },
        is_external=False,
    )

def parse_external_node(
    data: ExternalNodeConfig, bus_configs: Dict[str, BusConfig]
) -> Node:
    bus_name = data.bus_name
    bus_config = bus_configs[bus_name]
    bus = Bus(
        name=bus_name,
        peripheral="UNKNOWN",
        tx_messages=[parse_message(message, bus_config) for message in data.tx],
        rx_messages=[parse_rx_message(message) for message in data.rx],
    )

    return Node(
        name=data.node_name,
        busses={bus_name: bus},
        is_external=True,
    )
