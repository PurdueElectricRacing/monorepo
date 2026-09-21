"""SuperDBC JSON v1 shape"""

from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field

Name = Annotated[str, Field(min_length=1)]
Finite = Annotated[float, Field(allow_inf_nan=False)]
RawKey = Annotated[str, Field(pattern=r"^(0|-?[1-9][0-9]*)$")]


class ExportModel(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True, frozen=True)


class LimitsExport(ExportModel):
    min: Finite
    max: Finite


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
    limits: LimitsExport | None
    unit: str
    choices: dict[RawKey, str] | None


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


class NodeExport(ExportModel):
    name: Name
    is_external: bool


class BusExport(ExportModel):
    baud_rate: Annotated[int, Field(gt=0)]
    nodes: list[NodeExport]
    messages: list[MessageExport]


class VersionsExport(ExportModel):
    schema_version: Literal[1]
    hash: Name


class SuperDbcExport(ExportModel):
    content_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    versions: VersionsExport
    buses: dict[Name, BusExport]


def superdbc_json_schema() -> dict:
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        **SuperDbcExport.model_json_schema(),
    }
