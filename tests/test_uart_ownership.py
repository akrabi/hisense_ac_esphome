from copy import deepcopy
from pathlib import Path
import subprocess
import sys

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("case,expected", [
    ("shared", "exclusive UART ownership"),
    ("write", "exclusive UART ownership"),
    ("debug", "debug callbacks"),
    ("flow_control", "flow control"),
    ("baud", "baud rate 9600"),
    ("parity", "parity NONE"),
    ("stop_bits", "1 stop bits"),
    ("data_bits", "8 data bits"),
])
def test_reject_unsafe_uart(case, expected):
    config = yaml.safe_load((ROOT / "tests" / "minimal.yaml").read_text())
    config["external_components"][0]["source"]["path"] = str(ROOT / "components")
    if case == "shared":
        other = deepcopy(config["climate"][0])
        other["id"] = "other_ac"
        other["name"] = "Other AC"
        config["climate"].append(other)
    elif case == "write":
        config["esphome"]["on_boot"] = [{
            "then": [{"uart.write": {"id": "ac_uart", "data": [0]}}],
        }]
    elif case == "debug":
        config["uart"]["debug"] = {"dummy_receiver": True}
    elif case == "flow_control":
        config["uart"]["flow_control_pin"] = "GPIO18"
    else:
        key, value = {
            "baud": ("baud_rate", 19200), "parity": ("parity", "EVEN"),
            "stop_bits": ("stop_bits", 2), "data_bits": ("data_bits", 7),
        }[case]
        config["uart"][key] = value
    directory = ROOT / ".build" / "schema-tests"
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"{case}.yaml"
    try:
        path.write_text(yaml.safe_dump(config))
        result = subprocess.run(
            [sys.executable, "-m", "esphome", "config", str(path)],
            cwd=ROOT, text=True, capture_output=True,
        )
        assert result.returncode != 0
        assert expected in result.stdout + result.stderr
    finally:
        path.unlink(missing_ok=True)
