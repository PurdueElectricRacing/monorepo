"""
config_models.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
from typing import Annotated, Literal, Self

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator


ByteOrder = Literal["little_endian", "big_endian"]
FaultPriority = Literal["warning", "error", "fatal"]
Peripheral = Literal["CAN1", "CAN2", "FDCAN1", "FDCAN2", "FDCAN3"]
Number = int | float
BaseType = Literal[
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "float", "bool",
]

BitLength = Annotated[int, Field(ge=1, le=64)]
MessagePriority = Annotated[int, Field(ge=0, le=5)]
NonNegativeInt = Annotated[int, Field(ge=0)]


def _duplicate_value(values: list[object]) -> object | None:
    """Return the first duplicate while preserving list semantics and order."""
    for index, value in enumerate(values):
        if value in values[:index]:
            return value
    return None


class ConfigModel(BaseModel):
    """Strict base for JSON-backed generator configuration."""

    model_config = ConfigDict(
        extra="forbid",
        strict=True,
        validate_by_alias=True,
        validate_by_name=False,
        loc_by_alias=True,
    )

    @model_validator(mode="before")
    @classmethod
    def reject_explicit_null(cls, data: object) -> object:
        # The replaced schemas allowed fields to be absent, but none allowed JSON null.
        if isinstance(data, Mapping):
            null_fields = [str(name) for name, value in data.items() if value is None]
            if null_fields:
                raise ValueError(
                    f"null is not allowed for field(s): {', '.join(null_fields)}"
                )
        return data


class SignalConfig(ConfigModel):
    name: str = Field(validation_alias="sig_name")
    datatype: str = Field(validation_alias="type")
    desc: str = Field(default="", validation_alias="sig_desc")
    length: BitLength | None = None
    unit: str | None = None
    choices: list[str] | None = None
    scale: Number | None = None
    offset: Number | None = None
    min_val: Number | None = Field(default=None, validation_alias="min")
    max_val: Number | None = Field(default=None, validation_alias="max")

    @field_validator("choices")
    @classmethod
    def choices_are_unique(cls, choices: list[str] | None) -> list[str] | None:
        if choices is not None and _duplicate_value(choices) is not None:
            raise ValueError("choices must contain unique values")
        return choices

    @model_validator(mode="after")
    def validate_signal_rules(self) -> Self:
        if self.datatype == "float" and self.length not in (None, 32):
            raise ValueError("float signals must have length 32")
        if self.scale is not None and self.unit is None:
            raise ValueError("unit is required when scale is specified")
        return self


class TxMessageConfig(ConfigModel):
    name: str = Field(validation_alias="msg_name")
    desc: str = Field(validation_alias="msg_desc")
    signals: list[SignalConfig]
    priority: MessagePriority = Field(validation_alias="msg_priority")
    period: NonNegativeInt = Field(default=0, validation_alias="msg_period")
    id_override: str | None = Field(
        default=None,
        validation_alias="msg_id_override",
        pattern=r"^0x([0-1]?[0-9a-fA-F]{1,7}|[0-9a-fA-F]{1,7})$",
    )
    byte_order: ByteOrder = "little_endian"

    @model_validator(mode="after")
    def signal_names_are_unique(self) -> Self:
        names = [signal.name for signal in self.signals]
        duplicate = _duplicate_value(names)
        if duplicate is not None:
            raise ValueError(f"duplicate signal name '{duplicate}'")
        return self


class RxMessageConfig(ConfigModel):
    name: str = Field(validation_alias="msg_name")
    callback: bool = False


class BusAttachmentConfig(ConfigModel):
    peripheral: Peripheral
    tx: list[TxMessageConfig] = Field(default_factory=list)
    rx: list[RxMessageConfig] = Field(default_factory=list)
    accept_all_messages: bool = False

    @model_validator(mode="after")
    def rx_names_are_unique(self) -> Self:
        names = [message.name for message in self.rx]
        duplicate = _duplicate_value(names)
        if duplicate is not None:
            raise ValueError(f"duplicate RX message '{duplicate}'")
        return self


class FaultConfig(ConfigModel):
    name: str = Field(validation_alias="fault_name")
    max_val: Number = Field(validation_alias="max")
    min_val: Number = Field(validation_alias="min")
    priority: FaultPriority
    time_to_latch: NonNegativeInt
    time_to_unlatch: NonNegativeInt
    lcd_message: str = Field(max_length=100)

    @model_validator(mode="after")
    def limits_are_ordered(self) -> Self:
        if self.min_val >= self.max_val:
            raise ValueError("fault minimum must be less than maximum")
        return self


class InternalNodeConfig(ConfigModel):
    node_name: str
    busses: Annotated[dict[str, BusAttachmentConfig], Field(min_length=1)]
    fault_library_enabled: bool
    generate_fault_messages: bool = False
    faults: list[FaultConfig] = Field(default_factory=list)

    @model_validator(mode="after")
    def validate_fault_configuration(self) -> Self:
        if "faults" in self.model_fields_set and not self.fault_library_enabled:
            raise ValueError(
                "fault_library_enabled must be true when faults are configured"
            )
        if _duplicate_value(self.faults) is not None:
            raise ValueError("faults must contain unique values")
        return self


class ExternalNodeConfig(ConfigModel):
    node_name: str
    bus_name: str
    tx: list[TxMessageConfig] = Field(default_factory=list)
    rx: list[RxMessageConfig] = Field(default_factory=list)

    @model_validator(mode="after")
    def rx_names_are_unique(self) -> Self:
        names = [message.name for message in self.rx]
        duplicate = _duplicate_value(names)
        if duplicate is not None:
            raise ValueError(f"duplicate RX message '{duplicate}'")
        return self


class BusConfig(ConfigModel):
    name: str
    baud_rate: Literal[250000, 500000, 1000000]
    is_extended_id: bool
    is_flexible_data_rate: bool
    host_fault_library: bool

    @property
    def baud_label(self) -> str:
        return {
            250000: "FDCAN_BAUD_250K",
            500000: "FDCAN_BAUD_500K",
            1000000: "FDCAN_BAUD_1M",
        }[self.baud_rate]


class BusRegistryConfig(ConfigModel):
    busses: list[BusConfig]

    @model_validator(mode="after")
    def bus_names_are_unique(self) -> Self:
        names = [bus.name for bus in self.busses]
        duplicate = _duplicate_value(names)
        if duplicate is not None:
            raise ValueError(f"duplicate bus name '{duplicate}'")
        return self


class CustomTypeConfig(ConfigModel):
    name: str = Field(pattern=r"^[a-zA-Z_][a-zA-Z0-9_]*_t$")
    base_type: BaseType
    choices: Annotated[list[str], Field(min_length=1)] | None = None

    @field_validator("choices")
    @classmethod
    def choices_are_unique(cls, choices: list[str] | None) -> list[str] | None:
        if choices is not None and _duplicate_value(choices) is not None:
            raise ValueError("choices must contain unique values")
        return choices


class TypeRegistryConfig(ConfigModel):
    types: list[CustomTypeConfig]

    @model_validator(mode="after")
    def type_names_are_unique(self) -> Self:
        names = [custom_type.name for custom_type in self.types]
        duplicate = _duplicate_value(names)
        if duplicate is not None:
            raise ValueError(f"duplicate custom type name '{duplicate}'")
        return self


@dataclass(frozen=True)
class ConfigBundle:
    buses: dict[str, BusConfig]
    custom_types: dict[str, CustomTypeConfig]
    internal_nodes: list[InternalNodeConfig]
    external_nodes: list[ExternalNodeConfig]
