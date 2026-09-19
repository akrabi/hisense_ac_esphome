# Experimental ADT-09UX4RBL8 trial

Branch: `experimental/adt-09ux4rbl8-160-byte`

This is an isolated experiment for [issue #6](https://github.com/akrabi/hisense_ac_esphome/issues/6),
not a claim of working model support. The main improvements branch accepts
82-byte status only. This branch raises the decoded-frame buffer to 160 bytes
and accepts checksum-valid 160-byte class `0x66` responses.

It assumes existing status offsets also apply to the longer frame. Matching
headers and a valid checksum do not prove this. No tail fields or new commands
are inferred; negative/sentinel sensor readings may still need investigation.
No zoffypal v3 or hybrid IR changes are included.

## Selecting the branch

The branch must first be published by the maintainer before someone else can
fetch it from GitHub. Creating this local branch does not publish it.
After publication, replace the existing external-component source with:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/akrabi/hisense_ac_esphome
      ref: experimental/adt-09ux4rbl8-160-byte
    components: [hisense_ac]
    refresh: 0s
```

Use ESPHome 2026.8.2, the development baseline. Keep the device's existing board,
Wi-Fi, API and wiring configuration; do not use the test fixture's ESP32 board
settings on an ESP32-C3. Compile for the actual board before uploading.
The credential-free `tests/adt_09ux4rbl8.yaml` fixture covers the issue's
ESP32-C3-DevKitM-1 with TX GPIO21 / RX GPIO20, using Arduino, Celsius and
device-reported state. It is a compile fixture, not a deployment configuration.

The UART must be dedicated to this component at 9600/8N1 with RX and TX.
Remove the issue's `uart.debug` / `dummy_receiver` configuration: the async
implementation rejects competing UART readers/writers and debug callbacks.
Set `logger.baud_rate: 0`; ordinary component DEBUG logging is fine. Do not
add raw `uart.write` actions or manual writes through lambdas.

Under the existing `hisense_ac` climate entry, keep:

```yaml
optimistic: false
temperature_unit: CELSIUS
communication_connected:
  name: "AC Communication"
last_status_age:
  name: "AC Last Status Age"
invalid_frame_count:
  name: "AC Invalid Frames"
response_timeout_count:
  name: "AC Response Timeouts"
queue_rejection_count:
  name: "AC Queue Rejections"
```

`temperature_unit` must match the unit's actual protocol encoding. Celsius
matches the original issue configuration; change it only with evidence.

## First check: status reporting

Start without issuing climate commands from Home Assistant. The component
will send status queries. Use the original remote to change the device state
and compare the reported power/mode, target temperature, room temperature,
fan and swing against the real unit. Record unsupported functions rather than
assuming the component's defaults describe this model.

The old issue capture produces target 16 C and room 26 C under this branch's
decoder, but those numbers have not been confirmed against the unit. Treat
them as a hypothesis, not calibration. Communication showing connected only
means a structurally valid status frame arrived; it does not verify all fields.

If status fields disagree, stop before attempting controls and report the
specific differences. A larger buffer alone is not a complete model port.

## Optional control check

Only after the relevant status fields agree, try individual supported controls
and check the actual unit as well as Home Assistant. Stop if behavior differs.
Do not send command bursts or repeatedly retry toggles. Keep the original
remote available and retain the previously working firmware/configuration.

Report the exact AC and original Wi-Fi module, ESP board, ESPHome version,
branch commit, functions tested, observed versus expected states, and component
warning messages. Remove Wi-Fi/API credentials and unrelated identifying data
from any logs before sharing them. No physical testing, device upload or
issue comment is performed by creating this branch.
