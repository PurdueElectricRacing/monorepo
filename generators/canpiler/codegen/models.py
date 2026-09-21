"""
codegen/models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass
from typing import Mapping

from ..pipeline_models import (
    LinkedMessage,
    LinkedRxSubscription,
    CompiledNode,
    CompiledSignal,
)
from .hardware import BxcanFilters, FdcanFilters


@dataclass(frozen=True)
class SignalCodecRenderView:
    signal: CompiledSignal
    bswap_width: str
    sign_extend_shift: int | None
    is_float32: bool


@dataclass(frozen=True)
class RxMessageRenderView:
    rx_msg: LinkedRxSubscription
    msg: LinkedMessage
    periph: str
    codecs: tuple[SignalCodecRenderView, ...]


@dataclass(frozen=True)
class TxMessageRenderView:
    msg: LinkedMessage
    periph: str
    enqueue_func: str
    codecs: tuple[SignalCodecRenderView, ...]


@dataclass(frozen=True)
class SignalConstantsRenderView:
    msg: LinkedMessage
    signals: tuple[CompiledSignal, ...]
    emit_unpack: bool = False
    emit_pack: bool = False


@dataclass(frozen=True)
class PeripheralRenderView:
    name: str
    enqueue_func: str
    bus_type: str
    arch_define: str


@dataclass(frozen=True)
class NodeHeaderRenderContext:
    node: CompiledNode
    version: str
    rx_entries: tuple[RxMessageRenderView, ...]
    rx_entries_by_peripheral: Mapping[str, tuple[RxMessageRenderView, ...]]
    tx_entries: tuple[TxMessageRenderView, ...]
    peripheral_entries: tuple[PeripheralRenderView, ...]
    node_busses: tuple[str, ...]
    scaling_messages: tuple[SignalConstantsRenderView, ...]
    offset_messages: tuple[SignalConstantsRenderView, ...]
    stale_rx_entries: tuple[RxMessageRenderView, ...]
    fdcan_filters: Mapping[str, FdcanFilters]
    bxcan_filters: Mapping[str, BxcanFilters]
