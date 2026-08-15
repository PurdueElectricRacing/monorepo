"""
codegen.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass, field
from typing import List, Dict, Optional, Any
from parser import Node, Message, SystemContext
from utils import GENERATED_DIR, print_as_success, print_as_ok, get_jinja_env, render_template


@dataclass
class SignalCodec:
    signal: Any
    bswap_width: str
    sign_extend_shift: Optional[int]
    is_float32: bool


@dataclass
class RxEntry:
    rx_msg: Any
    msg: Message
    periph: str
    bus_name: str
    codecs: List[SignalCodec]


@dataclass
class TxEntry:
    msg: Message
    periph: str
    bus_name: str
    enqueue_func: str
    codecs: List[SignalCodec]


@dataclass
class ScalingMessage:
    msg: Message
    signals: List[Any]
    emit_unpack: bool = False
    emit_pack: bool = False

@dataclass
class OffsetMessage:
    msg: Message
    signals: List[Any]

@dataclass
class PeripheralContext:
    name: str
    enqueue_func: str
    queue_name: str
    bus_type: str
    arch_define: str


@dataclass
class RxPeripheralContext:
    peripheral: PeripheralContext
    entries: List[RxEntry]


@dataclass
class FdcanFilterContext:
    periph: str
    accept_all: bool
    std_ids: List[Message] = field(default_factory=list)
    ext_ids: List[Message] = field(default_factory=list)

    @property
    def has_filters(self) -> bool:
        return bool(self.std_ids or self.ext_ids)


@dataclass
class BxcanFilterBankContext:
    bank_idx: int
    msg1: Message
    msg2: Message
    is_ext1: bool
    is_ext2: bool
    has_second_msg: bool


@dataclass
class BxcanFilterContext:
    periph: str
    accept_all: bool
    accept_bank_idx: int
    banks: List[BxcanFilterBankContext] = field(default_factory=list)


@dataclass
class FilterRenderContext:
    fdcan: List[FdcanFilterContext] = field(default_factory=list)
    bxcan: List[BxcanFilterContext] = field(default_factory=list)

    @property
    def has_fdcan(self) -> bool:
        return bool(self.fdcan)

    @property
    def has_bxcan(self) -> bool:
        return bool(self.bxcan)


@dataclass
class NodeRenderContext:
    node: Node
    context: SystemContext
    mapping: Any
    rx_entries: List[RxEntry]
    rx_peripheral_entries: List[RxPeripheralContext]
    tx_entries: List[TxEntry]
    peripherals: List[str]
    peripheral_entries: List[PeripheralContext]
    node_busses: List[str]
    scaling_messages: List[ScalingMessage]
    offset_messages: List[OffsetMessage]
    stale_rx_entries: List[RxEntry]
    filters: FilterRenderContext


@dataclass
class TypeRenderContext:
    name: str
    prefix: str
    base_type: Optional[str]
    choices: List[str]


def build_type_render_context(custom_types: Dict) -> List[TypeRenderContext]:
    rows = []
    for name, config in custom_types.items():
        prefix = name[:-2].upper() if name.endswith("_t") else name.upper()
        rows.append(TypeRenderContext(
            name=name,
            prefix=prefix,
            base_type=config.get("base_type"),
            choices=config.get("choices", []),
        ))
    return rows


def build_signal_codec(sig, direction: str) -> SignalCodec:
    if direction not in ("rx", "tx"):
        raise ValueError(f"Unknown codec direction: {direction}")

    bswap_width = "BSWAP_NONE"
    if sig.byte_order == "big_endian" and sig.length in (16, 32, 64):
        bswap_width = f"BSWAP_{sig.length}"

    return SignalCodec(
        signal=sig,
        bswap_width=bswap_width,
        sign_extend_shift=64 - sig.length if sig.is_signed and sig.length < 64 else None,
        is_float32=sig.is_floating_point and sig.length == 32,
    )


def build_scaling_messages(rx_entries: List[RxEntry], tx_entries: List[TxEntry]) -> List[ScalingMessage]:
    by_name: Dict[str, ScalingMessage] = {}

    def register_msg(msg: Message, *, emit_unpack: bool = False, emit_pack: bool = False) -> None:
        signals = [sig for sig in msg.signals if sig.scale is not None and sig.scale != 1.0]
        if not signals:
            return

        scaling_msg = by_name.setdefault(msg.name, ScalingMessage(msg=msg, signals=signals))
        scaling_msg.emit_unpack |= emit_unpack
        scaling_msg.emit_pack |= emit_pack

    for entry in tx_entries:
        register_msg(entry.msg, emit_pack=True)
    for entry in rx_entries:
        register_msg(entry.msg, emit_unpack=True)

    return sorted(by_name.values(), key=lambda scaling: scaling.msg.name)


def build_offset_messages(rx_entries: List[RxEntry], tx_entries: List[TxEntry]) -> List[OffsetMessage]:
    by_name: Dict[str, OffsetMessage] = {}

    def register_msg(msg: Message) -> None:
        signals = [sig for sig in msg.signals if sig.offset is not None and sig.offset != 0.0]
        if not signals:
            return

        by_name.setdefault(msg.name, OffsetMessage(msg=msg, signals=signals))

    for entry in tx_entries:
        register_msg(entry.msg)
    for entry in rx_entries:
        register_msg(entry.msg)

    return sorted(by_name.values(), key=lambda offset: offset.msg.name)


def build_filter_render_context(mapping, peripherals: List[str]) -> FilterRenderContext:
    filters = FilterRenderContext()
    if mapping is None:
        return filters

    for periph in peripherals:
        accept_all = mapping.accept_all.get(periph, False)
        if periph.startswith("FDCAN"):
            fdcan_filt = mapping.fdcan_filters.get(periph)
            filters.fdcan.append(FdcanFilterContext(
                periph=periph,
                accept_all=accept_all,
                std_ids=list(fdcan_filt.std_ids) if fdcan_filt else [],
                ext_ids=list(fdcan_filt.ext_ids) if fdcan_filt else [],
            ))
            continue

        banks = []
        for fb in mapping.filters.get(periph, []):
            banks.append(BxcanFilterBankContext(
                bank_idx=fb.bank_idx,
                msg1=fb.msg1,
                msg2=fb.msg2 if fb.msg2 else fb.msg1,
                is_ext1=fb.is_ext1,
                is_ext2=fb.is_ext2 if fb.msg2 else fb.is_ext1,
                has_second_msg=fb.msg2 is not None,
            ))

        filters.bxcan.append(BxcanFilterContext(
            periph=periph,
            accept_all=accept_all,
            accept_bank_idx=0 if periph == "CAN1" else 14,
            banks=banks,
        ))
    return filters


def build_peripheral_contexts(peripherals: List[str]) -> List[PeripheralContext]:
    entries = []
    for periph in peripherals:
        if periph.startswith("FDCAN"):
            bus_type = "FDCAN_GlobalTypeDef"
            arch_define = "STM32G474xx"
        elif periph.startswith("CAN"):
            bus_type = "CAN_TypeDef"
            arch_define = "STM32F407xx"
        else:
            raise ValueError(f"Unsupported CAN peripheral: {periph}")

        entries.append(PeripheralContext(
            name=periph,
            enqueue_func=f"CAN_enqueue_tx_{periph}",
            queue_name=f"can{periph[-1]}_tx_queue",
            bus_type=bus_type,
            arch_define=arch_define,
        ))
    return entries


def build_rx_peripheral_entries(
    rx_entries: List[RxEntry],
    peripheral_entries: List[PeripheralContext],
) -> List[RxPeripheralContext]:
    rx_by_periph: Dict[str, List[RxEntry]] = {periph.name: [] for periph in peripheral_entries}
    for entry in rx_entries:
        rx_by_periph[entry.periph].append(entry)

    return [
        RxPeripheralContext(peripheral=periph, entries=rx_by_periph[periph.name])
        for periph in peripheral_entries
        if rx_by_periph[periph.name]
    ]


def build_enqueue_func(periph: str) -> str:
    return f"CAN_enqueue_tx_{periph}"


def build_node_render_context(node: Node, context: SystemContext) -> NodeRenderContext:
    mapping = context.mappings.get(node.name)
    rx_entries: List[RxEntry] = []
    tx_entries: List[TxEntry] = []
    peripherals = sorted(list(set(bus.peripheral for bus in node.busses.values())))
    peripheral_entries = build_peripheral_contexts(peripherals)
    bus_types = {periph.bus_type for periph in peripheral_entries}
    arch_defines = {periph.arch_define for periph in peripheral_entries}

    if len(bus_types) != 1 or len(arch_defines) != 1:
        raise ValueError(f"Node {node.name} mixes incompatible CAN peripheral families")

    node_busses = sorted(node.busses.keys())

    for bus_name in node_busses:
        bus = node.busses[bus_name]
        for rx_msg in bus.rx_messages:
            if rx_msg.resolved_message:
                msg = rx_msg.resolved_message
                rx_entries.append(RxEntry(
                    rx_msg=rx_msg,
                    msg=msg,
                    periph=bus.peripheral,
                    bus_name=bus_name,
                    codecs=[build_signal_codec(sig, "rx") for sig in msg.signals],
                ))
        for msg in bus.tx_messages:
            tx_entries.append(TxEntry(
                msg=msg,
                periph=bus.peripheral,
                bus_name=bus_name,
                enqueue_func=build_enqueue_func(bus.peripheral),
                codecs=[build_signal_codec(sig, "tx") for sig in msg.signals],
            ))

    return NodeRenderContext(
        node=node,
        context=context,
        mapping=mapping,
        rx_entries=rx_entries,
        rx_peripheral_entries=build_rx_peripheral_entries(rx_entries, peripheral_entries),
        tx_entries=tx_entries,
        peripherals=peripherals,
        peripheral_entries=peripheral_entries,
        node_busses=node_busses,
        scaling_messages=build_scaling_messages(rx_entries, tx_entries),
        offset_messages=build_offset_messages(rx_entries, tx_entries),
        stale_rx_entries=[entry for entry in rx_entries if entry.msg.period > 0],
        filters=build_filter_render_context(mapping, peripherals),
    )


def generate_headers(context: SystemContext):
    print("Generating headers...")
    env = get_jinja_env()
    
    # Ensure generated directory exists
    GENERATED_DIR.mkdir(exist_ok=True)

    # Generate types header
    generate_types_header(env, context.custom_types)

    # Generate header for each bus
    for bus_name, view in context.busses.items():
        config = context.bus_configs.get(bus_name, {})
        generate_bus_header(env, bus_name, config, view.messages)

    # Generate header for each node
    generate_node_headers(env, context)

    # Generate router header
    generate_router_header(env, context.nodes)
    
    print_as_success("Successfully generated C headers")

def generate_types_header(env, custom_types: Dict):
    render_template(env, 'can_types.h.jinja', 
                    GENERATED_DIR / "can_types.h", 
                    types=build_type_render_context(custom_types))
    print_as_ok("Generated can_types.h")

def generate_router_header(env, nodes: List[Node]):
    render_template(env, 'can_router.h.jinja',
                    GENERATED_DIR / "can_router.h",
                    nodes=nodes,
                    router_nodes=[node for node in nodes if not node.is_external])
    print_as_ok("Generated can_router.h")

def generate_node_headers(env, context: SystemContext):
    for node in context.nodes:
        if node.is_external:
            continue
        generate_node_header(env, node, context)

def generate_node_header(env, node: Node, context: SystemContext):
    filename = GENERATED_DIR / f"{node.name}.h"
    render_context = build_node_render_context(node, context)

    render_template(env, 'node_header.h.jinja',
                    filename,
                    ctx=render_context)
    print_as_ok(f"Generated {filename.name}")

def generate_bus_header(env, bus_name: str, config: Dict, messages: List[Message]):
    render_template(env, 'bus_header.h.jinja',
                    GENERATED_DIR / f"{bus_name}.h",
                    bus_name=bus_name,
                    config=config,
                    messages=messages)
    print_as_ok(f"Generated {bus_name}.h")
