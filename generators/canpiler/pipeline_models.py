"""
pipeline_models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

from dataclasses import dataclass
from types import MappingProxyType
from typing import Mapping

from core.declarations import (
    BusDeclaration,
    ByteOrder,
    CustomTypeDeclaration,
)
from core.contributions import RxDeclaration, TxDeclaration


def frozen_mapping(values: Mapping) -> Mapping:
    """Return a defensive, read-only copy for immutable pipeline stages."""
    return MappingProxyType(dict(values))


@dataclass(frozen=True)
class SourceBusAttachment:
    node_name: str
    bus_name: str
    peripheral: str
    accept_all_messages: bool = False


@dataclass(frozen=True)
class SourceNode:
    node_name: str
    is_external: bool = False


@dataclass(frozen=True)
class CanSource:
    nodes: tuple[SourceNode, ...]
    bus_attachments: tuple[SourceBusAttachment, ...]
    tx_messages: tuple[TxDeclaration, ...]
    rx_subscriptions: tuple[RxDeclaration, ...]
    bus_definitions: Mapping[str, BusDeclaration]
    custom_types: Mapping[str, CustomTypeDeclaration]


@dataclass(frozen=True)
class CompiledSignal:
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
    def macro_name(self) -> str:
        return self.signal_name.upper()

    @property
    def is_reserved(self) -> bool:
        return self.signal_name.startswith("reserved")


@dataclass(frozen=True)
class CompiledMessage:
    message_name: str
    description: str
    signals: tuple[CompiledSignal, ...]
    priority: int
    period_ms: int
    id_override: int | None
    is_extended: bool
    byte_order: ByteOrder
    dlc: int
    layout_hash: str

    @property
    def macro_name(self) -> str:
        return self.message_name.upper()


@dataclass(frozen=True, order=True)
class MessageKey:
    bus_name: str
    message_name: str


@dataclass(frozen=True)
class CompiledBusAttachment:
    name: str
    peripheral: str
    accept_all_messages: bool = False


@dataclass(frozen=True)
class CompiledNode:
    name: str
    busses: Mapping[str, CompiledBusAttachment]
    is_external: bool = False

@dataclass(frozen=True)
class CompiledTxMessage:
    node_name: str
    bus_name: str
    message: CompiledMessage

    @property
    def key(self) -> MessageKey:
        return MessageKey(self.bus_name, self.message.message_name)


@dataclass(frozen=True)
class CompiledRxSubscription:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool = False

    @property
    def key(self) -> MessageKey:
        return MessageKey(self.bus_name, self.message_name)


@dataclass(frozen=True)
class CompiledCan:
    nodes: tuple[CompiledNode, ...]
    tx_messages: tuple[CompiledTxMessage, ...]
    rx_subscriptions: tuple[CompiledRxSubscription, ...]
    bus_definitions: Mapping[str, BusDeclaration]
    custom_types: Mapping[str, CustomTypeDeclaration]


@dataclass(frozen=True)
class LinkedMessage(CompiledMessage):
    final_id: int


@dataclass(frozen=True)
class LinkedTxMessage:
    node_name: str
    bus_name: str
    message: LinkedMessage

@dataclass(frozen=True)
class LinkedRxSubscription:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool
    resolved_message: LinkedMessage


@dataclass(frozen=True)
class LinkedCan:
    nodes: tuple[CompiledNode, ...]
    tx_messages: tuple[LinkedTxMessage, ...]
    subscriptions: tuple[LinkedRxSubscription, ...]
    bus_configs: Mapping[str, BusDeclaration]
    custom_types: Mapping[str, CustomTypeDeclaration]
