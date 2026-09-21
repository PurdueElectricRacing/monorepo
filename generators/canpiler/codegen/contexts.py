"""
codegen/contexts.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from collections import defaultdict

from ..pipeline_models import CompiledNode, CompiledSignal, LinkedCan, frozen_mapping
from .hardware import FdcanFilters, NodeHardwareMap
from .models import (
    NodeHeaderRenderContext,
    PeripheralRenderView,
    RxMessageRenderView,
    SignalConstantsRenderView,
    SignalCodecRenderView,
    TxMessageRenderView,
)


def build_signal_codec(signal: CompiledSignal) -> SignalCodecRenderView:
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
        views.append(
            PeripheralRenderView(
                name=peripheral,
                enqueue_func=f"CAN_enqueue_tx_{peripheral}",
                bus_type=bus_type,
                arch_define=arch_define,
            )
        )
    return tuple(views)


def build_node_header_context(
    node: CompiledNode,
    linked: LinkedCan,
    mapping: NodeHardwareMap | None,
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
        subscriptions = (
            item
            for item in linked.subscriptions
            if item.node_name == node.name and item.bus_name == bus_name
        )
        messages = (
            item.message
            for item in linked.tx_messages
            if item.node_name == node.name and item.bus_name == bus_name
        )

        for subscription in subscriptions:
            message = subscription.resolved_message
            rx_entries.append(
                RxMessageRenderView(
                    rx_msg=subscription,
                    msg=message,
                    periph=bus.peripheral,
                    codecs=tuple(
                        build_signal_codec(signal)
                        for signal in message.signals
                    ),
                )
            )
        for message in messages:
            tx_entries.append(
                TxMessageRenderView(
                    msg=message,
                    periph=bus.peripheral,
                    enqueue_func=f"CAN_enqueue_tx_{bus.peripheral}",
                    codecs=tuple(
                        build_signal_codec(signal)
                        for signal in message.signals
                    ),
                )
            )

    rx_by_peripheral = defaultdict(list)
    for entry in rx_entries:
        rx_by_peripheral[entry.periph].append(entry)
    rx_entries_by_peripheral = frozen_mapping({
        view.name: tuple(rx_by_peripheral[view.name])
        for view in peripheral_views
        if rx_by_peripheral[view.name]
    })

    directions: dict[str, list[object]] = {}
    offsets: dict[str, SignalConstantsRenderView] = {}
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
                SignalConstantsRenderView(message, offset_signals),
            )

    scaling = tuple(
        SignalConstantsRenderView(item[0], item[1], item[2], item[3])
        for _, item in sorted(directions.items())
    )
    offset_views = tuple(value for _, value in sorted(offsets.items()))

    fdcan_filters = {}
    bxcan_filters = {}
    if mapping is not None:
        for peripheral, filters in mapping.filters.items():
            if isinstance(filters, FdcanFilters):
                fdcan_filters[peripheral] = filters
            else:
                bxcan_filters[peripheral] = filters

    return NodeHeaderRenderContext(
        node=node,
        version=version,
        rx_entries=tuple(rx_entries),
        rx_entries_by_peripheral=rx_entries_by_peripheral,
        tx_entries=tuple(tx_entries),
        peripheral_entries=peripheral_views,
        node_busses=tuple(sorted(node.busses)),
        scaling_messages=scaling,
        offset_messages=offset_views,
        stale_rx_entries=tuple(entry for entry in rx_entries if entry.msg.period_ms > 0),
        fdcan_filters=frozen_mapping(fdcan_filters),
        bxcan_filters=frozen_mapping(bxcan_filters),
    )
