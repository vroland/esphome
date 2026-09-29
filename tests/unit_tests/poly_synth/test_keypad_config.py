import pytest

from esphome.components import poly_synth
import esphome.config_validation as cv


def validate_mapping(keys: str, base_note: int = 69, velocity: float = 1.0) -> dict:
    schema = cv.All(
        poly_synth.KEYPAD_MAPPING_SCHEMA, poly_synth.validate_keypad_mapping
    )
    return schema({"keys": keys, "base_note": base_note, "velocity": velocity})


def test_ordered_mapping_configuration() -> None:
    assert validate_mapping("012ABC")["base_note"] == 69
    assert validate_mapping("AB", base_note=126)["velocity"] == 1.0


@pytest.mark.parametrize(
    ("keys", "base_note", "velocity"),
    [
        ("", 69, 1.0),
        ("001", 69, 1.0),
        ("AB", 127, 1.0),
        ("A", 128, 1.0),
        ("A", -1, 1.0),
        ("A", 69, -0.1),
        ("A", 69, 1.1),
        ("é", 69, 1.0),
        ("A\x00B", 69, 1.0),
    ],
)
def test_invalid_mapping(keys: str, base_note: int, velocity: float) -> None:
    with pytest.raises(cv.Invalid):
        validate_mapping(keys, base_note, velocity)
