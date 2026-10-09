"""Validation primitives shared by plot and signal specifications."""

from typing import Annotated

from pydantic import BaseModel, ConfigDict, Field, PositiveFloat, StringConstraints


NonEmptyString = Annotated[str, StringConstraints(strip_whitespace=True, min_length=1)]
PositiveFiniteFloat = Annotated[float, Field(gt=0, allow_inf_nan=False)]
FiniteFloat = Annotated[float, Field(allow_inf_nan=False)]


class StrictConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")


class PlotConfig(StrictConfig):
    title: NonEmptyString
    filename: NonEmptyString
    width: PositiveFloat
    height: PositiveFloat
    time_column: NonEmptyString
