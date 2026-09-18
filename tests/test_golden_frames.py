import json
from pathlib import Path

import pytest

FIXTURES = json.loads(
    (Path(__file__).parent / "fixtures" / "commands.json").read_text()
)


@pytest.mark.parametrize("name,wire", FIXTURES.items())
def test_original_command_integrity(name, wire):
    assert wire[:2] == [0xF4, 0xF5]
    assert wire[-2:] == [0xF4, 0xFB]
    decoded = []
    index = 2
    while index < len(wire) - 2:
        byte = wire[index]
        decoded.append(byte)
        index += 1
        if byte == 0xF4:
            assert wire[index] == 0xF4, name
            index += 1
    assert len(decoded) + 4 == decoded[2] + 9
    assert sum(decoded[:-2]) == (decoded[-2] << 8) | decoded[-1]
    assert len(wire) == (51 if name == "temp_16_C" else 50)


def test_upstream_status_capture_integrity():
    wire = bytes.fromhex(
        (Path(__file__).parent / "fixtures" / "status_82.hex").read_text()
    )
    assert len(wire) == wire[4] + 9 == 82
    assert wire[:4] == bytes([0xF4, 0xF5, 1, 0x40])
    assert wire[13] == 0x66
    assert wire[-2:] == bytes([0xF4, 0xFB])
    assert sum(wire[2:-4]) == int.from_bytes(wire[-4:-2], "big") == 0x04C3
