"""CAN system JSON v1 contract."""

from typing import Annotated, Literal, Self

from pydantic import BaseModel, ConfigDict, Field, model_validator

Name = Annotated[str, Field(min_length=1)]
Finite = Annotated[float, Field(allow_inf_nan=False)]
RawKey = Annotated[str, Field(pattern=r"^(0|-?[1-9][0-9]*)$")]


class ExportModel(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True, frozen=True)


class SignalExport(ExportModel):
    signal_name: Name
    description: str
    data_type: Name
    raw_type: Literal["unsigned", "signed", "float32"]
    start_bit: Annotated[int, Field(ge=0, le=63)]
    bit_length: Annotated[int, Field(ge=1, le=64)]
    byte_order: Literal["little_endian", "big_endian"]
    scale: Finite
    offset: Finite
    minimum: Finite | None
    maximum: Finite | None
    unit: str
    choices: dict[RawKey, str]

    def occupied_bits(self) -> list[int]:
        bit = self.start_bit
        bits = []
        for _ in range(self.bit_length):
            bits.append(bit)
            if self.byte_order == "little_endian":
                bit += 1
            else:
                bit = bit + 15 if bit % 8 == 0 else bit - 1
        return bits

    @model_validator(mode="after")
    def validate_signal(self) -> Self:
        prefix = f"Signal '{self.signal_name}'"
        if self.minimum is not None and self.maximum is not None:
            if self.minimum > self.maximum:
                raise ValueError(f"{prefix}: minimum exceeds maximum")
        if self.raw_type == "float32":
            if self.bit_length != 32 or self.choices:
                raise ValueError(f"{prefix}: float32 requires 32 bits and no choices")
        else:
            signed = self.raw_type == "signed"
            lower = -(1 << (self.bit_length - 1)) if signed else 0
            upper = (1 << (self.bit_length - int(signed))) - 1
            for raw in self.choices:
                if not lower <= int(raw) <= upper:
                    raise ValueError(f"{prefix}: enum value {raw} does not fit signal")
        return self


class MessageExport(ExportModel):
    id: Annotated[int, Field(ge=0, le=0x1FFFFFFF)]
    is_extended_id: bool
    message_name: Name
    transmitter: Name
    receivers: list[Name]
    length_bytes: Annotated[int, Field(ge=0, le=8)]
    nominal_period_ms: Annotated[int, Field(gt=0)] | None
    priority: Annotated[int, Field(ge=0, le=5)]
    description: str
    signals: list[SignalExport]

    @model_validator(mode="after")
    def validate_message(self) -> Self:
        prefix = f"Message '{self.message_name}'"
        if not self.is_extended_id and self.id > 0x7FF:
            raise ValueError(f"{prefix}: standard ID exceeds 11 bits")
        if len(set(self.receivers)) != len(self.receivers):
            raise ValueError(f"{prefix}: duplicate receiver")
        names = set()
        occupied = set()
        for signal in self.signals:
            context = f"{prefix}, signal '{signal.signal_name}'"
            if signal.signal_name in names:
                raise ValueError(f"{context}: duplicate signal name")
            names.add(signal.signal_name)
            bits = set(signal.occupied_bits())
            if any(bit >= self.length_bytes * 8 for bit in bits):
                raise ValueError(f"{context}: signal extends beyond payload")
            if occupied & bits:
                raise ValueError(f"{context}: overlapping signal bits")
            occupied.update(bits)
        return self


class NodeExport(ExportModel):
    name: Name
    is_external: bool


class BusExport(ExportModel):
    baud_rate: Annotated[int, Field(gt=0)]
    nodes: list[NodeExport]
    messages: list[MessageExport]

    @model_validator(mode="after")
    def validate_bus(self) -> Self:
        nodes = {node.name for node in self.nodes}
        if len(nodes) != len(self.nodes):
            raise ValueError("duplicate node name")
        names = set()
        identities = set()
        for message in self.messages:
            prefix = f"Message '{message.message_name}'"
            identity = (message.is_extended_id, message.id)
            if identity in identities or message.message_name in names:
                raise ValueError(f"{prefix}: duplicate message identity or name")
            names.add(message.message_name)
            identities.add(identity)
            for node in [message.transmitter, *message.receivers]:
                if node not in nodes:
                    raise ValueError(f"{prefix}: unknown node '{node}'")
        return self


class VersionsExport(ExportModel):
    schema_version: Literal[1]
    hash: Name


class SystemExport(ExportModel):
    content_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    versions: VersionsExport
    buses: dict[Name, BusExport]


def system_json_schema() -> dict:
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        **SystemExport.model_json_schema(),
    }
