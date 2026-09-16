"""
render_models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass
from typing import Mapping

from .ir import (
    LinkedMessage,
    LinkedRxSubscription,
    SignalIR,
)


@dataclass(frozen=True)
class BusAttachmentRenderView:
    name: str
    peripheral: str
    tx_messages: tuple[LinkedMessage, ...]
    rx_subscriptions: tuple[LinkedRxSubscription, ...]
    accept_all_messages: bool

@dataclass(frozen=True)
class NodeRenderView:
    name: str
    busses: Mapping[str, BusAttachmentRenderView]
    is_external: bool

    @property
    def macro_name(self) -> str:
        return self.name.upper()

@dataclass(frozen=True)
class SignalCodecRenderView:
    signal: SignalIR
    bswap_width: str
    sign_extend_shift: int | None
    is_float32: bool


@dataclass(frozen=True)
class RxMessageRenderView:
    rx_msg: LinkedRxSubscription
    msg: LinkedMessage
    periph: str
    bus_name: str
    codecs: tuple[SignalCodecRenderView, ...]


@dataclass(frozen=True)
class TxMessageRenderView:
    msg: LinkedMessage
    periph: str
    bus_name: str
    enqueue_func: str
    codecs: tuple[SignalCodecRenderView, ...]


@dataclass(frozen=True)
class ScalingConstantsRenderView:
    msg: LinkedMessage
    signals: tuple[SignalIR, ...]
    emit_unpack: bool
    emit_pack: bool


@dataclass(frozen=True)
class OffsetConstantsRenderView:
    msg: LinkedMessage
    signals: tuple[SignalIR, ...]


@dataclass(frozen=True)
class PeripheralRenderView:
    name: str
    enqueue_func: str
    queue_name: str
    bus_type: str
    arch_define: str


@dataclass(frozen=True)
class RxPeripheralRenderView:
    peripheral: PeripheralRenderView
    entries: tuple[RxMessageRenderView, ...]


@dataclass(frozen=True)
class FdcanFilterRenderView:
    periph: str
    accept_all: bool
    std_ids: tuple[LinkedMessage, ...] = ()
    ext_ids: tuple[LinkedMessage, ...] = ()

    @property
    def has_filters(self) -> bool:
        return bool(self.std_ids or self.ext_ids)


@dataclass(frozen=True)
class BxcanFilterBankRenderView:
    bank_idx: int
    msg1: LinkedMessage
    msg2: LinkedMessage
    is_ext1: bool
    is_ext2: bool
    has_second_msg: bool


@dataclass(frozen=True)
class BxcanFilterRenderView:
    periph: str
    accept_all: bool
    accept_bank_idx: int
    banks: tuple[BxcanFilterBankRenderView, ...] = ()


@dataclass(frozen=True)
class FilterRenderView:
    fdcan: tuple[FdcanFilterRenderView, ...] = ()
    bxcan: tuple[BxcanFilterRenderView, ...] = ()

    @property
    def has_fdcan(self) -> bool:
        return bool(self.fdcan)

    @property
    def has_bxcan(self) -> bool:
        return bool(self.bxcan)


@dataclass(frozen=True)
class NodeHeaderRenderContext:
    node: NodeRenderView
    version: str
    rx_entries: tuple[RxMessageRenderView, ...]
    rx_peripheral_entries: tuple[RxPeripheralRenderView, ...]
    tx_entries: tuple[TxMessageRenderView, ...]
    peripherals: tuple[str, ...]
    peripheral_entries: tuple[PeripheralRenderView, ...]
    node_busses: tuple[str, ...]
    scaling_messages: tuple[ScalingConstantsRenderView, ...]
    offset_messages: tuple[OffsetConstantsRenderView, ...]
    stale_rx_entries: tuple[RxMessageRenderView, ...]
    filters: FilterRenderView


@dataclass(frozen=True)
class TypeRenderView:
    name: str
    prefix: str
    base_type: str | None
    choices: tuple[str, ...]
