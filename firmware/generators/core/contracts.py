"""
contracts.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass, field
from core.config_models import CustomTypeConfig


@dataclass
class SignalContribution:
    name: str
    datatype: str
    desc: str = ""
    length: int = 0
    unit: str | None = None
    choices: list[str] | None = None
    scale: float | None = None
    offset: float | None = None
    min_val: float | None = None
    max_val: float | None = None


@dataclass
class MessageContribution:
    name: str
    desc: str = ""
    priority: int = 0
    period: int = 0
    signals: list[SignalContribution] = field(default_factory=list)


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
