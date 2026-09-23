# Regression tests

Install `requirements-dev.txt` into a virtual environment. The firmware fixtures
are credential-free, use the local component, and require no Wi-Fi.
Never substitute personal configurations or run `upload`/`run` for these tests.

| Configuration | Coverage |
| --- | --- |
| `minimal.yaml` | ESP32 Arduino, Celsius, device-reported defaults |
| `full.yaml` | ESP32 Arduino, Fahrenheit, optimism, all sensors/diagnostics |
| `idf.yaml` | Full configuration on ESP-IDF without Arduino |
| `two_instances.yaml` | Independent UARTs/components with both unit settings |

```powershell
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
.venv\Scripts\python.exe -m pytest tests -q
.venv\Scripts\cmake.exe -S tests -B .build\native -DPython3_EXECUTABLE="$PWD\.venv\Scripts\python.exe"
.venv\Scripts\cmake.exe --build .build\native
.venv\Scripts\ctest.exe --test-dir .build\native --output-on-failure
.venv\Scripts\python.exe -m esphome compile tests\minimal.yaml
.venv\Scripts\python.exe -m esphome compile tests\full.yaml
.venv\Scripts\python.exe -m esphome compile tests\idf.yaml
.venv\Scripts\python.exe -m esphome compile tests\two_instances.yaml
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
tests link remaining production arrays and the replacement temperature encoder,
comparing every byte (including the 51-byte 16 C command). Native parser tests
compile production `protocol.cpp` with signed and unsigned plain `char`.
Integration tests compile actual `hisense_ac.cpp` and `transport.cpp` with small
ESPHome UART, clock and entity stubs. They exercise the complete component
control/loop/publication path, not a second implementation of that behavior.

Coverage includes every capture fragmentation/truncation position, concatenated
packets, header overlap, noise, payload/checksum stuffing, high-bit data, checksum
and footer errors, accepted lengths from 9–128 decoded bytes and rejection of
larger declared lengths through 264, inconsistent declared lengths, full capacity
with stuffing, timeout boundaries and clock rollover, interleaved
instances, explicit signed decoding, all mode/swing/display masks, and 100,000
deterministic noise bytes. These replace the foundation commit's known-bug
characterization tests. Supported 82-byte captures are tested at every wire
fragmentation boundary, individually and concatenated. Synthetic nonzero final
payload bytes at lengths 82 and 128 must participate in the checksum;
checksums calculated using the original off-by-one boundary are rejected.
The issue #6 capture is a negative regression: it must not publish status or
advance an operation, and the parser must recover for subsequent supported data.

Transport/component coverage includes queue acceptance and exhaustion, safe
coalescing, response timing, prerequisite failures, all 16 swing transitions,
rapid reversals, physical-remote changes, missing/stale responses, independent
instances, clock rollover, optimistic generations/expiry, Celsius/Fahrenheit
quantization and remembered setpoints. Diagnostics are exercised through the
actual component; unknown startup readings remain unpublished. Python tests
validate metadata defaults/overrides, capability restrictions, unsafe UART
configurations, visual limits, and documentation examples with dummy secrets.
Diagnostic trace tests reconstruct complete TX wire and RX decoded packets from
bounded log chunks, including stuffing, unsupported classes, short frames and
the maximum supported frame length. They verify that rejected frames do not
publish status and that request normalization, confirmation and expiry appear
in DEBUG traces without changing command bytes or state behavior. Operation
traces omit absent controls while retaining explicitly requested zero/off
values, including combined calls and display requests.

GCC/Clang hosts can configure with `-DENABLE_SANITIZERS=ON` to run AddressSanitizer
and UndefinedBehaviorSanitizer; CI uses this configuration. The installed MSVC
19.12 host toolchain does not support these sanitizers, so local Windows tests
run without them. See [protocol evidence and API](../doc/protocol.md).

Fan regressions preserve every historical mapping, recognize raw status `1` as
Auto without changing command bytes, and reject other aliases in confirmation.
They cover already-Auto requests without redundant setters, Low-to-Auto
confirmation through an explicit status poll, ignored class-`65` replies,
optimistic/reported presentation, and expiry-free completion. Temperature-memory
tests cover unknown fan/room readings and invalid mode/target retention.
Five-speed presentation groups speeds 1/2 as Low and 3/4 as Medium without
warnings. Integration tests verify that requesting Low/Medium from speed 2/4
still sends the unchanged speed-1/3 command and waits for exact feedback,
in both optimistic and device-reported modes. Other unknown codes remain unknown.
`fixtures/fan_auto_status_82.hex` is a packet-only maintainer capture dated
2026-09-23 at 14:27:38.396 (82 bytes, checksum `0706`), not synthetic data.
It reports Auto before and after the HA Auto command; the Low-to-Auto transition
in tests is synthetic. The model/module and deployed component revision were
not supplied. See [capture context](../doc/protocol.md#fan-status-and-auto-confirmation).

Generated response examples are **synthetic**, not hardware captures.
`fixtures/status_82.hex` is the single sanitized RX frame at `08:57:01.087` in
[this public log](https://github.com/fabiogermann/esphome_hisence_ac/blob/446f25e3f8d9c47cb299414d42b8b31ccd3d01ba/output.txt).
Only frame bytes are retained. The source project targets AEH-W4A1/W4B1, but the
specific indoor unit/module producing this log is not identified: this is
upstream-reported hardware traffic, not validation on our hardware. Independently
checked facts: 82 decoded bytes, length byte `0x49`, response header `01 40`,
class byte 13 `0x66`, and additive checksum `0x04c3`.

Additional packet-only public fixtures (retrieved 2026-09-18 through read-only
`gh api repos/akrabi/hisense_ac_esphome/issues/{number}`):

| Fixture | Source location | Wire / decoded bytes | Checksum |
| --- | --- | --- | --- |
| `fixtures/issue_1_status_82_escaped.hex` | [Issue #1 body](https://github.com/akrabi/hisense_ac_esphome/issues/1), first RX at `19:31:05` | 83 / 82 | `049B` |
| `fixtures/issue_6_status_160.hex` | [Issue #6 body](https://github.com/akrabi/hisense_ac_esphome/issues/6), first two consecutive RX fragments at `18:28:36` | 160 / 160 | `09BA` |

Issue #1 reports a Hisense mini apple pie unit with AEH-W4E1; issue #6 reports
an ADT-09UX4RBL8 ducted unit without identifying its original Wi-Fi module.
These are reporter-provided identifiers, not independently verified hardware.
Issue metadata `updated_at` at retrieval: #1 `2025-05-30T20:31:41Z`, #6
`2025-09-04T13:52:04Z`. Only hexadecimal packet bytes are stored, not complete
logs, configurations, personal identifiers or attachments.

The two captures have identical 16-byte header layouts except the declared
length. Explicit integrity tests independently verify length, stuffing and
`sum(decoded[2:-4])`; native tests exercise the actual production parser and
82-byte decoder. The 160-byte capture's field meanings are unverified, so it is
rejected by the component despite its valid checksum. Retaining it as a test
fixture does not enable that model or establish meanings for the extended tail.

Other frame classes, status variants, unused wire flags, and acknowledgment
correlation require separate evidence. The stubs do not emulate the physical
UART driver or Home Assistant. Firmware compilation verifies the actual ESPHome
API surface; it is not a hardware test or a measured callback-latency guarantee.

## Hardware acceptance (manual, not run by these tests)

Record the ESPHome version, AC model/module and firmware revision. Confirm normal
mode, setpoint, fan, swing and display controls; verify both device-reported and
optimistic presentation, changes made by the physical remote, rapid requests,
and AC disconnect/reconnect without replaying expired controls. Check UART-write
latency warnings and Home Assistant responsiveness during bursts and silence.
Capture packet-only evidence before adding model-specific fields or fault bits.
Do not mark an untested model supported solely because its packet parses.
