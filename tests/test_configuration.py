from pathlib import Path
import subprocess
import sys
import re

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("fixture", ["minimal", "full"])
def test_configuration_codegen(fixture):
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "compile",
         str(ROOT / "tests" / f"{fixture}.yaml"), "--only-generate"],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    generated = (
        ROOT / "tests" / ".esphome" / "build" / f"hisense-test-{fixture}"
        / "src" / "main.cpp"
    ).read_text()
    assert "hisense_ac::HisenseAC(ac_uart)" in generated
    assert "set_uart_parent(" in generated
    assert f"set_optimistic({'true' if fixture == 'full' else 'false'})" in generated
    if fixture == "full":
        for sensor in (
            "compressor_frequency", "compressor_frequency_setting",
            "compressor_frequency_send", "outdoor_temperature",
            "outdoor_condenser_temperature", "compressor_exhaust_temperature",
            "target_exhaust_temperature", "indoor_pipe_temperature",
            "indoor_humidity_setting", "indoor_humidity_status",
        ):
            assert f"set_{sensor}(" in generated
        assert "set_display_switch(" in generated
        assert "hisense_ac::FAHRENHEIT" in generated
        code = "\n".join(line.split("//", 1)[0] for line in generated.splitlines())
        assert code.count("->set_state_class(sensor::STATE_CLASS_MEASUREMENT);") == 10
        assert code.count("->set_accuracy_decimals(0);") == 10
        for device_class in ("frequency", "temperature", "humidity"):
            assert f'PROGMEM = "{device_class}";' in code
        assert '"Hz"' in code and '"%"' in code and r'"\302\260C"' in code


@pytest.mark.parametrize("unit,visual,valid", [
    ("CELSIUS", {"min_temperature": 16, "max_temperature": 32, "temperature_step": 1}, True),
    ("FAHRENHEIT", {"min_temperature": 20, "max_temperature": 30, "temperature_step": 5 / 9}, True),
    ("CELSIUS", {"min_temperature": 15}, False),
    ("CELSIUS", {"max_temperature": 33}, False),
    ("CELSIUS", {"temperature_step": 0.5}, False),
    ("CELSIUS", {"min_temperature": 20.5}, False),
    ("FAHRENHEIT", {"min_temperature": 16}, False),
    ("FAHRENHEIT", {"temperature_step": 1}, False),
    ("FAHRENHEIT", {"max_temperature": 35}, False),
    ("CELSIUS", {"min_temperature": 31, "max_temperature": 30}, False),
])
def test_visual_bounds(tmp_path, unit, visual, valid):
    config = yaml.safe_load((ROOT / "tests" / "minimal.yaml").read_text())
    config["external_components"][0]["source"]["path"] = str(ROOT / "components")
    config["climate"][0].update(temperature_unit=unit, visual=visual)
    path = tmp_path / "visual.yaml"
    path.write_text(yaml.safe_dump(config))
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(path)],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert (result.returncode == 0) == valid, result.stdout + result.stderr


def test_metadata_overrides(tmp_path):
    config = yaml.safe_load((ROOT / "tests" / "minimal.yaml").read_text())
    config["esphome"]["name"] = "hisense-metadata-test"
    config["esphome"]["build_path"] = str(tmp_path / "build")
    config["external_components"][0]["source"]["path"] = str(ROOT / "components")
    config["climate"][0]["compressor_frequency"] = {
        "id": "custom_sensor", "name": "Custom frequency",
        "unit_of_measurement": "kHz", "device_class": "",
        "state_class": "total", "accuracy_decimals": 2,
    }
    path = tmp_path / "metadata.yaml"
    path.write_text(yaml.safe_dump(config))
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "compile", str(path), "--only-generate"],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    generated = (tmp_path / "build" / "src" / "main.cpp").read_text()
    code = "\n".join(line.split("//", 1)[0] for line in generated.splitlines())
    assert "custom_sensor->set_state_class(sensor::STATE_CLASS_TOTAL);" in code
    assert "custom_sensor->set_accuracy_decimals(2);" in code
    assert '"kHz"' in code and 'PROGMEM = "frequency";' not in code
    # Only a unit index is set; the low-byte device-class index is zero.
    registration = re.search(r'App\.register_sensor\(custom_sensor,.*?,\s*(\d+)\);', code)
    assert registration and int(registration.group(1)) & 0xFF == 0
