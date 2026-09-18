"""Validate shipped examples without reading personal or example secrets."""
import base64
from pathlib import Path
import subprocess
import sys

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "doc" / "configuration" / "examples"


@pytest.mark.parametrize("example", ["ac-bedroom.yaml", "ac-livingroom.yaml", "ac-office.yaml"])
def test_documented_example(tmp_path, example):
    common = (EXAMPLES / "ac-common.yaml").read_text()
    relative = "../../../components"
    assert (EXAMPLES / relative).resolve() == ROOT / "components"
    assert f"path: {relative}" in common
    common = common.replace(f"path: {relative}", f"path: '{(ROOT / 'components').as_posix()}'")
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
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(tmp_path / example)],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
