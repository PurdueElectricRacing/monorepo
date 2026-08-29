from dataclasses import dataclass, field


@dataclass
class TxMessageContribution:
    node_name: str
    bus_name: str
    message: dict


@dataclass
class RxSubscriptionContribution:
    node_name: str
    bus_name: str
    message_name: str
    callback: bool = True


@dataclass
class CanContribution:
    types: dict = field(default_factory=dict)
    tx_messages: list[TxMessageContribution] = field(default_factory=list)
    rx_subscriptions: list[RxSubscriptionContribution] = field(default_factory=list)
