# Compatible Devices

This document distinguishes models reported working with this ESPHome component
from unverified candidates. Matching a brand, protocol family or
Wi-Fi module does not by itself establish compatibility or support for every
optional feature.

## Confirmed Working Models

| AC Model | WiFi Module |
|----------|-------------|
| Tornado TOP-INV-120A (WIFI) | AEH-W4F1 |
| Tornado MULTI-12A (WIFI) (AST-09UW4RVETV00D, 2020) | AEH-W4G2 |
| Hisense AST-12UW4RVETG00A | AEH-W4E1 |
| ACOND ASTI-09UW4RVEDC00 | AEH-W4B1 |

The Tornado TOP-INV-120A entry also covers the TOP-INV-140A (WIFI) and TOP-INV-180A (WIFI)
variants, which use the same AEH-W4F1 module.

## Unverified candidate models

**These models are not confirmed or advertised as supported by this component.**
They are listed to help owners identify candidates for investigation, not as a
recommendation to buy hardware or replace a working module.

These candidates are based on publicly available model and protocol information,
not hardware validation with this component. Their original Wi-Fi module
identifiers have not been verified here, so none is inferred.

| Candidate model | Status with this component |
| --- | --- |
| Hisense CITY DC Inverter AS-13UW4RYRCM04G/04W | Unverified |
| Hisense SILVER CRYSTAL SUPER DC Inverter AS-13UW4RVETG01 | Unverified |
| Newtek NT-77HSDC12 | Unverified |
| Ballu iGreen Pro DC BSAGI-07HN8 | Unverified |
| Ballu iGreen Pro DC BSAGI-12HN8 | Unverified |
| Ballu iGreen Pro DC BSAGI-18HN8_V4 | Unverified |
| Ballu Platinum DC BSEI-09HN8_V3 | Unverified |
| Hisense Free Match Multi Split 4AMW81U4RJC | Unverified |

This list does not establish AEH-W4G1 support or endorse reflashing original
Wi-Fi modules. UART wiring, voltage levels, response layouts and control
encodings need model-specific verification. A valid packet or a successful
firmware build alone is not enough to mark a model working.

## Adding Your Device

Open an [issue](https://github.com/akrabi/hisense_ac_esphome/issues/new) or submit
a pull request with:

- Exact AC model, year if known, and original Wi-Fi module identifier.
- ESP32 board, RS-485 adapter, connector pinout, supply and logic voltages.
- ESPHome version and this component's release or commit.
- Sanitized configuration and any special setup requirements.
- Results for power, modes, temperature, fan and swing; optional display,
  presets, sensors or Dry adjustment only if tested.
- Whether Home Assistant follows physical-remote changes and recovers after
  communication is interrupted, plus any limitations or failed controls.

For an unverified candidate, start with the
[hardware safety guidance](README.md) and provide sanitized
[protocol logs](../configuration/README.md#debug-logging) when investigating.
Do not share passwords, API keys or other secrets. Move a candidate to the
confirmed list only after evidence from **this component on that model** has
been reviewed, recording feature-specific limitations.
