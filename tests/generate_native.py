"""Generate linkage tests from independent goldens; extract, never copy, the legacy parser."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1])
fixtures = json.loads((ROOT / "tests" / "fixtures" / "commands.json").read_text())
lines = [
    '#include "commands.h"',
    "#include <cstring>",
    "#include <cstdio>",
    "using namespace esphome::hisense_ac;",
    "int main() {",
]
for name, data in fixtures.items():
    lines += [
        "  {",
        "    const unsigned char expected[] = {" + ",".join(map(str, data)) + "};",
        f'    if (sizeof({name}) != sizeof(expected) || std::memcmp({name}, expected, sizeof(expected))) {{',
        f'      std::fprintf(stderr, "Golden mismatch: {name}\\n"); return 1;',
        "    }",
        "  }",
    ]
lines += ["  return 0;", "}"]
(OUT / "command_tests.cpp").write_text("\n".join(lines))

source = (ROOT / "components" / "hisense_ac" / "hisense_ac.cpp").read_text()
start = source.index("int HisenseAC::get_response(")
end = source.index("// This function buffers messages", start)
method = source[start:end]
scaffold = r"""
#include <cstdint>
#include <cstring>
#include <cstdio>
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
class HisenseAC {
public:
    static const int UART_BUF_SIZE = 128;
    bool wait_for_rx = true;
    int get_response(uint8_t input, uint8_t *out);
};
"""
tests = r"""
int main() {
    HisenseAC ac;
    uint8_t out[128] = {};
    // Characterize the bug: noise incorrectly releases the pending response.
    ac.get_response(0, out);
    if (ac.wait_for_rx) return 1;
    // Synthetic framing-only response, not a hardware status capture.
    const uint8_t frame[] = {0xf4,0xf5,1,0x40,0,0,0x41,0xf4,0xfb};
    int size = 0;
    for (uint8_t b : frame) size = ac.get_response(b, out);
    if (size != sizeof(frame) || std::memcmp(out, frame, sizeof(frame))) return 2;
    // Characterize shared state: two instances can finish each other's frame.
    HisenseAC other;
    ac.get_response(frame[0], out);
    for (unsigned i = 1; i < sizeof(frame); ++i) size = other.get_response(frame[i], out);
    if (size != sizeof(frame)) return 3;
    return 0;
}
"""
(OUT / "legacy_parser_tests.cpp").write_text(scaffold + method + tests)
