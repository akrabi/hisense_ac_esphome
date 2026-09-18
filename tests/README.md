# Regression tests

Install `requirements-dev.txt` into a virtual environment. Both YAML fixtures are
credential-free, use the local component and ESP32 Arduino, and require no Wi-Fi.
Never substitute personal configurations or run `upload`/`run` for these tests.

```powershell
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
.venv\Scripts\python.exe -m pytest tests -q
.venv\Scripts\cmake.exe -S tests -B .build\native -DPython3_EXECUTABLE="$PWD\.venv\Scripts\python.exe"
.venv\Scripts\cmake.exe --build .build\native
.venv\Scripts\ctest.exe --test-dir .build\native --output-on-failure
.venv\Scripts\python.exe -m esphome compile tests\minimal.yaml
```

Select an installed native compiler using CMake's standard generator/toolchain
options. The firmware compile is a separate gate: Python code-generation tests
are not a firmware build.

On the development Windows machine, `.\tests\native-windows.ps1` configures the
installed standalone MSVC 19.12 Scope SDK and runs all native tests. Override
`-Sdk` for another installation of that SDK. It disables executable manifests
because this standalone SDK lacks `mt.exe`; this affects host tests only.

`fixtures/commands.json` is an independent snapshot of every `commands.cpp` array
before refactoring. Do not regenerate it from modified production code. Native
tests link the actual production arrays and compare every byte (including the
51-byte 16 C command). Native parser tests compile production `protocol.cpp`,
both with signed and unsigned plain `char`. The RX-adapter test extracts the
actual `HisenseAC::get_response()` method, replacing only its class shell,
ESPHome logging, and clock. It proves malformed/unknown frames cannot release a
pending response or overwrite the retained snapshot.

Coverage includes every capture fragmentation/truncation position, concatenated
packets, header overlap, noise, payload/checksum stuffing, high-bit data, checksum
and footer errors, every in-capacity length, all oversized length bytes, full
capacity with stuffing, timeout boundaries and clock rollover, interleaved
instances, explicit signed decoding, all mode/swing/display masks, and 100,000
deterministic noise bytes. These replace the foundation commit's known-bug
characterization tests.

GCC/Clang hosts can configure with `-DENABLE_SANITIZERS=ON` to run AddressSanitizer
and UndefinedBehaviorSanitizer; CI uses this configuration. The installed MSVC
19.12 host toolchain does not support these sanitizers, so local Windows tests
run without them. See [protocol evidence and API](../doc/protocol.md).

Generated response examples are **synthetic**, not hardware captures.
`fixtures/status_82.hex` is the single sanitized RX frame at `08:57:01.087` in
[this public log](https://github.com/fabiogermann/esphome_hisence_ac/blob/446f25e3f8d9c47cb299414d42b8b31ccd3d01ba/output.txt).
Only frame bytes are retained. The source project targets AEH-W4A1/W4B1, but the
specific indoor unit/module producing this log is not identified: this is
upstream-reported hardware traffic, not validation on our hardware. Independently
checked facts: 82 decoded bytes, length byte `0x49`, response header `01 40`,
class byte 13 `0x66`, and additive checksum `0x04c3`.

Other frame classes, status variants, unused wire flags, and acknowledgment
correlation require separate evidence. State publication and swing transition
regressions belong to the subsequent state/transport milestones; this harness
does not yet emulate ESPHome's entire climate runtime.
