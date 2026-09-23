# Hisense UART protocol

This describes the protocol subset supported by this component, not a universal
Hisense specification. All offsets are **zero-based decoded frame offsets**,
after removing byte stuffing. Fixture provenance and validation results belong
in [tests/README.md](../tests/README.md).

## Frame format

The bus uses **9600 baud, 8 data bits, no parity, 1 stop bit**. One component
exclusively owns each UART; other readers, writers and `uart.debug` are unsupported.

| Offset | Meaning |
| --- | --- |
| 0-1 | Header `F4 F5` |
| 2-3 | Commands: `00 40`; responses: `01 40` |
| 4 | Length value; total decoded size is this value + 9 |
| 5 through size - 5 | Payload, including message class at offset 13 when present |
| size - 4, size - 3 | Additive checksum, big-endian |
| size - 2, size - 1 | Footer `F4 FB` |

The checksum is the unsigned 16-bit sum of decoded bytes from offset 2 through
size - 5, inclusive. Interior `F4` bytes, including checksum bytes, are doubled
on the wire; each pair contributes one decoded byte. Header/footer markers are
not stuffed, and stuffing does not change the declared length.

The receive parser accepts **9-128 decoded bytes**. Partial frames expire after
100 ms between consumed bytes; this is a software recovery threshold, not a
measured bus requirement. Invalid headers, lengths, escapes, checksums and
footers are rejected, with resynchronization at `F4 F5`.

## Message classes

| Class at offset 13 | Direction / purpose | Component handling |
| --- | --- | --- |
| `0x65` | Control command | Sends the requested control bytes |
| `0x65` | Control response | Recognizes the 82-byte shape for diagnostics only |
| `0x66` | Status query | Sends a read-only poll |
| `0x66` | Status response | Decodes only checksum-valid 82-byte frames |

The status query is:

```text
F4 F5 00 40 0C 00 00 01 01 FE 01 00 00 66 00 00 00 01 B3 F4 FB
```

Other response classes and lengths are not decoded as status. In particular,
160-byte status variants are unsupported, even when their checksums are valid.

### Control responses (class `0x65`)

An 82-byte control response can contain status-shaped data, but its class alone
does not indicate success or rejection. The component logs it as a control
response without publishing state, refreshing communication health/status age,
or advancing an operation. Confirmation requires matching fields in a subsequent
`0x66` response after an explicit poll. No result-code or short-ACK handling is
implemented.

## Supported status layout

An 82-byte status response has length byte `49`, class `66`, data at offsets
16-77, checksum at 78-79, and footer at 80-81. The component consumes:

| Offset | Field / decoding |
| --- | --- |
| 16 | Fan code; see below |
| 18 | Run: `(byte & 0x0C) >> 2`; mode: `byte >> 4` |
| 19 | Ordinary target temperature, unsigned |
| 20 | Room temperature, unsigned |
| 21 | Indoor pipe temperature, unsigned |
| 22-23 | Humidity setting / reading, signed 8-bit |
| 35 | Horizontal swing: `0x40`; vertical swing: `0x80` |
| 37 | Display backlight: `0x80` |
| 41-43 | Compressor frequency / setting / sent frequency, unsigned |
| 44-47 | Outdoor / condenser / exhaust / target exhaust temperatures, signed 8-bit |

Run value zero means Off; nonzero means powered on. Mode codes are `0` Fan,
`1` Heat, `2` Cool and `3` Dry. Unknown enum values retain the last recognized
field rather than inventing a new state. Humidity meanings, sensor scaling and
compressor-field ordering are compatibility mappings, not verified across models.
Unused bytes still participate in checksum validation.

### Fan status and Auto confirmation

| Raw byte 16 | Presentation | Command matching |
| --- | --- | --- |
| `00`, `01` | Auto | Normalized to internal Auto value `0` |
| `02` | Quiet | Exact match |
| `0A` (10) | Low, physical speed 1 | Confirms Low |
| `0C` (12) | Low, physical speed 2 | Does not confirm Low |
| `0E` (14) | Medium, physical speed 3 | Confirms Medium |
| `10` (16) | Medium, physical speed 4 | Does not confirm Medium |
| `12` (18) | High, physical speed 5 | Confirms High |

Five-speed readback and Auto `01` are established on Tornado TOP-INV-120A (WIFI)
with AEH-W4F1. Auto `00` and Quiet `02` remain compatibility mappings. Low/Medium
grouping affects presentation only: raw feedback remains distinct, and outgoing
Low/Medium/High commands request speeds 1/3/5. Individual commands for speeds 2/4
and cross-model mappings are unverified.

### Dry-mode adjustment

On Tornado TOP-INV-120A (WIFI) with AEH-W4F1, status byte 26's upper nibble encodes
the Dry adjustment as **sign-and-magnitude**: bit 7 is the negative sign and
bits 4-6 are magnitude 0-7. Positive values 1-7 use upper nibbles `1`-`7`;
negative values use `9`-`F`; neutral uses `0`. Upper nibble `8` is unverified.
The lower nibble's meaning is unknown.

Byte 19 is not the adjustment. The signed interpretation applies only to Dry,
and the adjustment's physical units and baseline are unknown. Ordinary
temperature commands do not reliably set it. This component does not yet decode
or write the Dry adjustment.

## Unused status fields

