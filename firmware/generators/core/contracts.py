"""
contracts.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass
from typing import Literal

from core.config_models import (
    CustomTypeDeclaration,
    MessageDeclaration,
    RxSubscriptionDeclaration,
)


@dataclass(frozen=True)
class CustomTypeContribution:
    declaration: CustomTypeDeclaration
    mode: Literal["add", "replace"] = "add"


@dataclass(frozen=True)
class TxDeclaration:
    node_name: str
    bus_name: str
    message: MessageDeclaration


@dataclass(frozen=True)
class RxDeclaration:
    node_name: str
    bus_name: str
    subscription: RxSubscriptionDeclaration


@dataclass(frozen=True)
class DeclarationContribution:
    custom_types: tuple[CustomTypeContribution, ...] = ()
    tx_messages: tuple[TxDeclaration, ...] = ()
    rx_subscriptions: tuple[RxDeclaration, ...] = ()
