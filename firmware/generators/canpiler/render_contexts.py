"""
render_contexts.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections import defaultdict
from typing import Mapping

from core.config_models import CustomTypeDeclaration
from .ir import LinkedCan, SignalIR
from .mapper import FdcanFilters, NodeMapping
from .render_models import (
    BxcanFilterBankRenderView,
    BxcanFilterRenderView,
    BusAttachmentRenderView,
    FdcanFilterRenderView,
    FilterRenderView,
    NodeHeaderRenderContext,
    NodeRenderView,
    OffsetConstantsRenderView,
    PeripheralRenderView,
    RxMessageRenderView,
    RxPeripheralRenderView,
    ScalingConstantsRenderView,
    SignalCodecRenderView,
    TxMessageRenderView,
    TypeRenderView,
)


def build_node_render_views(
    linked: LinkedCan,
) -> tuple[NodeRenderView, ...]:
    nodes = []

    for node in linked.nodes:
        busses = {}

        for bus_name, attachment in node.busses.items():
            tx_messages = tuple(
                item.message
                for item in linked.tx_messages
                if item.bus_name == bus_name
                and item.node_name == node.name
            )
            subscriptions = tuple(
                item
                for item in linked.subscriptions
                if item.node_name == node.name and item.bus_name == bus_name
            )

            busses[bus_name] = BusAttachmentRenderView(
                name=bus_name,
                peripheral=attachment.peripheral,
                tx_messages=tx_messages,
                rx_subscriptions=subscriptions,
                accept_all_messages=attachment.accept_all_messages,
            )

        nodes.append(
            NodeRenderView(
                node.name,
                busses,
                node.is_external,
            )
        )

    return tuple(nodes)


def build_type_render_views(
    custom_types: Mapping[str, CustomTypeDeclaration],
) -> tuple[TypeRenderView, ...]:
    return tuple(TypeRenderView(
        name=name,
        prefix=name[:-2].upper() if name.endswith("_t") else name.upper(),
        base_type=config.base_type,
        choices=tuple(config.choices or ()),
    ) for name, config in custom_types.items())


def build_signal_codec(signal: SignalIR) -> SignalCodecRenderView:
    bswap_width = "BSWAP_NONE"
    if signal.byte_order == "big_endian" and signal.length in (16, 32, 64):
        bswap_width = f"BSWAP_{signal.length}"
    return SignalCodecRenderView(
        signal=signal,
        bswap_width=bswap_width,
        sign_extend_shift=(
            64 - signal.length if signal.is_signed and signal.length < 64 else None
        ),
        is_float32=signal.data_type == "float" and signal.length == 32,
    )


def build_peripheral_views(peripherals: tuple[str, ...]) -> tuple[PeripheralRenderView, ...]:
    views = []
    for peripheral in peripherals:
        if peripheral.startswith("FDCAN"):
            bus_type, arch_define = "FDCAN_GlobalTypeDef", "STM32G474xx"
        elif peripheral.startswith("CAN"):
            bus_type, arch_define = "CAN_TypeDef", "STM32F407xx"
        else:
            raise ValueError(f"Unsupported CAN peripheral: {peripheral}")
        views.append(PeripheralRenderView(
            name=peripheral,
            enqueue_func=f"CAN_enqueue_tx_{peripheral}",
            queue_name=f"can{peripheral[-1]}_tx_queue",
            bus_type=bus_type,
            arch_define=arch_define,
        ))
    return tuple(views)


def build_filter_view(
    mapping: NodeMapping | None,
    peripherals: tuple[str, ...],
) -> FilterRenderView:
    if mapping is None:
        return FilterRenderView()
    fdcan = []
    bxcan = []
    for peripheral in peripherals:
        filters = mapping.filters[peripheral]
        if isinstance(filters, FdcanFilters):
            fdcan.append(FdcanFilterRenderView(
                periph=peripheral,
                accept_all=filters.accept_all,
                std_ids=filters.std_ids,
                ext_ids=filters.ext_ids,
            ))
        else:
            banks = tuple(BxcanFilterBankRenderView(
                bank_idx=bank.bank_idx,
                msg1=bank.msg1,
                msg2=bank.msg2 or bank.msg1,
                is_ext1=bank.msg1.is_extended,
                is_ext2=(
                    bank.msg2.is_extended
                    if bank.msg2
                    else bank.msg1.is_extended
                ),
                has_second_msg=bank.msg2 is not None,
            ) for bank in filters.banks)
            bxcan.append(BxcanFilterRenderView(
                periph=peripheral,
                accept_all=filters.accept_all,
                accept_bank_idx=filters.accept_bank_idx,
                banks=banks,
            ))
    return FilterRenderView(tuple(fdcan), tuple(bxcan))


def build_node_header_context(
    node: NodeRenderView,
    mapping: NodeMapping | None,
    version: str,
) -> NodeHeaderRenderContext:
    peripherals = tuple(sorted({bus.peripheral for bus in node.busses.values()}))
    peripheral_views = build_peripheral_views(peripherals)
    if len({item.bus_type for item in peripheral_views}) != 1 or len({
        item.arch_define for item in peripheral_views
    }) != 1:
        raise ValueError(f"Node {node.name} mixes incompatible CAN peripheral families")

    rx_entries = []
    tx_entries = []
    for bus_name in sorted(node.busses):
        bus = node.busses[bus_name]
        for subscription in bus.rx_subscriptions:
            message = subscription.resolved_message
            rx_entries.append(RxMessageRenderView(
                rx_msg=subscription,
                msg=message,
                periph=bus.peripheral,
                bus_name=bus_name,
                codecs=tuple(build_signal_codec(signal) for signal in message.signals),
            ))
        for message in bus.tx_messages:
            tx_entries.append(TxMessageRenderView(
                msg=message,
                periph=bus.peripheral,
                bus_name=bus_name,
                enqueue_func=f"CAN_enqueue_tx_{bus.peripheral}",
                codecs=tuple(build_signal_codec(signal) for signal in message.signals),
            ))

    rx_by_peripheral = defaultdict(list)
    for entry in rx_entries:
        rx_by_peripheral[entry.periph].append(entry)
    rx_peripheral_entries = tuple(
        RxPeripheralRenderView(view, tuple(rx_by_peripheral[view.name]))
        for view in peripheral_views if rx_by_peripheral[view.name]
    )

    directions: dict[str, list[object]] = {}
    offsets: dict[str, OffsetConstantsRenderView] = {}
    for entry, unpack, pack in (
        *((entry, False, True) for entry in tx_entries),
        *((entry, True, False) for entry in rx_entries),
    ):
        message = entry.msg
        scaled = tuple(signal for signal in message.signals if signal.scale is not None)
        if scaled:
            current = directions.setdefault(
                message.message_name,
                [message, scaled, False, False],
            )
            current[2] = current[2] or unpack
            current[3] = current[3] or pack
        offset_signals = tuple(signal for signal in message.signals if signal.offset is not None)
        if offset_signals:
            offsets.setdefault(
                message.message_name,
                OffsetConstantsRenderView(message, offset_signals),
            )

    scaling = tuple(
        ScalingConstantsRenderView(item[0], item[1], item[2], item[3])
        for _, item in sorted(directions.items())
    )
    offset_views = tuple(value for _, value in sorted(offsets.items()))

    return NodeHeaderRenderContext(
        node=node,
        version=version,
        rx_entries=tuple(rx_entries),
        rx_peripheral_entries=rx_peripheral_entries,
        tx_entries=tuple(tx_entries),
        peripherals=peripherals,
        peripheral_entries=peripheral_views,
        node_busses=tuple(sorted(node.busses)),
        scaling_messages=scaling,
        offset_messages=offset_views,
        stale_rx_entries=tuple(entry for entry in rx_entries if entry.msg.period_ms > 0),
        filters=build_filter_view(mapping, peripherals),
    )
