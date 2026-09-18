"""Signal metadata is validated before compilation and every output stage."""

import pytest
from pydantic import ValidationError

from core.declarations import SignalDeclaration


@pytest.mark.parametrize("field", ["scale", "offset", "min", "max"])
@pytest.mark.parametrize("value", [float("nan"), float("inf"), float("-inf")])
def test_nonfinite_signal_metadata(field, value):
    with pytest.raises(ValidationError, match="finite"):
        SignalDeclaration(signal_name="value", data_type="uint8_t", unit="V", **{field: value})


def test_signal_bounds():
    with pytest.raises(ValidationError, match="minimum must not exceed maximum"):
        SignalDeclaration(signal_name="value", data_type="uint8_t", min=2, max=1)
    for bounds in ({}, {"min": 0}, {"max": 0}, {"min": 0, "max": 0}, {"min": -1, "max": 1}):
        SignalDeclaration(signal_name="value", data_type="int8_t", **bounds)
