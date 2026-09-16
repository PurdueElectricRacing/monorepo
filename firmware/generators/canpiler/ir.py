"""
ir.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

from dataclasses import dataclass
from types import MappingProxyType
from typing import Mapping

from core.config_models import (
    BusConfig,
    ByteOrder,
    CustomTypeDeclaration,
    MessageDeclaration,
    RxSubscriptionDeclaration,
)
from core.utils import to_macro_name


def frozen_mapping(values: Mapping) -> Mapping:
    """Return a defensive, read-only copy for immutable pipeline stages."""
    return MappingProxyType(dict(values))


@dataclass(frozen=True)
class BuildMetadata:
    version: str


@dataclass(frozen=True)
class SourceBusAttachment:
    bus_name: str
    peripheral: str
    tx_messages: tuple[MessageDeclaration, ...]
    rx_subscriptions: tuple[RxSubscriptionDeclaration, ...]
    accept_all_messages: bool = False


@dataclass(frozen=True)
class SourceNode:
    node_name: str
    busses: tuple[SourceBusAttachment, ...]
    is_external: bool = False


@dataclass(frozen=True)
class CanSource:
    nodes: tuple[SourceNode, ...]
    bus_definitions: Mapping[str, BusConfig]
    custom_types: Mapping[str, CustomTypeDeclaration]


@dataclass(frozen=True)
class SignalIR:
    signal_name: str
    data_type: str
    description: str
    length: int
    unit: str | None
    choices: tuple[str, ...] | None
    scale: int | float | None
    offset: int | float | None
    min: int | float | None
    max: int | float | None
    byte_order: ByteOrder
    bit_offset: int
    bit_shift: int
    is_signed: bool
    mask: int

    @property
    def name(self) -> str:
        return self.signal_name

    @property
    def datatype(self) -> str:
        return self.data_type

    @property
    def desc(self) -> str:
        return self.description

    @property
    def min_val(self) -> int | float | None:
        return self.min

    @property
    def max_val(self) -> int | float | None:
        return self.max

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.signal_name)

    @property
    def c_type(self) -> str:
        return self.data_type

    @property
    def is_floating_point(self) -> bool:
        return self.data_type == "float"

    @property
    def is_reserved(self) -> bool:
        return self.signal_name.startswith("reserved")


@dataclass(frozen=True)
class MessageIR:
    message_name: str
    description: str
    signals: tuple[SignalIR, ...]
    priority: int
    period_ms: int
    id_override: int | None
    is_extended: bool
    byte_order: ByteOrder
    dlc: int
    layout_hash: str

    @property
    def name(self) -> str:
        return self.message_name

    @property
    def desc(self) -> str:
        return self.description

    @property
    def period(self) -> int:
        return self.period_ms

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.message_name)

    def get_dlc(self, custom_types: object = None) -> int:
        return self.dlc


@dataclass(frozen=True, order=True)
class MessageKey:
    bus_name: str
    message_name: str


@dataclass(frozen=True)
class BusAttachmentIR:
    name: str
    peripheral: str
    accept_all_messages: bool = False

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)


@dataclass(frozen=True)
class NodeIR:
    name: str
    busses: Mapping[str, BusAttachmentIR]
    is_external: bool = False

    @property
    def macro_name(self) -> str:
        return to_macro_name(self.name)



@dataclass(frozen=True)
class TxMessageIR:
    node_name: str
    bus_name: str
    message: MessageIR

    @property
    def key(self) -> MessageKey:
        return MessageKey(self.bus_name, self.message.message_name)


@dataclass(frozen=True)
class RxSubscriptionIR:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool = False

    @property
    def key(self) -> MessageKey:
        return MessageKey(self.bus_name, self.message_name)


@dataclass(frozen=True)
class CanIR:
    nodes: tuple[NodeIR, ...]
    tx_messages: tuple[TxMessageIR, ...]
    rx_subscriptions: tuple[RxSubscriptionIR, ...]
    bus_definitions: Mapping[str, BusConfig]
    custom_types: Mapping[str, CustomTypeDeclaration]


@dataclass(frozen=True)
class LinkedMessage(MessageIR):
    final_id: int


@dataclass(frozen=True)
class LinkedRxSubscription:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool
    resolved_message: LinkedMessage

    @property
    def name(self) -> str:
        return self.message_name

@dataclass(frozen=True)
class LinkedCan:
    nodes: tuple[NodeIR, ...]
    messages: Mapping[MessageKey, LinkedMessage]
    transmitters: Mapping[MessageKey, str]
    tx_order: tuple[MessageKey, ...]
    subscriptions: tuple[LinkedRxSubscription, ...]
    bus_configs: Mapping[str, BusConfig]
    custom_types: Mapping[str, CustomTypeDeclaration]
