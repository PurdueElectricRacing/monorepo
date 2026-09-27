import json
import re
from fractions import Fraction

import pytest

from dearunits.api import DearUnits
from dearunits.config_loader import load_unit_config_bundle

from .conftest import C_TEST_DIR, REPO_ROOT, STRICT_C_FLAGS
from .test_constants import EXACT, OFFSET_UNITS, TRANSCENDENTAL

G4_TEST = REPO_ROOT / "firmware" / "source" / "g4_testing" / "dearunits_test.c"
FIRMWARE_C_FLAGS = [
    "-std=c23", "-Wall", "-Werror", "-Wshadow", "-Wdouble-promotion", "-Wsign-compare",
    "-Wformat=2", "-Werror=conversion",
]


def test_generation_is_deterministic(bundle):
    generator = DearUnits()
    first = {a.relative_path: a.content for a in generator.generate(generator.parse(bundle))}
    second = {a.relative_path: a.content for a in generator.generate(generator.parse(bundle))}
    assert first == second


def test_header_compiles_under_strict_warnings(cc, generated_dir, tmp_path):
    source = tmp_path / "include_only.c"
    source.write_text('#include "dear_units.h"\n#include "dear_units.h"\n#include "dear_units_internal.h"\nint main(void) { return 0; }\n')
    result = cc.compile(source, generated_dir, "include_only")
    assert result.ok, result.output


def test_internal_header_is_self_contained(cc, generated_dir, tmp_path):
    source = tmp_path / "internal_only.c"
    source.write_text('#include "dear_units_internal.h"\nint main(void) { return 0; }\n')
    result = cc.compile(source, generated_dir, "internal_only")
    assert result.ok, result.output


def test_behavior(cc, generated_dir):
    result = cc.compile(C_TEST_DIR / "behavior_test.c", generated_dir, "behavior_test")
    assert result.ok, result.output
    run = cc.run("behavior_test")
    assert run.returncode == 0, run.stdout + run.stderr


def conversion_source(bundle) -> str:
    owner = {}
    for quantity in [*bundle.base_quantities.values(), *bundle.derived_quantities.values()]:
        for unit in getattr(quantity, "units", []):
            owner[unit.name] = quantity.base_unit
    lines = []
    exact = {name: float(value) for name, value in EXACT.items()} | TRANSCENDENTAL
    for name, scale in exact.items():
        base = owner[name]
        lines.append(f'    check("{base}_from_{name}(1)", {base}_from_{name}(({name}_t){{ .value = 1.0f }}).value, {scale!r}f);')
        lines.append(f'    check("{base}_from_{name}(37.5)", {base}_from_{name}(({name}_t){{ .value = 37.5f }}).value, {37.5 * scale!r}f);')
        lines.append(f'    check("{name}_from_{base}(1)", {name}_from_{base}(({base}_t){{ .value = 1.0f }}).value, {1.0 / scale!r}f);')
    conversions = [
        ("celsius_from_kelvin", "kelvin", 273.15, 0.0), ("celsius_from_kelvin", "kelvin", 373.15, 100.0),
        ("celsius_from_fahrenheit", "fahrenheit", 32.0, 0.0), ("celsius_from_fahrenheit", "fahrenheit", 212.0, 100.0),
        ("celsius_from_fahrenheit", "fahrenheit", -40.0, -40.0), ("celsius_from_fahrenheit", "fahrenheit", 98.6, 37.0),
        ("kelvin_from_celsius", "celsius", 0.0, 273.15), ("kelvin_from_celsius", "celsius", 100.0, 373.15),
        ("fahrenheit_from_celsius", "celsius", 100.0, 212.0), ("fahrenheit_from_celsius", "celsius", -40.0, -40.0),
        ("fahrenheit_from_celsius", "celsius", 37.0, 98.6),
    ]
    for function, unit, value, expected in conversions:
        lines.append(f'    check("{function}({value})", {function}(({unit}_t){{ .value = {value!r}f }}).value, {expected!r}f);')
    body = "\n".join(lines)
    return f'''#include <math.h>
#include <stdio.h>

#include "dear_units.h"

static int failures;
static int checks;

static void check(const char *name, float actual, float expected) {{
    checks++;
    float tolerance = fabsf(expected) < 1e-3f ? 1e-4f : 3e-7f * fabsf(expected);
    if (fabsf(actual - expected) > tolerance) {{
        failures++;
        printf("MISMATCH %s: got %.9g expected %.9g\\n", name, (double)actual, (double)expected);
    }}
}}

int main(void) {{
{body}
    printf("%d conversion checks, %d failures\\n", checks, failures);
    return failures == 0 ? 0 : 1;
}}
'''


def test_every_conversion_matches_exact_reference(cc, generated_dir, bundle, tmp_path):
    assert set(EXACT) | set(TRANSCENDENTAL) | set(OFFSET_UNITS) >= {
        unit.name for quantity in bundle.base_quantities.values() for unit in quantity.units
    }
    source = tmp_path / "conversions.c"
    source.write_text(conversion_source(bundle))
    result = cc.compile(source, generated_dir, "conversions", flags=["-std=c23", "-Wall", "-Wextra", "-Werror"])
    assert result.ok, result.output
    run = cc.run("conversions")
    assert run.returncode == 0, run.stdout