The following names and masks preserve the historical packed `Device_Status`
layout from commit `e65774a`. They are **not implemented capabilities**. Except
for the Dry upper nibble described above, their meanings, units and polarity are
unverified. A fault-like name does not establish an active fault indication.
Masks refer to bits before shifting; unqualified entries occupy a whole byte.
Never cast a frame to a C++ bitfield struct: allocation order and padding are
implementation-dependent.

| Offset | Historical fields (mask where applicable) |
| --- | --- |
| 17 | `sleep_status` |
| 18 | `direction_status` (`03`) |
| 24 | `somatosensory_temperature` |
| 25 | `somatosensory_compensation_ctrl` (`07`), `somatosensory_compensation` (`F8`) |
| 26 | `temperature_Fahrenheit` (`07`), `temperature_compensation` (`F8`; includes unexplained bit 3) |
| 27-33 | `timer`, `hour`, `minute`, `poweron_hour`, `poweron_minute`, `poweroff_hour`, `poweroff_minute` |
| 34 | `wind_door` (`0F`), `drying` (`F0`; no established link to Dry adjustment) |
| 35 | `dual_frequency` (`01`), `efficient` (`02`), `low_electricity` (`04`), `low_power` (`08`), `heat` (`10`), `nature` (`20`) |
| 36 | `smoke` (`01`), `voice` (`02`), `mute` (`04`), `smart_eye` (`08`), `outdoor_clear` (`10`), `indoor_clear` (`20`), `swap` (`40`), `dew` (`80`) |
| 37 | `indoor_electric` (`01`), `right_wind` (`02`), `left_wind` (`04`), `filter_reset` (`08`), `indoor_led` (`10`), `indicate_led` (`20`), `display_led` (`40`; not the supported backlight bit) |
| 38 | `indoor_eeprom` (`01`), `sample` (`02`), reserved `rev23` (`3C`), `time_lapse` (`40`), `auto_check` (`80`) |
| 39 | `indoor_outdoor_communication` (`01`), `indoor_zero_voltage` (`02`), `indoor_bars` (`04`), `indoor_machine_run` (`08`), `indoor_water_pump` (`10`), `indoor_humidity_sensor` (`20`), `indoor_temperature_pipe_sensor` (`40`), `indoor_temperature_sensor` (`80`) |
| 40 | Reserved `rev25` (`07`), `eeprom_communication` (`08`), `electric_communication` (`10`), `keypad_communication` (`20`), `display_communication` (`40`), unnamed padding bit (`80`) |
| 48 | `expand_threshold` |
| 49-54 | `UAB_HIGH`, `UAB_LOW`, `UBC_HIGH`, `UBC_LOW`, `UCA_HIGH`, `UCA_LOW` |
| 55-57 | `IAB`, `IBC`, `ICA` |
| 58-60 | `generatrix_voltage_high`, `generatrix_voltage_low`, `IUV` |
| 61 | `wind_machine` (`07`), `outdoor_machine` (`08`), `four_way` (`10`), reserved `rev46` (`E0`) |
| 62-71 | Reserved bytes `rev47` through `rev56` |
| 72-77 | Opaque six-byte `extra` tail |

These offsets apply only to the 82-byte layout. High/low names do not establish
multi-byte values or scaling. Additional fields require model-specific
verification before decoding or exposure.

## Commands and confirmation

Temperature commands are 50 decoded bytes with length byte `29` and class `65`.
Offset 19 encodes `2 * device_temperature + 1`; the remaining command body is
fixed. Supported ranges are 16-32 C and 61-90 F. Checksums and stuffing follow
the common frame format; the 16 C command is 51 wire bytes because its checksum
contains `F4`.

Climate values are Celsius internally. `temperature_unit` selects wire
encoding, not the AC's display unit. Requests are rounded to whole device-unit
degrees; invalid or out-of-range values are rejected. Auxiliary temperature
readings are not assumed to change units with the setpoint.

Swing commands toggle individual axes. The component reads the current state
and confirms each toggle before sending the next. Preset feedback is unverified;
sending preset bytes does not imply a confirmed preset.

The transaction sequence is **baseline poll -> control -> settle -> confirmation
poll**. The following are component limits, not device-protocol guarantees:

| Setting | Behavior |
| --- | --- |
| Poll interval | 5 seconds by default |
| TX limit | 64 wire bytes, fitting the assigned hardware FIFO |
| TX drain allowance | `ceil(wire_bytes * 10 * 1000 / 9600) + 2 ms` |
| Control settling | 500 ms after drain before polling |
| Poll response window | 500 ms after poll drain |
| Operation lifetime | 10 seconds from acceptance, including queue time |
| Pending capacity | Eight queued operations plus one active operation |

Only supported status arriving within a poll's response window can confirm
matching requested fields. A mismatch can trigger further polls, **never a
setter retry**. Failure cancels dependent queued work and permits at most one
read-only recovery poll; expired controls are not replayed after reconnection.
There is no verified unique transaction ID, so matching status is not proof of
unique command correlation.

## References

- [Configuration and diagnostics](configuration/README.md)
- [Regression tests and capture provenance](../tests/README.md)
- [External protocol implementation, pinned revision 96f355b](https://github.com/straga/hisense_ac_xm_protocol/blob/96f355b12da33c1c187f9d31cace1b02abcf2446/src/protocol/air-condition_msg.c):
  corroborates set/query classes `101`/`102` and Auto feedback `1`; other mappings
  must not be assumed to match this component or every model.
