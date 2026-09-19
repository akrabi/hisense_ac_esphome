# UART receive protocol and evidence

## Supported envelope

On this experimental branch, the receive parser supports decoded frames of
9–160 bytes, covering the existing 82-byte layout and the issue #6 capture.
Larger packets are rejected:

| Decoded offset | Meaning |
| --- | --- |
| 0–1 | Header `F4 F5` |
| 2–3 | Supported response format `01 40` |
| 4 | Payload length; total decoded length is this value + 9 |
| 5 through size − 5 | Payload |
| size − 4, size − 3 | Unsigned additive checksum, big-endian |
| size − 2, size − 1 | Footer `F4 FB` |

The checksum sums decoded offsets 2 through size − 5 inclusive. Interior `F4`
bytes, including checksum bytes, must be doubled on the wire; each pair contributes
one decoded byte and is summed only once. Header/footer markers are not stuffed.
Wire size may be larger because of stuffing. The length field can theoretically
represent up to 264 decoded bytes (`255 + 9`), but that does not establish another
supported status format or require allocating its maximum size.

Each component has its own fixed-capacity parser and decoded snapshot. Partial
frames expire after **100 ms between consumed bytes**, using rollover-safe
elapsed time. This is a conservative software recovery threshold, not measured
AC timing. UART buffering means consumption times are not wire-arrival times.
Bad checksums, malformed escapes/lengths/footers and timeouts never acknowledge
a command. Resynchronization recognizes `F4 F5`, including overlapping headers
and a new header beginning where an invalid footer was expected.

## Supported status layout

Checksum-valid **82-byte** responses with **offset 13 = `0x66`** are decoded
as status. This branch also experimentally decodes **160-byte** responses using
the same field offsets through byte 47. Both length and class are required.
Other lengths within the parser's bound remain framing-only. The additional
layout is an assumption for device testing, not established compatibility.
This is based on:

- The historical component's wire struct: 16 header bytes, 56 status bytes,
  six extra bytes and four checksum/footer bytes (82 decoded bytes, regardless
  of compiler-added alignment padding).
- The independently checked raw public capture described in
  [tests/README.md](../tests/README.md): header `F4 F5 01 40 49`, class `66`,
  total 82 bytes and checksum `04 C3`. Source revision and timestamp are pinned.
  It is not a local hardware capture; its exact indoor-unit model is unknown.
