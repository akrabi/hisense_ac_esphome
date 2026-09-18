# UART receive protocol and evidence

## Supported envelope

The receive parser supports decoded frames of 9–264 bytes, covering every value
of the one-byte declared payload length (`255 + 9` maximum):

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
Wire size may be larger because of stuffing. The 264-byte decoded capacity is
derived from the supported envelope, not from one model's observed response size.

Each component has its own fixed-capacity parser and decoded snapshot. Partial
frames expire after **100 ms between consumed bytes**, using rollover-safe
elapsed time. This is a conservative software recovery threshold, not measured
AC timing. UART buffering means consumption times are not wire-arrival times.
Bad checksums, malformed escapes/lengths/footers and timeouts never acknowledge
a command. Resynchronization recognizes `F4 F5`, including overlapping headers
and a new header beginning where an invalid footer was expected.

## Supported status layout

Checksum-valid **82-byte and 160-byte** responses with **offset 13 = `0x66`**
are decoded using only the existing shared status prefix (consumed fields end at
offset 47). Both evidenced length and class are required. All other lengths
remain framing-only: a longer valid envelope does not establish a status layout.
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
- [Issue #6](https://github.com/akrabi/hisense_ac_esphome/issues/6), first RX at
  `18:28:36`: the two consecutive RX log fragments form one 160-byte frame.
  Declared length `97`, checksum `09 BA`. Its header offsets 0–15 match the
  issue #1 header **except offset 4 (length)**; offsets 5–15 in both are
  `01 00 FE 01 01 01 01 00 66 00 01`. There is no shifted/extra header byte before
  the existing prefix at offset 16. These packet-only fixtures and provenance
  are retained in [tests/README.md](../tests/README.md).

This is receive-layout compatibility, **not a claim of model-specific control,
capability, sensor meaning or sentinel handling**. No tail fields are inferred
from expanded fork structs; offsets after the consumed prefix remain opaque
but are included in checksum verification through the final payload byte.
No zoffypal v3 or other model-specific protocol is enabled.

Do not infer response semantics from inconsistent upstream prose or fixture
descriptions. Short responses and other classes/lengths are deliberately not
decoded or treated as command success. No short acknowledgment format or unique
transaction correlation has been verified. A recognized status still releases
the **existing** response wait for compatibility; it does not prove execution of
the last command. The existing 500 ms send timeout remains unchanged.

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
across models. The existing backlight-bit verification applies only to
ACOND ASTI-09UW4RVEDC00 / AEH-W4B1.

Unused fields from the removed packed wire struct remain **unverified**, not
new capabilities: sleep, direction, somatosensory compensation, Fahrenheit and
temperature-compensation flags, timers, wind door/drying, dual frequency,
efficient/low-power/heat/nature, smoke/voice/mute/smart-eye/cleaning/swap/dew,
electrical/wind/filter/other LED flags, EEPROM/self-test/time-lapse, all fault
and communication flags, electrical voltage/current fields, expansion threshold,
outdoor-machine/four-way flags and reserved/extra bytes. Do not expose or decode
them based solely on their historical names.

## Historical complete status map

This reference preserves the information from the packed `Device_Status` in
commit `e65774a` (before its replacement with a decoded snapshot). It is not a
second decoder, a specification supplied by Hisense, or a claim that every named
field works on every unit. `DeviceStatus` deliberately contains only consumed
values; the parser still validates the entire frame, including unused bytes.

Offsets below are **zero-based decoded frame offsets**, after removing byte
stuffing, not positions in the escaped UART stream. They reconstruct the
historical ESP32 layout with bitfields allocated from the least-significant bit
upward. C++ bitfield ordering is implementation-dependent; this table must not be
used to justify casting a byte buffer to a struct. In particular, the original
byte 40 declaration used only seven bits; its eighth bit was unnamed padding.

Evidence labels:

- **Used**: explicitly decoded by the component, preserving its existing mapping;
  this does not establish universal model support.
- **Model-verified**: a specific observation with the model qualification stated.
- **Historical**: original name/type/comment only; interpretation, units, encoding,
  polarity and applicability remain unverified. A field with a fault-like name
  is not necessarily an active-high fault indication.
- **Opaque**: reserved, unnamed or uninterpreted bytes/bits.
- **Framing**: interpreted by frame validation rather than copied to `DeviceStatus`.

For bitfields, the mask identifies bits before shifting. All are unsigned;
single-bit fields express a historical flag, not a verified boolean meaning.
`u8` and `s8` denote the original unsigned and signed byte interpretations.
No additional scaling or multi-byte value construction is implied.

### Header and primary status

| Offset | Mask / original type | Original field | Evidence / historical description |
| --- | --- | --- | --- |
| 0-15 | `uint8_t[16]` | `header` | Framing/header area; see envelope and class checks above. Not all header bytes have established meanings. |
| 16 | u8 | `wind_status` | Used; fan/air-volume code |
| 17 | u8 | `sleep_status` | Historical; sleep code |
| 18 | `0x03` | `direction_status` | Historical; wind direction |
| 18 | `0x0C` | `run_status` | Used; run bits, shifted right 2 |
| 18 | `0xF0` | `mode_status` | Used; operating mode, shifted right 4 |
| 19 | u8 | `indoor_temperature_setting` | Used; target temperature |
| 20 | u8 | `indoor_temperature_status` | Used; room temperature |
| 21 | u8 | `indoor_pipe_temperature` | Used; pipe temperature |
| 22 | s8 | `indoor_humidity_setting` | Used; historical humidity setting interpretation |
| 23 | s8 | `indoor_humidity_status` | Used; historical humidity reading interpretation |
| 24 | u8 | `somatosensory_temperature` | Historical; sensible temperature |
| 25 | `0x07` | `somatosensory_compensation_ctrl` | Historical; compensation control |
| 25 | `0xF8` | `somatosensory_compensation` | Historical; compensation value |
| 26 | `0x07` | `temperature_Fahrenheit` | Historical; Fahrenheit display field, not verified protocol-unit detection |
| 26 | `0xF8` | `temperature_compensation` | Historical; temperature compensation |
| 27 | u8 | `timer` | Historical; timer |
| 28 | u8 | `hour` | Historical; hour |
| 29 | u8 | `minute` | Historical; minute |
| 30 | u8 | `poweron_hour` | Historical; power-on hour |
| 31 | u8 | `poweron_minute` | Historical; power-on minute |
| 32 | u8 | `poweroff_hour` | Historical; power-off hour |
| 33 | u8 | `poweroff_minute` | Historical; power-off minute |
| 34 | `0x0F` | `wind_door` | Historical; wind-door field |
| 34 | `0xF0` | `drying` | Historical; drying field |

### Feature, display and diagnostic flags

| Offset | Mask | Original field | Evidence / historical description |
| --- | --- | --- | --- |
| 35 | `0x01` | `dual_frequency` | Historical |
| 35 | `0x02` | `efficient` | Historical |
| 35 | `0x04` | `low_electricity` | Historical; save electricity |
| 35 | `0x08` | `low_power` | Historical; energy saving |
| 35 | `0x10` | `heat` | Historical; heating air |
| 35 | `0x20` | `nature` | Historical; natural wind |
| 35 | `0x40` | `left_right` | Used; horizontal swing |
| 35 | `0x80` | `up_down` | Used; vertical swing |
| 36 | `0x01` | `smoke` | Historical; smoke removal |
| 36 | `0x02` | `voice` | Historical |
| 36 | `0x04` | `mute` | Historical |
| 36 | `0x08` | `smart_eye` | Historical |
| 36 | `0x10` | `outdoor_clear` | Historical; outdoor cleaning |
| 36 | `0x20` | `indoor_clear` | Historical; indoor cleaning |
| 36 | `0x40` | `swap` | Historical; change the wind |
| 36 | `0x80` | `dew` | Historical; fresh |
| 37 | `0x01` | `indoor_electric` | Historical |
| 37 | `0x02` | `right_wind` | Historical |
| 37 | `0x04` | `left_wind` | Historical |
| 37 | `0x08` | `filter_reset` | Historical |
| 37 | `0x10` | `indoor_led` | Historical |
| 37 | `0x20` | `indicate_led` | Historical |
| 37 | `0x40` | `display_led` | Historical; not the observed display-state bit on the tested ACOND unit |
| 37 | `0x80` | `back_led` | Used, model-verified for display state on ACOND ASTI-09UW4RVEDC00 / AEH-W4B1 only |
| 38 | `0x01` | `indoor_eeprom` | Historical; EEPROM |
| 38 | `0x02` | `sample` | Historical |
| 38 | `0x3C` | `rev23` | Opaque; four reserved bits |
| 38 | `0x40` | `time_lapse` | Historical |
| 38 | `0x80` | `auto_check` | Historical; self-test |
| 39 | `0x01` | `indoor_outdoor_communication` | Historical |
| 39 | `0x02` | `indoor_zero_voltage` | Historical |
| 39 | `0x04` | `indoor_bars` | Historical |
| 39 | `0x08` | `indoor_machine_run` | Historical |
| 39 | `0x10` | `indoor_water_pump` | Historical |
| 39 | `0x20` | `indoor_humidity_sensor` | Historical |
| 39 | `0x40` | `indoor_temperature_pipe_sensor` | Historical |
| 39 | `0x80` | `indoor_temperature_sensor` | Historical |
| 40 | `0x07` | `rev25` | Opaque; three reserved bits |
| 40 | `0x08` | `eeprom_communication` | Historical |
| 40 | `0x10` | `electric_communication` | Historical |
| 40 | `0x20` | `keypad_communication` | Historical |
| 40 | `0x40` | `display_communication` | Historical |
| 40 | `0x80` | (unnamed) | Opaque; unused bit in the original declaration |

### Compressor, electrical fields and trailing data

| Offset | Mask / original type | Original field | Evidence / historical description |
| --- | --- | --- | --- |
| 41 | u8 | `compressor_frequency` | Used; compressor frequency |
| 42 | u8 | `compressor_frequency_setting` | Used; compressor frequency setting |
| 43 | u8 | `compressor_frequency_send` | Used; sent compressor frequency |
| 44 | s8 | `outdoor_temperature` | Used |
| 45 | s8 | `outdoor_condenser_temperature` | Used |
| 46 | s8 | `compressor_exhaust_temperature` | Used |
| 47 | s8 | `target_exhaust_temperature` | Used |
| 48 | u8 | `expand_threshold` | Historical; expansion threshold |
| 49 | u8 | `UAB_HIGH` | Historical; named high byte, unverified units/scaling |
| 50 | u8 | `UAB_LOW` | Historical; named low byte |
| 51 | u8 | `UBC_HIGH` | Historical; named high byte |
| 52 | u8 | `UBC_LOW` | Historical; named low byte |
| 53 | u8 | `UCA_HIGH` | Historical; named high byte |
| 54 | u8 | `UCA_LOW` | Historical; named low byte |
| 55 | u8 | `IAB` | Historical |
| 56 | u8 | `IBC` | Historical |
| 57 | u8 | `ICA` | Historical |
| 58 | u8 | `generatrix_voltage_high` | Historical; named high byte |
| 59 | u8 | `generatrix_voltage_low` | Historical; named low byte |
| 60 | u8 | `IUV` | Historical |
| 61 | `0x07` | `wind_machine` | Historical |
| 61 | `0x08` | `outdoor_machine` | Historical |
| 61 | `0x10` | `four_way` | Historical |
| 61 | `0xE0` | `rev46` | Opaque; three reserved bits |
| 62-71 | ten u8 fields | `rev47` through `rev56`, respectively | Opaque; one reserved byte per field |
| 72-77 | `uint8_t[6]` | `extra` | Opaque; historical six-byte tail |
| 78-79 | originally `uint16_t` | `chk_sum` | Framing; decoded explicitly as big-endian, not native-endian struct access |
| 80-81 | `uint8_t[2]` | `foooter` | Framing; `F4 FB` (historical spelling retained) |

The trailing offsets above describe **only the historical 82-byte frame**.
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
the complete envelope and the evidenced 82/160-byte layouts, then assigns only
the shared prefix to the typed snapshot. On
failure `out` is unchanged. It uses explicit unsigned masks, big-endian checksum
decoding and signed arithmetic, never packed bitfields or a raw-buffer cast.

Native tests link this production implementation. Generated frame builders in
the tests are synthetic stimulus only; the independently retained public captures
and all original command goldens anchor checksum/framing checks.

## Deliberately unchanged

The queue remains synchronous/function-static, and existing polling publication,
optimistic control behavior, display-confirmation policy, swing commands and
temperature units are unchanged. The snapshot is initialized, but measurement
publication before first valid status still needs the planned state-sync change.
This parser milestone does not claim transport/state synchronization is fixed.
