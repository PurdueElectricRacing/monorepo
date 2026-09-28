import re

import pytest

HAND = {
    "length": (0, 1, 0, 0, 0, 0),
    "time": (0, 0, 1, 0, 0, 0),
    "mass": (1, 0, 0, 0, 0, 0),
    "current": (0, 0, 0, 1, 0, 0),
    "temperature": (0, 0, 0, 0, 1, 0),
    "angle": (0, 0, 0, 0, 0, 1),
    "velocity": (0, 1, -1, 0, 0, 0),
    "acceleration": (0, 1, -2, 0, 0, 0),
    "angular_velocity": (0, 0, -1, 0, 0, 1),
    "force": (1, 1, -2, 0, 0, 0),
    "torque": (1, 2, -2, 0, 0, 0),
    "pressure": (1, -1, -2, 0, 0, 0),
    "voltage": (1, 2, -3, -1, 0, 0),
    "area": (0, 2, 0, 0, 0, 0),
    "frequency": (0, 0, -1, 0, 0, 0),
    "charge": (0, 0, 1, 1, 0, 0),
    "resistance": (1, 2, -3, -2, 0, 0),
    "power": (1, 2, -3, 0, 0, 0),
    "energy": (1, 2, -2, 0, 0, 0),
}
ANGLE_AXIS = 5
DEFINITION = re.compile(r"^DEARUNITS_DEFINE_(MULTIPLY|DIVIDE)(_TO_FLOAT)?\(([^)]*)\)$", re.M)
PAIRS_NEEDING_ANGLE = {
    ("MULTIPLY", "angular_velocity", "torque", "power"),
    ("MULTIPLY", "torque", "angular_velocity", "power"),
    ("DIVIDE", "power", "angular_velocity", "torque"),
    ("DIVIDE", "power", "torque", "angular_velocity"),
}
AMBIGUOUS = {"torque", "energy"}


def add(lhs, rhs, sign=1):
    return tuple(a + sign * b for a, b in zip(lhs, rhs))


def terms_dimensions(terms):
    total = (0,) * 6
    for term in terms:
        total = add(total, tuple(term.exponent * axis for axis in HAND[term.group_name]))
    return total


def base_unit_owners(bundle):
    owners = {}
    for quantity in [*bundle.base_quantities.values(), *bundle.derived_quantities.values()]:
        owners[quantity.base_unit] = quantity.name
    return owners


def generated_pairs(header, bundle):
    owners = base_unit_owners(bundle)
    pairs = []
    for match in DEFINITION.finditer(header):
        op, to_float, arguments = match.group(1), match.group(2), [a.strip() for a in match.group(3).split(",")]
        result = None if to_float else owners[arguments[2]]
        pairs.append((op, owners[arguments[0]], owners[arguments[1]], result))
    return pairs


def test_hand_table_covers_every_quantity(bundle):
    assert set(HAND) == {*bundle.base_quantities, *bundle.derived_quantities}


def test_derived_quantities_have_their_si_dimensions(bundle):
    for quantity in bundle.derived_quantities.values():
        assert terms_dimensions(quantity.composed_of) == HAND[quantity.name], quantity.name
        for unit in quantity.units:
            if unit.composed_of is not None:
                assert terms_dimensions(unit.composed_of) == HAND[quantity.name], unit.name


def test_relations_are_dimensionally_consistent_up_to_angle(bundle):
    for relation in bundle.relations:
        product = add(HAND[relation.lhs], HAND[relation.rhs])
        expected = HAND[relation.result]
        without_angle = lambda dims: dims[:ANGLE_AXIS]
        assert without_angle(product) == without_angle(expected), relation


def test_generated_pairs_are_dimensionally_correct(header, bundle):
    needing_angle = set()
    for op, lhs, rhs, result in generated_pairs(header, bundle):
        got = add(HAND[lhs], HAND[rhs], 1 if op == "MULTIPLY" else -1)
        want = (0,) * 6 if result is None else HAND[result]
        if got == want:
            continue
        assert got[:ANGLE_AXIS] == want[:ANGLE_AXIS], (op, lhs, rhs, result)
        needing_angle.add((op, lhs, rhs, result))
    assert needing_angle == PAIRS_NEEDING_ANGLE