- [Issue #1](https://github.com/akrabi/hisense_ac_esphome/issues/1), first RX at
  `19:31:05`: 83 wire bytes decode to 82 bytes because payload byte 50 (`F4`) is
  doubled. Declared length `49`, checksum `04 9B`.

The 160-byte ADT-09UX4RBL8 capture in
[issue #6](https://github.com/akrabi/hisense_ac_esphome/issues/6) has valid framing
and checksum (`09 BA`), but its status-field meanings and control compatibility
are unverified. Matching header bytes do not prove a shared payload layout.
This branch intentionally tests the shared-prefix hypothesis by publishing its
decoded values. The main improvements branch instead rejects this packet.
See [ADT-09UX4RBL8 testing](adt-09ux4rbl8-testing.md) before using it.

This is **not a claim of model-specific control,
capability, sensor meaning or sentinel handling**. No tail fields are inferred
from expanded fork structs; offsets after the consumed prefix remain opaque
but are included in checksum verification through the final payload byte.
No zoffypal v3 or other model-specific protocol is enabled.

Do not infer response semantics from inconsistent upstream prose or fixture
descriptions. Short responses and other classes/lengths are deliberately not
decoded or treated as command success. No short acknowledgment format or unique
transaction correlation has been verified. A recognized status may update the
reported state; it does not by itself prove execution of the last command.
The transaction engine below checks requested fields after an explicit poll.

The decoder preserves only mappings used by the old component:

| Offset | Existing field / conversion |
| --- | --- |
| 16 | Fan code |
| 18 | Run bits `(byte & 0x0c) >> 2`, mode `byte >> 4` |
| 19, 20, 21 | Setpoint, room temperature, pipe temperature; unsigned |
| 22, 23 | Existing humidity fields; explicit signed 8-bit conversion |
| 35 | Horizontal swing `0x40`, vertical swing `0x80` |
| 37 | Display backlight `0x80` |
| 41, 42, 43 | Existing compressor frequency / setting / sent fields |
| 44–47 | Outdoor, condenser, exhaust, target exhaust; signed conversion |

These are **compatibility mappings**, not newly verified measurements.
In particular, humidity meanings and compressor-field ordering are not proven
across models. Display feedback through `back_led` (byte 37, mask `0x80`) is
verified on ACOND ASTI-09UW4RVEDC00 / AEH-W4B1 and independently confirmed by the
maintainer on another device. The latter confirmation did not identify a
model/module, so it does not add a named compatibility entry.

Unused fields from the removed packed wire struct remain **unverified**, not
new capabilities: sleep, direction, somatosensory compensation, Fahrenheit and
temperature-compensation flags, timers, wind door/drying, dual frequency,
efficient/low-power/heat/nature, smoke/voice/mute/smart-eye/cleaning/swap/dew,
electrical/wind/filter/other LED flags, EEPROM/self-test/time-lapse, all fault
and communication flags, electrical voltage/current fields, expansion threshold,
outdoor-machine/four-way flags and reserved/extra bytes. Do not expose or decode
them based solely on their historical names.

## Complete status field reference

This reference preserves the information from the packed `Device_Status` in
commit `e65774a` (before its replacement with a decoded snapshot). It is not a
second decoder, a specification supplied by Hisense, or a claim that every named
field works on every unit. `DeviceStatus` deliberately contains only consumed
values; the parser still validates the entire frame, including unused bytes.

Offsets below are **zero-based decoded frame offsets**, after removing byte
stuffing, not positions in the escaped UART stream. They reconstruct the
original ESP32 layout with bitfields allocated from the least-significant bit
upward. C++ bitfield ordering is implementation-dependent; this table must not be
used to justify casting a byte buffer to a struct. In particular, the original
byte 40 declaration used only seven bits; its eighth bit was unnamed padding.

Usage and verification are independent:

- **Used**: decoded into `DeviceStatus` and consumed by the component.
- **Unused**: documented but not decoded into `DeviceStatus`; not obsolete.
- **Reserved**: reserved or unnamed bits/bytes without an assigned interpretation.
- **Framing**: handled by frame validation rather than copied to `DeviceStatus`.

Verification describes the available evidence, not whether the code uses a field:

- **Verified**: confirmed through device observations or captured frames, with the
  available evidence stated; not a claim that every Hisense model has been tested.
- **Unverified**: mapping retained from the original struct without field-specific
  confirmation recorded here. This includes used compatibility mappings.
- **Unknown**: no established meaning.
- **Partial**: only some fields within a grouped area have established meanings.

A field with a fault-like name is not necessarily an active-high fault indication.
Units, scaling and polarity must not be inferred solely from its name or usage.

For bitfields, the mask identifies bits before shifting. All are unsigned;
single-bit fields express an originally named flag, not necessarily a verified
boolean meaning.
`u8` and `s8` denote the original unsigned and signed byte interpretations.
No additional scaling or multi-byte value construction is implied.

### Header and primary status

| Offset | Mask / original type | Original field | Usage | Verification | Description |
| --- | --- | --- | --- | --- | --- |
| 0-15 | `uint8_t[16]` | `header` | Framing | Partial | See envelope and class checks above; not all header bytes have established meanings |
| 16 | u8 | `wind_status` | Used | Unverified | Fan/air-volume code |
| 17 | u8 | `sleep_status` | Unused | Unverified | Sleep code |
| 18 | `0x03` | `direction_status` | Unused | Unverified | Wind direction |
| 18 | `0x0C` | `run_status` | Used | Unverified | Run bits, shifted right 2 |
| 18 | `0xF0` | `mode_status` | Used | Unverified | Operating mode, shifted right 4 |
| 19 | u8 | `indoor_temperature_setting` | Used | Unverified | Target temperature |
| 20 | u8 | `indoor_temperature_status` | Used | Unverified | Room temperature |
| 21 | u8 | `indoor_pipe_temperature` | Used | Unverified | Pipe temperature |
| 22 | s8 | `indoor_humidity_setting` | Used | Unverified | Retained humidity setting interpretation |
| 23 | s8 | `indoor_humidity_status` | Used | Unverified | Retained humidity reading interpretation |
| 24 | u8 | `somatosensory_temperature` | Unused | Unverified | Sensible temperature |
| 25 | `0x07` | `somatosensory_compensation_ctrl` | Unused | Unverified | Compensation control |
| 25 | `0xF8` | `somatosensory_compensation` | Unused | Unverified | Compensation value |
| 26 | `0x07` | `temperature_Fahrenheit` | Unused | Unverified | Fahrenheit display field, not verified protocol-unit detection |
| 26 | `0xF8` | `temperature_compensation` | Unused | Unverified | Temperature compensation |
| 27 | u8 | `timer` | Unused | Unverified | Timer |
| 28 | u8 | `hour` | Unused | Unverified | Hour |
| 29 | u8 | `minute` | Unused | Unverified | Minute |
| 30 | u8 | `poweron_hour` | Unused | Unverified | Power-on hour |
| 31 | u8 | `poweron_minute` | Unused | Unverified | Power-on minute |
| 32 | u8 | `poweroff_hour` | Unused | Unverified | Power-off hour |
| 33 | u8 | `poweroff_minute` | Unused | Unverified | Power-off minute |
| 34 | `0x0F` | `wind_door` | Unused | Unverified | Wind-door field |
| 34 | `0xF0` | `drying` | Unused | Unverified | Drying field |

### Feature, display and diagnostic flags

| Offset | Mask | Original field | Usage | Verification | Description |
| --- | --- | --- | --- | --- | --- |
| 35 | `0x01` | `dual_frequency` | Unused | Unverified | Meaning not confirmed |
| 35 | `0x02` | `efficient` | Unused | Unverified | Meaning not confirmed |
| 35 | `0x04` | `low_electricity` | Unused | Unverified | Save electricity |
| 35 | `0x08` | `low_power` | Unused | Unverified | Energy saving |
| 35 | `0x10` | `heat` | Unused | Unverified | Heating air |
| 35 | `0x20` | `nature` | Unused | Unverified | Natural wind |
| 35 | `0x40` | `left_right` | Used | Unverified | Horizontal swing |
| 35 | `0x80` | `up_down` | Used | Unverified | Vertical swing |
| 36 | `0x01` | `smoke` | Unused | Unverified | Smoke removal |
| 36 | `0x02` | `voice` | Unused | Unverified | Meaning not confirmed |
| 36 | `0x04` | `mute` | Unused | Unverified | Meaning not confirmed |
| 36 | `0x08` | `smart_eye` | Unused | Unverified | Meaning not confirmed |
| 36 | `0x10` | `outdoor_clear` | Unused | Unverified | Outdoor cleaning |
| 36 | `0x20` | `indoor_clear` | Unused | Unverified | Indoor cleaning |
| 36 | `0x40` | `swap` | Unused | Unverified | Change the wind |
| 36 | `0x80` | `dew` | Unused | Unverified | Fresh |
| 37 | `0x01` | `indoor_electric` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x02` | `right_wind` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x04` | `left_wind` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x08` | `filter_reset` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x10` | `indoor_led` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x20` | `indicate_led` | Unused | Unverified | Meaning not confirmed |
| 37 | `0x40` | `display_led` | Unused | Unverified | Not the observed display-state bit on the tested ACOND unit |
| 37 | `0x80` | `back_led` | Used | Verified | Display state; confirmed on the ACOND unit and the maintainer's device (see evidence above) |
| 38 | `0x01` | `indoor_eeprom` | Unused | Unverified | EEPROM |
| 38 | `0x02` | `sample` | Unused | Unverified | Meaning not confirmed |
| 38 | `0x3C` | `rev23` | Reserved | Unknown | Four reserved bits |
| 38 | `0x40` | `time_lapse` | Unused | Unverified | Meaning not confirmed |
| 38 | `0x80` | `auto_check` | Unused | Unverified | Self-test |
| 39 | `0x01` | `indoor_outdoor_communication` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x02` | `indoor_zero_voltage` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x04` | `indoor_bars` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x08` | `indoor_machine_run` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x10` | `indoor_water_pump` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x20` | `indoor_humidity_sensor` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x40` | `indoor_temperature_pipe_sensor` | Unused | Unverified | Meaning not confirmed |
| 39 | `0x80` | `indoor_temperature_sensor` | Unused | Unverified | Meaning not confirmed |
| 40 | `0x07` | `rev25` | Reserved | Unknown | Three reserved bits |
| 40 | `0x08` | `eeprom_communication` | Unused | Unverified | Meaning not confirmed |
| 40 | `0x10` | `electric_communication` | Unused | Unverified | Meaning not confirmed |
| 40 | `0x20` | `keypad_communication` | Unused | Unverified | Meaning not confirmed |
| 40 | `0x40` | `display_communication` | Unused | Unverified | Meaning not confirmed |
| 40 | `0x80` | (unnamed) | Reserved | Unknown | Unused bit in the original declaration |

### Compressor, electrical fields and trailing data

| Offset | Mask / original type | Original field | Usage | Verification | Description |
| --- | --- | --- | --- | --- | --- |
| 41 | u8 | `compressor_frequency` | Used | Unverified | Compressor frequency |
| 42 | u8 | `compressor_frequency_setting` | Used | Unverified | Compressor frequency setting |
| 43 | u8 | `compressor_frequency_send` | Used | Unverified | Sent compressor frequency |
| 44 | s8 | `outdoor_temperature` | Used | Unverified | Retained temperature interpretation |
| 45 | s8 | `outdoor_condenser_temperature` | Used | Unverified | Retained temperature interpretation |
| 46 | s8 | `compressor_exhaust_temperature` | Used | Unverified | Retained temperature interpretation |
| 47 | s8 | `target_exhaust_temperature` | Used | Unverified | Retained temperature interpretation |
| 48 | u8 | `expand_threshold` | Unused | Unverified | Expansion threshold |
| 49 | u8 | `UAB_HIGH` | Unused | Unverified | Named high byte, unverified units/scaling |
| 50 | u8 | `UAB_LOW` | Unused | Unverified | Named low byte |
| 51 | u8 | `UBC_HIGH` | Unused | Unverified | Named high byte |
| 52 | u8 | `UBC_LOW` | Unused | Unverified | Named low byte |
| 53 | u8 | `UCA_HIGH` | Unused | Unverified | Named high byte |
| 54 | u8 | `UCA_LOW` | Unused | Unverified | Named low byte |
| 55 | u8 | `IAB` | Unused | Unverified | Meaning not confirmed |
| 56 | u8 | `IBC` | Unused | Unverified | Meaning not confirmed |
| 57 | u8 | `ICA` | Unused | Unverified | Meaning not confirmed |
| 58 | u8 | `generatrix_voltage_high` | Unused | Unverified | Named high byte |
| 59 | u8 | `generatrix_voltage_low` | Unused | Unverified | Named low byte |
| 60 | u8 | `IUV` | Unused | Unverified | Meaning not confirmed |
| 61 | `0x07` | `wind_machine` | Unused | Unverified | Meaning not confirmed |
| 61 | `0x08` | `outdoor_machine` | Unused | Unverified | Meaning not confirmed |
| 61 | `0x10` | `four_way` | Unused | Unverified | Meaning not confirmed |
| 61 | `0xE0` | `rev46` | Reserved | Unknown | Three reserved bits |
| 62-71 | ten u8 fields | `rev47` through `rev56`, respectively | Reserved | Unknown | One reserved byte per field |
| 72-77 | `uint8_t[6]` | `extra` | Unused | Unknown | Original six-byte tail |
| 78-79 | originally `uint16_t` | `chk_sum` | Framing | Verified | Decoded explicitly as big-endian, not native-endian struct access; see captures above |
| 80-81 | `uint8_t[2]` | `foooter` | Framing | Verified | `F4 FB`; original spelling retained; see captures above |

The trailing offsets above describe **only the original 82-byte frame layout**.
For another frame length, the checksum/footer positions are relative to its
validated end; do not assume that an extended tail shares these fixed positions
or that its extra bytes have known meanings. Compiler tail padding from the old
`aligned(4)` attribute is not protocol data.

To add a feature, verify the field meaning on the relevant model, add explicit
decoding and regression evidence, and then extend `DeviceStatus` if the runtime
needs the value. Do not decode-and-discard fields just to mirror this reference.

## Pure C++ API

`protocol::FrameParser::feed(uint8_t byte, uint32_t now_ms)` returns zero until a
complete valid envelope arrives, then its decoded size. `data()` exposes that
frame until the next `feed()` call. Consume or copy it before feeding more bytes.
`reset()` clears all framing/escape/timestamp state. `expire(now_ms)` lets the
component discard a stale partial frame even with no subsequent UART traffic.

`protocol::decode_status(frame, size, DeviceStatus &out)` independently validates
the complete envelope and the 82-byte or experimental 160-byte layout, then
assigns only consumed fields to the typed snapshot. On
failure `out` is unchanged. It uses explicit unsigned masks, big-endian checksum
decoding and signed arithmetic, never packed bitfields or a raw-buffer cast.

Native tests link this production implementation. Generated frame builders in
the tests are synthetic stimulus only; the independently retained public captures
and all original command goldens anchor checksum/framing checks.

## Asynchronous transactions

Each instance owns an eight-operation pending queue and one active operation.
`control()`, display writes, and periodic `update()` only enqueue work.
`loop()` consumes at most 512 RX bytes per iteration and advances a timestamp-
driven state machine; it does not sleep, flush the UART, or wait for responses.

A fresh baseline status precedes an operation. Its power/mode, setpoint, fan,
swing, preset, and display steps retain their prerequisite ordering. After a
control packet drains, a 500 ms settling interval is followed by an explicit
status poll. Only status received after poll drain, within its 500 ms response
window, can satisfy that step's expected fields. A stale but valid mismatch can
cause additional polls within the operation lifetime, never a control retry.

Acceptance is atomic for combined calls. Only consecutive unsent temperature,
fan, or display-only requests of the same kind can supersede each other; mode
barriers and active commands are preserved. Polls are deduplicated. A failure
cancels dependent queued work and schedules at most one read-only recovery poll.
Queued and active operations expire 10 seconds after acceptance, including when
there is no RX traffic, and expired controls are not replayed after reconnection.

There are no verified unique transaction IDs. A late unsolicited status cannot
always be distinguished from a poll response. Matching the expected reported
fields is stronger than treating any packet as an ACK, but is not unique command
correlation. Preset feedback remains unverified; sending those command bytes
does not result in a fabricated confirmed preset.

### UART backend and timing assumptions

The supported native ESP32 IDF UART is exclusively owned by one AC component.
Final validation rejects other UART consumers, direct YAML `uart.write`, debug
callbacks and flow-control configuration. Lambdas must not bypass ownership.
Outgoing frames are bounded to 64 wire bytes and must fit the hardware FIFO.

In the inspected ESPHome 2026.8.2 / ESP-IDF 5.5.5 backend, the TX ring buffer is
disabled. `uart_write_bytes()` copies into available FIFO space; it can wait when
that space is insufficient. With exclusive ownership and a complete short frame
in an idle FIFO, no line-drain wait is needed. The engine prevents another write
until `ceil(wire_bytes * 10 * 1000 / 9600) + 2 ms` has elapsed. Compile-time and
LP-UART runtime checks reject insufficient FIFO capacity.

This is a driver-based timing model, **not a hardware latency measurement**.
Writes taking at least 10 ms emit a warning. Other backends, manual UART access,
and arbitrary RS485 direction-control hardware are not covered. Hardware
acceptance must include callback latency and disconnect/burst behavior.

## Reported state, diagnostics and evidence limits

Climate and sensor state are published from validated snapshots, not when a
poll is queued. By default controls also remain device-reported. Optional
optimistic presentation overlays accepted controls only; measurements and
compressor action remain reported. Per-field generations prevent an older
operation from removing a newer request's presentation.

Optional communication health, status age, parser-error, response-timeout and
queue-rejection entities are documented in the configuration guide. Parser
error counters survive parser resets, saturate rather than wrap, and reset on
device reboot. Error logs are rate-limited and identify the last failure reason.
Unknown payload tail meanings, named hardware faults and preset feedback are
not inferred from legacy field names.

## Swing control

All 16 transitions between Off, Vertical, Horizontal and Both are supported.
Each axis uses a toggle command, so the component first reads the actual swing
state and confirms each change before sending another. Desired states, not
precomputed toggles, are queued. Commands are not blindly retried: if a response
is lost or the remote changes an axis between steps, the operation fails with a
warning and the component requests status instead of guessing.

## Temperature boundary

Climate values, pending targets, and remembered heat/cool setpoints are Celsius.
`temperature_unit` selects the protocol encoding, not the frontend display unit.
Fahrenheit setpoints are rounded to whole Fahrenheit degrees before transmission;
optimistic state shows that representable value converted back to Celsius.
Invalid/non-finite or out-of-range requests are rejected before integer conversion.

The temperature commands encode 16-32 C and 61-90 F. Default visual limits remain
16-30 C in Celsius mode. Fahrenheit mode uses 61-86 F expressed in Celsius
(approximately 16.111-30 C), with a 5/9 C step. The component does not change the
AC's temperature-display unit. Auxiliary temperatures retain their existing
protocol interpretation; they are not assumed to switch units with the setpoint.

## Bounded command encoding

`encode_temperature(int device_temperature, bool fahrenheit, CommandPacket &out)`
replaces the 47 repetitive temperature arrays with one immutable 44-byte decoded
body template. Its input is the already-normalized integer **device-unit** value;
Celsius normalization/conversion remains at the transport boundary. Only body
offset 17 (full decoded frame offset 19) changes to `2 * value + 1`. All other
body bytes are retained verbatim from the original commands, not reconstructed
from guessed flag meanings. Inputs outside 16–32 C or 61–90 F return false.

`encode_command(body, body_size, output, capacity, size)` accepts the existing
decoded `00 40 length payload...` command body, without delimiters/checksum.
It validates the declared length, sums unsigned decoded body bytes once, emits
the big-endian checksum and doubles interior `F4` bytes. It preflights the entire
escaped wire length against caller capacity and `MAX_COMMAND_WIRE_SIZE` (64).
Invalid input or overflow returns false, sets output size to zero and leaves
output bytes unchanged; commands are never truncated. Local staging also
supports overlapping input/output buffers.

The result is 50 decoded bytes for every temperature command. The original
16 C checksum is `01 F4`, so its wire packet remains **51 bytes**, ending in
`01 F4 F4 F4 FB`. All 77 original command fixtures independently check the common
encoder; temperature fixtures also check the temperature builder, and the other
30 packets remain immutable arrays, including unused variants. No unused command
is activated or assigned new semantics by this refactor.

Each transport Engine owns its temperature packet. Deferred mode-plus-temperature
steps reference that instance-owned buffer until the active operation completes;
queued requests contain values only and cannot overwrite it. Engines are
non-copyable to keep these internal pointers valid. Regression tests interleave
two engines and queue a newer temperature while an earlier temperature step
waits for mode confirmation.

The original `tests/fixtures/commands.json` is not regenerated from production.
Native golden headers are generated from that independent snapshot only.
Additional synthetic tests cover unsigned high-bit bytes, escaped payload and
checksum bytes, exact/insufficient capacity, a full 64-byte wire packet, overflow,
invalid lengths/types/temperatures and unchanged output on failure.
