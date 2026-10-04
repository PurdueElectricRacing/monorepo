import copy

import pytest


def base_unit_list(base, quantity):
    return next(q for q in base["classes"] if q["name"] == quantity)["units"]


def derived(compound, name):
    return next(q for q in compound["compounds"] if q["name"] == name)


def test_real_configuration_is_accepted(load_config):
    bundle, output = load_config()
    assert bundle is not None, output


def add_base_unit(name, **fields):
    def mutate(base, compound):
        base_unit_list(base, "length").append({"name": name, "scale": 2.0, **fields})
    return mutate


def set_scale(value):
    def mutate(base, compound):
        base_unit_list(base, "length").append({"name": "extra", "scale": value})
    return mutate


def set_offset(value):
    def mutate(base, compound):
        base_unit_list(base, "length").append({"name": "extra", "scale": 2.0, "offset": value})
    return mutate


def wrong_unit_dimension(base, compound):
    derived(compound, "velocity")["units"][1]["composed_of"][1]["exponent"] = 1


def add_relation(factor_a, factor_b, result):
    def mutate(base, compound):
        compound["relations"].append({"factor_a": factor_a, "factor_b": factor_b, "result": result})
    return mutate


def add_derived(name, base_unit="extra_base", composed_of=None, units=None):
    def mutate(base, compound):
        compound["compounds"].append({
            "name": name,
            "base_unit": base_unit,
            "composed_of": composed_of or [{"group_name": "length", "unit_name": "meter", "exponent": 1}],
            "units": units or [{"name": "extra_unit", "scale": 2.0}],
        })
    return mutate


def cyclic_quantities(base, compound):
    compound["compounds"].append({
        "name": "alpha", "base_unit": "alpha_base",
        "composed_of": [{"group_name": "beta", "unit_name": "beta_base", "exponent": 1}],
        "units": [{"name": "alpha_unit", "scale": 2.0}],
    })
    compound["compounds"].append({
        "name": "beta", "base_unit": "beta_base",
        "composed_of": [{"group_name": "alpha", "unit_name": "alpha_base", "exponent": 1}],
        "units": [{"name": "beta_unit", "scale": 2.0}],
    })


def base_unit_not_coherent(base, compound):
    derived(compound, "velocity")["composed_of"][0]["unit_name"] = "kilometer"


def duplicate_relation_pair(base, compound):
    compound["relations"].append({"factor_a": "length", "factor_b": "force", "result": "torque"})
    compound["relations"].append({"factor_a": "force", "factor_b": "length", "result": "torque"})


def second_angle_quantity(base, compound):
    next(q for q in base["classes"] if q["name"] == "length")["is_angle"] = True


CASES = [
    ("wrong unit dimension", wrong_unit_dimension, "don't match velocity"),
    ("dimensionally false relation", add_relation("time", "time", "force"), "has dimensions time^2"),
    ("relation over unknown quantity", add_relation("force", "nonexistent", "torque"), "unknown base/derived quantity"),
    ("relation duplicating a pair", duplicate_relation_pair, "already defined"),
    ("space in a unit name", add_base_unit("square meter"), "should match pattern"),
    ("leading digit in a unit name", add_base_unit("9lives"), "should match pattern"),
    ("uppercase unit name", add_base_unit("Meter"), "should match pattern"),
    ("C keyword unit name", add_base_unit("int"), "reserved"),
    ("reserved unit name", add_base_unit("scalar"), "reserved"),
    ("separator fragment in a unit name", add_base_unit("a_by_b"), "separator"),
    ("_from fragment in a unit name", add_base_unit("a_from_b"), "separator"),
    ("negative scale", set_scale(-1.0), "greater than 0"),
    ("zero scale", set_scale(0), "greater than 0"),
    ("infinite scale", set_scale(float("inf")), "finite number"),
    ("NaN scale", set_scale(float("nan")), "greater than 0"),
    ("infinite offset", set_offset(float("inf")), "finite number"),
    ("unit named like a quantity", add_base_unit("velocity"), "already defined"),
    ("quantity named like a unit", add_derived("meter"), "already defined"),
    ("duplicate unit across quantities", add_base_unit("newton"), "already defined"),
    ("dependency cycle", cyclic_quantities, "could not resolve"),
    ("non-coherent compound base", base_unit_not_coherent, "must derive to scale=1.0"),
    ("second base quantity marked is_angle", second_angle_quantity, "Only one base quantity may set"),
]


@pytest.mark.parametrize("label,mutate,expected", CASES, ids=[case[0] for case in CASES])
def test_invalid_configuration_is_rejected(load_config, real_configs, label, mutate, expected):
    base, compound = copy.deepcopy(real_configs[0]), copy.deepcopy(real_configs[1])
    mutate(base, compound)
    bundle, output = load_config(base, compound)
    assert bundle is None, f"{label} was accepted"
    assert expected in output


def test_angle_is_exempt_from_relation_dimensions(load_config, real_configs):
    base, compound = copy.deepcopy(real_configs[0]), copy.deepcopy(real_configs[1])
    assert {"factor_a": "torque", "factor_b": "angular_velocity", "result": "power"} in compound["relations"]
    bundle, output = load_config(base, compound)
    assert bundle is not None, output
