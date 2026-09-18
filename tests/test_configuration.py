from pathlib import Path
import subprocess
import sys

import pytest

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