def test_inverse_definitions_are_dimensionally_correct(header, bundle):
    owners = base_unit_owners(bundle)
    for match in re.finditer(r"^DEARUNITS_DEFINE_INVERSE\(([^)]*)\)$", header, re.M):
        operand, result = [owners[a.strip()] for a in match.group(1).split(",")]
        assert add((0,) * 6, HAND[operand], -1) == HAND[result], (operand, result)


def relation_allowed_pairs(bundle):
    allowed = set()
    for relation in bundle.relations:
        allowed |= {
            ("MULTIPLY", relation.lhs, relation.rhs, relation.result),
            ("MULTIPLY", relation.rhs, relation.lhs, relation.result),
            ("DIVIDE", relation.result, relation.lhs, relation.rhs),
            ("DIVIDE", relation.result, relation.rhs, relation.lhs),
        }
    return allowed


def test_ambiguous_quantities_only_combine_through_relations(header, bundle):
    allowed = relation_allowed_pairs(bundle)
    for op, lhs, rhs, result in generated_pairs(header, bundle):
        if lhs not in AMBIGUOUS and rhs not in AMBIGUOUS:
            continue
        if op == "DIVIDE" and lhs == rhs and result is None:
            continue
        assert (op, lhs, rhs, result) in allowed, (op, lhs, rhs, result)


@pytest.mark.parametrize("definition", [
    "DEARUNITS_DEFINE_MULTIPLY(newton_meter, hertz, watt)",
    "DEARUNITS_DEFINE_MULTIPLY(hertz, newton_meter, watt)",
    "DEARUNITS_DEFINE_DIVIDE(newton_meter, second, watt)",
    "DEARUNITS_DEFINE_DIVIDE(newton_meter, volt, coulomb)",
    "DEARUNITS_DEFINE_DIVIDE(newton_meter, coulomb, volt)",
    "DEARUNITS_DEFINE_DIVIDE(newton_meter, watt, second)",
    "DEARUNITS_DEFINE_DIVIDE_TO_FLOAT(newton_meter, joule)",
    "DEARUNITS_DEFINE_DIVIDE_TO_FLOAT(joule, newton_meter)",
    "DEARUNITS_DEFINE_DIVIDE(joule, meter, newton)",
    "DEARUNITS_DEFINE_DIVIDE(joule, newton, meter)",
])
def test_torque_and_energy_do_not_interoperate_implicitly(header, definition):
    assert definition not in header


@pytest.mark.parametrize("definition", [
    "DEARUNITS_DEFINE_MULTIPLY(newton, meter, newton_meter)",
    "DEARUNITS_DEFINE_MULTIPLY(meter, newton, newton_meter)",
    "DEARUNITS_DEFINE_MULTIPLY(newton_meter, radians_per_second, watt)",
    "DEARUNITS_DEFINE_MULTIPLY(radians_per_second, newton_meter, watt)",
    "DEARUNITS_DEFINE_DIVIDE(watt, radians_per_second, newton_meter)",
    "DEARUNITS_DEFINE_DIVIDE(watt, newton_meter, radians_per_second)",
    "DEARUNITS_DEFINE_MULTIPLY(watt, second, joule)",
    "DEARUNITS_DEFINE_MULTIPLY(second, watt, joule)",
    "DEARUNITS_DEFINE_MULTIPLY(coulomb, volt, joule)",
    "DEARUNITS_DEFINE_MULTIPLY(volt, coulomb, joule)",
    "DEARUNITS_DEFINE_DIVIDE(watt, hertz, joule)",
    "DEARUNITS_DEFINE_DIVIDE(joule, second, watt)",
    "DEARUNITS_DEFINE_DIVIDE(joule, watt, second)",
    "DEARUNITS_DEFINE_DIVIDE_TO_FLOAT(newton_meter, newton_meter)",
    "DEARUNITS_DEFINE_DIVIDE_TO_FLOAT(joule, joule)",
    "DEARUNITS_DEFINE_MULTIPLY(volt, amp, watt)",
    "DEARUNITS_DEFINE_MULTIPLY(amp, volt, watt)",
    "DEARUNITS_DEFINE_DIVIDE(volt, amp, ohm)",
])
def test_expected_pairs_are_generated(header, definition):
    assert definition in header
