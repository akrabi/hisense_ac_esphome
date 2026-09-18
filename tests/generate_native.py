"""Generate native expectations only from the independent, unchanged goldens."""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1])
fixtures = json.loads((ROOT / "tests" / "fixtures" / "commands.json").read_text())
header = ["#pragma once", "#include <cstdint>", "namespace golden {"]
for name, data in fixtures.items():
    header.append(f"constexpr uint8_t {name}[] = {{" + ",".join(map(str, data)) + "};")
header.append("}")
(OUT / "command_goldens.h").write_text("\n".join(header) + "\n")

lines = [
    '#include "commands.h"',
    '#include "command_goldens.h"',
    "#include <climits>",
    "#include <cstring>",
    "#include <cstdio>",
    "#include <cstdlib>",
    "#include <initializer_list>",
    "#include <type_traits>",
    "using namespace esphome::hisense_ac;",
    r'#define REQUIRE(c) do { if (!(c)) { std::fprintf(stderr, "Encoder failure at line %d: %s\n", __LINE__, #c); std::exit(1); } } while (false)',
    r"""
void encoder_boundaries() {
    // Synthetic byte-level expectations, independently calculated sums.
    const uint8_t payload[] = {0, 0x40, 1, 0xF4};
    const uint8_t payload_wire[] = {0xF4,0xF5,0,0x40,1,0xF4,0xF4,1,0x35,0xF4,0xFB};
    const uint8_t checksum[] = {0, 0x40, 1, 0xB3};
    const uint8_t checksum_wire[] = {0xF4,0xF5,0,0x40,1,0xB3,0,0xF4,0xF4,0xF4,0xFB};
    const uint8_t both[] = {0, 0x40, 2, 0xF4, 0xBE};
    const uint8_t both_wire[] = {0xF4,0xF5,0,0x40,2,0xF4,0xF4,0xBE,1,0xF4,0xF4,0xF4,0xFB};
    struct Case { const uint8_t *body; size_t body_size; const uint8_t *wire; size_t wire_size; };
    const Case cases[] = {
        {payload, sizeof(payload), payload_wire, sizeof(payload_wire)},
        {checksum, sizeof(checksum), checksum_wire, sizeof(checksum_wire)},
        {both, sizeof(both), both_wire, sizeof(both_wire)},
    };
    uint8_t output[MAX_COMMAND_WIRE_SIZE + 2];
    size_t size = 99;
    for (const auto &test : cases) {
        for (size_t capacity = 0; capacity < test.wire_size; ++capacity) {
            std::memset(output, 0xA5, sizeof(output));
            REQUIRE(!encode_command(test.body, test.body_size, output, capacity, size));
            REQUIRE(size == 0);
            for (uint8_t byte : output) REQUIRE(byte == 0xA5);
        }
        REQUIRE(encode_command(test.body, test.body_size, output, test.wire_size, size));
        REQUIRE(size == test.wire_size && std::memcmp(output, test.wire, size) == 0);
        REQUIRE(output[size] == 0xA5);
    }
    REQUIRE(!encode_command(nullptr, 4, output, sizeof(output), size) && size == 0);
    REQUIRE(!encode_command(payload, sizeof(payload), nullptr, sizeof(output), size) && size == 0);
    for (size_t short_size = 0; short_size < 3; ++short_size)
        REQUIRE(!encode_command(payload, short_size, output, sizeof(output), size) && size == 0);
    uint8_t invalid[] = {0, 0x40, 2, 1};
    REQUIRE(!encode_command(invalid, sizeof(invalid), output, sizeof(output), size));
    invalid[2] = 1; invalid[0] = 1;
    REQUIRE(!encode_command(invalid, sizeof(invalid), output, sizeof(output), size));
    invalid[0] = 0; invalid[1] = 0;
    REQUIRE(!encode_command(invalid, sizeof(invalid), output, sizeof(output), size));

    uint8_t maximum[58] = {};
    maximum[1] = 0x40; maximum[2] = 55;
    REQUIRE(encode_command(maximum, sizeof(maximum), output, 64, size) && size == 64);
    REQUIRE(output[60] == 0 && output[61] == 0x77);
    maximum[57] = 0xF4; // Decoded frame fits, escaped wire frame does not.
    std::memset(output, 0xA5, sizeof(output));
    REQUIRE(!encode_command(maximum, sizeof(maximum), output, sizeof(output), size) && size == 0);
    for (uint8_t byte : output) REQUIRE(byte == 0xA5);
    REQUIRE(!encode_command(maximum, 59, output, sizeof(output), size) && size == 0);
    REQUIRE(!encode_command(maximum, SIZE_MAX, output, sizeof(output), size) && size == 0);
    std::memcpy(output, both, sizeof(both));
    REQUIRE(encode_command(output, sizeof(both), output, sizeof(output), size));
    REQUIRE(size == sizeof(both_wire) && std::memcmp(output, both_wire, size) == 0);

    CommandPacket packet;
    for (int value : {INT_MIN, -1, 0, 15, 33, 60, 91, 256, INT_MAX}) {
        for (bool fahrenheit : {false, true}) {
            std::memset(packet.data, 0xA5, sizeof(packet.data)); packet.size = 17;
            REQUIRE(!encode_temperature(value, fahrenheit, packet) && packet.size == 0);
            for (uint8_t byte : packet.data) REQUIRE(byte == 0xA5);
        }
    }
    REQUIRE(!encode_temperature(16, true, packet));
    REQUIRE(!encode_temperature(61, false, packet));
    REQUIRE(encode_temperature(16, false, packet) && packet.size == 51);
}
""",
    "int main() {",
    "  encoder_boundaries();",
]
for name, data in fixtures.items():
    temperature = re.fullmatch(r"temp_(\d+)_([CF])", name)
    lines += ["  {", f"    const auto &expected = golden::{name};"]
    if temperature:
        value, unit = temperature.groups()
        lines += [
            "    CommandPacket packet;",
            f"    REQUIRE(encode_temperature({value}, {'true' if unit == 'F' else 'false'}, packet));",
            "    REQUIRE(packet.size == sizeof(expected));",
            "    REQUIRE(std::memcmp(packet.data, expected, sizeof(expected)) == 0);",
        ]
    else:
        lines += [
            f"    static_assert(std::is_const<std::remove_extent<decltype({name})>::type>::value, \"immutable command\");",
            f"    REQUIRE(sizeof({name}) == sizeof(expected));",
            f"    REQUIRE(std::memcmp({name}, expected, sizeof(expected)) == 0);",
        ]
    interior = []
    index = 2
    while index < len(data) - 2:
        byte = data[index]
        interior.append(byte)
        index += 1
        if byte == 0xF4:
            assert data[index] == 0xF4
            index += 1
    lines += [
        "    const uint8_t body[] = {" + ",".join(map(str, interior[:-2])) + "};",
        "    uint8_t encoded[MAX_COMMAND_WIRE_SIZE];",
        "    size_t size = 0;",
        "    std::memset(encoded, 0xA5, sizeof(encoded));",
        "    REQUIRE(!encode_command(body, sizeof(body), encoded, sizeof(expected) - 1, size));",
        "    REQUIRE(size == 0);",
        "    for (uint8_t byte : encoded) REQUIRE(byte == 0xA5);",
        "    REQUIRE(encode_command(body, sizeof(body), encoded, sizeof(encoded), size));",
        "    REQUIRE(size == sizeof(expected) && std::memcmp(encoded, expected, size) == 0);",
        "  }",
    ]
lines += ["  return 0;", "}"]
(OUT / "command_tests.cpp").write_text("\n".join(lines))
