import math
from fractions import Fraction

INCH = Fraction(254, 10000)
FOOT = Fraction(3048, 10000)
POUND = Fraction(45359237, 100000000)
GRAVITY = Fraction(980665, 100000)
MILE = 5280 * FOOT
POUND_FORCE = POUND * GRAVITY

EXACT = {
    "millimeter": Fraction(1, 1000),
    "centimeter": Fraction(1, 100),
    "inch": INCH,
    "foot": FOOT,
    "mile": MILE,
    "kilometer": Fraction(1000),
    "millisecond": Fraction(1, 1000),
    "minute": Fraction(60),
    "hour": Fraction(3600),
    "day": Fraction(86400),
    "gram": Fraction(1, 1000),
    "pound": POUND,
    "milliamp": Fraction(1, 1000),
    "kilometers_per_hour": Fraction(1000, 3600),
    "miles_per_hour": MILE / 3600,
    "standard_gravity": GRAVITY,
    "pound_force": POUND_FORCE,
    "pound_foot": POUND_FORCE * FOOT,
    "kilopascal": Fraction(1000),
    "pound_per_square_inch": POUND_FORCE / (INCH * INCH),
    "bar": Fraction(100000),
    "millivolt": Fraction(1, 1000),
    "square_millimeter": Fraction(1, 10**6),
    "square_centimeter": Fraction(1, 10**4),
    "square_inch": INCH * INCH,
    "kilohertz": Fraction(1000),
    "amp_hour": Fraction(3600),
    "milliamp_hour": Fraction(36, 10),
    "milliohm": Fraction(1, 1000),
    "kiloohm": Fraction(1000),
    "kilowatt": Fraction(1000),
    "horsepower": 550 * FOOT * POUND_FORCE,
    "kilojoule": Fraction(1000),
    "watt_hour": Fraction(3600),
    "kilowatt_hour": Fraction(3600000),
}
TRANSCENDENTAL = {
    "degree": math.pi / 180,
    "revolution": 2 * math.pi,
    "degrees_per_second": math.pi / 180,
    "revolutions_per_minute": 2 * math.pi / 60,
}
OFFSET_UNITS = {
    "kelvin": (1.0, -273.15),
    "fahrenheit": (5 / 9, -160 / 9),
}


def resolved_units(bundle) -> dict[str, tuple[float, float]]:
    units = {}
    for quantity in bundle.base_quantities.values():
        for unit in quantity.units:
            units[unit.name] = (float(unit.scale), float(unit.offset))
    for name, scales in bundle.derived_scales.items():
        base = bundle.derived_quantities[name].base_unit
        for unit_name, scale in scales.items():
            if unit_name != base:
                units[unit_name] = (scale, 0.0)
    return units


def test_every_unit_has_a_reference(bundle):
    assert set(resolved_units(bundle)) == set(EXACT) | set(TRANSCENDENTAL) | set(OFFSET_UNITS)


def test_scales_match_exact_definitions(bundle):
    units = resolved_units(bundle)
    for name, exact in EXACT.items():
        scale, offset = units[name]
        assert math.isclose(scale, float(exact), rel_tol=1e-12), name
        assert offset == 0.0, name


def test_angle_scales_match_pi(bundle):
    units = resolved_units(bundle)
    for name, expected in TRANSCENDENTAL.items():
        assert math.isclose(units[name][0], expected, rel_tol=1e-14), name


def test_temperature_scales_and_offsets(bundle):
    units = resolved_units(bundle)
    for name, (scale, offset) in OFFSET_UNITS.items():
        assert math.isclose(units[name][0], scale, rel_tol=1e-14), name
        assert math.isclose(units[name][1], offset, rel_tol=1e-14), name


def test_international_mile_is_exact(bundle):
    assert resolved_units(bundle)["mile"][0] == 1609.344


def test_compound_base_units_are_coherent(bundle):
    for name, scales in bundle.derived_scales.items():
        assert scales[bundle.derived_quantities[name].base_unit] == 1.0
