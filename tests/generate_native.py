"""Generate golden linkage tests and extract the actual component RX adapter."""
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
start = source.index("bool HisenseAC::get_response(")
end = source.index("// This function buffers messages", start)
method = source[start:end]
(OUT / "response_method.inc").write_text(method)
