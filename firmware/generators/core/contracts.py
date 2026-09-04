from dataclasses import dataclass, field
from typing import NotRequired, TypedDict

from core.config_models import CustomTypeConfig


class SignalContribution(TypedDict):
    name: str
    datatype: str
    desc: NotRequired[str]
    length: NotRequired[int]
    unit: NotRequired[str]
    choices: NotRequired[list[str]]
    scale: NotRequired[float]
    offset: NotRequired[float]
    min_val: NotRequired[float]
    max_val: NotRequired[float]


class MessageContribution(TypedDict):
    name: str
    desc: NotRequired[str]
    priority: NotRequired[int]
    period: NotRequired[int]
    signals: NotRequired[list[SignalContribution]]


@dataclass
class TxMessageContribution:
    node_name: str
    bus_name: str
    message: MessageContribution


@dataclass
class RxSubscriptionContribution:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool = True


@dataclass
class CanContribution:
    types: dict[str, CustomTypeConfig] = field(default_factory=dict)
    tx_messages: list[TxMessageContribution] = field(default_factory=list)
    rx_subscriptions: list[RxSubscriptionContribution] = field(default_factory=list)
