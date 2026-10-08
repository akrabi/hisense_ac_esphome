"""Validate shipped examples without reading personal or example secrets."""
import base64
from pathlib import Path
import subprocess
import sys

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "doc" / "configuration" / "examples"


def use_local_component(config):
    source = "source: github://akrabi/hisense_ac_esphome"
    assert config.count(source) == 1
    return config.replace(
        source,
        f"source:\n      type: local\n      path: '{(ROOT / 'components').as_posix()}'",
    )


def validate_config(path):
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(path)],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("example", ["ac-bedroom.yaml", "ac-livingroom.yaml", "ac-office.yaml"])
def test_documented_example(tmp_path, example):
    common = use_local_component((EXAMPLES / "ac-common.yaml").read_text())
    (tmp_path / "ac-common.yaml").write_text(common)
    (tmp_path / example).write_text((EXAMPLES / example).read_text())
    # Fixed, non-secret test values, never loaded from a developer's config.
    (tmp_path / "secrets.yaml").write_text(yaml.safe_dump({
        "api_key": base64.b64encode(bytes(32)).decode(),
        "ota_pass": "test-ota-password",
        "web_server_user": "test-user",
        "web_server_pass": "test-web-password",
        "wifi_ssid": "test-network",
        "wifi_pass": "test-network-password",
        "wifi_gateway": "192.168.1.1",
        "wifi_subnet": "255.255.255.0",
        "wifi_ap_pass": "test-ap-password",
    }))
    validate_config(tmp_path / example)


def test_readme_quick_start(tmp_path):
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert readme.count("```yaml\n") == 1
    config = readme.split("```yaml\n", 1)[1].split("```", 1)[0]
    config = use_local_component(config)
    path = tmp_path / "quick-start.yaml"
    path.write_text(
        "esphome:\n  name: quick-start\n"
        "esp32:\n  board: esp32dev\n  framework:\n    type: arduino\n"
        + config
    )
    validate_config(path)