NEGATIVE_CASES = [
    ("mixed types in a same-type op", "meter_t a = {1}; second_t s = {1}; (void)DEARUNITS_MIN(a, s);", "incompatible type"),
    ("mixed types in ADD", "meter_t a = {1}; second_t s = {1}; (void)DEARUNITS_ADD(a, s);", "incompatible type"),
    ("unsupported multiply pair", "meter_t a = {1}; second_t s = {1}; (void)DEARUNITS_MULTIPLY(a, s);", "not a function"),
    ("unsupported divide pair", "mile_t a = {1}; second_t s = {1}; (void)DEARUNITS_DIVIDE(a, s);", "not a function"),
    ("int scalar", "meter_t a = {1}; (void)DEARUNITS_MULTIPLY(2, a);", "not a function"),
    ("inverse without a matching type", "meter_t a = {1}; (void)DEARUNITS_DIVIDE(1.0f, a);", "not a function"),
    ("torque times hertz", "newton_meter_t t = {1}; hertz_t h = {1}; (void)DEARUNITS_MULTIPLY(t, h);", "not a function"),
    ("torque divided by energy", "newton_meter_t t = {1}; joule_t j = {1}; (void)DEARUNITS_DIVIDE(t, j);", "not a function"),
    ("non-base unit op", "mile_t a = {1}; (void)DEARUNITS_ABS(a);", "not compatible with any association"),
    ("trig on a non-angle", "meter_t a = {1}; (void)DEARUNITS_SIN(a);", "not compatible with any association"),
    ("square of a type with no area", "second_t s = {1}; (void)DEARUNITS_SQUARE(s);", "not compatible with any association"),
    ("unit op on a raw float", "(void)DEARUNITS_ABS(1.0f);", "not compatible with any association"),
]


@pytest.mark.parametrize("label,snippet,expected", NEGATIVE_CASES, ids=[case[0] for case in NEGATIVE_CASES])
def test_misuse_fails_to_compile(cc, generated_dir, tmp_path, label, snippet, expected):
    source = tmp_path / "misuse.c"
    source.write_text(f'#include "dear_units.h"\nvoid misuse(void) {{ {snippet} }}\n')
    result = cc.compile(source, generated_dir, "misuse", flags=["-std=c23", "-c"])
    assert not result.ok, f"{label} compiled"
    assert expected in result.output


def test_g4_hardware_test_logic_passes_on_host(cc, generated_dir, tmp_path):
    text = G4_TEST.read_text()
    start = text.index("volatile float dearunits_")
    end = text.index("int main(void)")
    body = text[start:end]
    assert "run_dearunits_test" in body
    harness = tmp_path / "g4_dearunits_host.c"
    harness.write_text(
        '#include <stdio.h>\n#include "dear_units.h"\n' + body +
        'int main(void) {\n    bool passed = run_dearunits_test();\n    printf("%s\\n", passed ? "PASS" : "FAIL");\n    return passed ? 0 : 1;\n}\n'
    )
    result = cc.compile(harness, generated_dir, "g4_dearunits_host", flags=FIRMWARE_C_FLAGS)
    assert result.ok, result.output
    run = cc.run("g4_dearunits_host")
    assert run.returncode == 0, run.stdout + run.stderr


def strip_angle(base_config: dict, compound_config: dict) -> tuple[dict, dict]:
    base = {"classes": [q for q in base_config["classes"] if q["name"] != "angle"]}
    compounds = [
        q for q in compound_config["compounds"]
        if not any(term["group_name"] == "angle" for term in q["composed_of"])
        and not any(term["group_name"] == "angular_velocity" for term in q["composed_of"])
    ]
    removed = {"angular_velocity"}
    relations = [
        r for r in compound_config["relations"]
        if not ({r["lhs"], r["rhs"], r["result"]} & removed)
    ]
    return base, {"compounds": compounds, "relations": relations}


def test_header_without_an_angle_quantity_still_compiles(cc, tmp_path, real_configs):
    base, compound = strip_angle(*real_configs)
    (tmp_path / "base.json").write_text(json.dumps(base))
    (tmp_path / "compound.json").write_text(json.dumps(compound))
    bundle = load_unit_config_bundle(tmp_path / "base.json", tmp_path / "compound.json")
    generator = DearUnits()
    output = tmp_path / "no_angle"
    output.mkdir()
    for artifact in generator.generate(generator.parse(bundle)):
        (output / str(artifact.relative_path)).write_text(artifact.content)
    header = (output / "dear_units.h").read_text()
    internal = (output / "dear_units_internal.h").read_text()
    assert "atan2" not in internal and "DEARUNITS_SIN" not in header
    source = tmp_path / "no_angle.c"
    source.write_text('#include "dear_units.h"\nint main(void) { meter_t a = {1}; meter_t b = {2}; return DEARUNITS_MAX(a, b).value > 1.0f ? 0 : 1; }\n')
    result = cc.compile(source, output, "no_angle")
    assert result.ok, result.output
    assert cc.run("no_angle").returncode == 0
