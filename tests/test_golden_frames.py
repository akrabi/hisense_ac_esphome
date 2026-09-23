import json
from pathlib import Path
import re

import pytest

FIXTURES = json.loads(
    (Path(__file__).parent / "fixtures" / "commands.json").read_text()
)


def unstuff_interior(wire):
    decoded = []
    index = 2
    while index < len(wire) - 2:
        byte = wire[index]
        decoded.append(byte)
        index += 1
        if byte == 0xF4:
            assert wire[index] == 0xF4
            index += 1
    return decoded


@pytest.mark.parametrize("name,wire", FIXTURES.items())
def test_original_command_integrity(name, wire):
    assert wire[:2] == [0xF4, 0xF5]
    assert wire[-2:] == [0xF4, 0xFB]
    decoded = unstuff_interior(wire)
    assert len(decoded) + 4 == decoded[2] + 9
    assert sum(decoded[:-2]) == (decoded[-2] << 8) | decoded[-1]
    assert len(wire) == (51 if name == "temp_16_C" else 50)


def test_temperature_template_evidence():
    """Verify the common-body claim from the old snapshots, not from the encoder."""
    bodies = []
    count = 0
    for name, wire in FIXTURES.items():
        match = re.fullmatch(r"temp_(\d+)_([CF])", name)
        if match is None:
            continue
        value, unit = match.groups()
        value = int(value)
        assert 16 <= value <= 32 if unit == "C" else 61 <= value <= 90
        body = unstuff_interior(wire)[:-2]
        assert len(body) == 44 and body[:3] == [0, 0x40, 0x29]
        assert body[17] == value * 2 + 1
        body[17] = 0
        bodies.append(body)
        count += 1
    assert count == 47 and all(body == bodies[0] for body in bodies)


def test_upstream_status_capture_integrity():
    wire = bytes.fromhex(
        (Path(__file__).parent / "fixtures" / "status_82.hex").read_text()
    )
    assert len(wire) == wire[4] + 9 == 82
    assert wire[:4] == bytes([0xF4, 0xF5, 1, 0x40])
    assert wire[13] == 0x66
    assert wire[-2:] == bytes([0xF4, 0xFB])
    assert sum(wire[2:-4]) == int.from_bytes(wire[-4:-2], "big") == 0x04C3


@pytest.mark.parametrize("name,wire_size,decoded_size,checksum", [
    ("issue_1_status_82_escaped.hex", 83, 82, 0x049B),
    ("issue_6_status_160.hex", 160, 160, 0x09BA),
])
def test_public_issue_capture_integrity(name, wire_size, decoded_size, checksum):
    wire = bytes.fromhex((Path(__file__).parent / "fixtures" / name).read_text())
    assert len(wire) == wire_size
    decoded = bytearray(wire[:2])
    index = 2
    while index < len(wire) - 2:
        byte = wire[index]
        decoded.append(byte)
        index += 1
        if byte == 0xF4:
            assert wire[index] == 0xF4
            index += 1
    decoded.extend(wire[-2:])
    assert len(decoded) == decoded_size == decoded[4] + 9
    assert decoded[:4] == bytes.fromhex("f4 f5 01 40")
    assert decoded[5:16] == bytes.fromhex("01 00 fe 01 01 01 01 00 66 00 01")
    assert decoded[-2:] == bytes.fromhex("f4 fb")
    assert sum(decoded[2:-4]) == int.from_bytes(decoded[-4:-2], "big") == checksum


@pytest.mark.parametrize("mode,control_checksum,poll_checksum,differences", [
    ("cool", 0x0653, 0x0653, [13, 45]),
    ("dry", 0x05A5, 0x05A6, [13, 79]),
])
def test_control_response_capture_pairs(mode, control_checksum, poll_checksum, differences):
    packets = []
    for kind, response_class, checksum in [
        ("control", 0x65, control_checksum), ("poll", 0x66, poll_checksum),
    ]:
        packet = bytes.fromhex(
            (Path(__file__).parent / "fixtures" / f"{mode}_{kind}_{response_class:02x}.hex").read_text()
        )
        assert len(packet) == packet[4] + 9 == 82
        assert packet[:4] == bytes.fromhex("F4 F5 01 40")
        assert packet[13] == response_class
        assert packet[-2:] == bytes.fromhex("F4 FB")
        assert sum(packet[2:-4]) == int.from_bytes(packet[-4:-2], "big") == checksum
        assert packet[18] == (0x28 if mode == "cool" else 0x38)
        assert packet[19] == 27
        assert packet[26] == (0xB1 if mode == "cool" else 0x01)
        packets.append(packet)
    assert [i for i, (a, b) in enumerate(zip(*packets)) if a != b] == differences
