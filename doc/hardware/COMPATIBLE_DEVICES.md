# Hisense ESPHome Compatibility: Air Conditioners and Wi-Fi Modules

This document lists Hisense, Tornado and ACOND air conditioners confirmed working
with the `hisense_ac` ESPHome component for local Home Assistant control, plus
additional models expected to be compatible based on public protocol information.
Optional features can vary by model; a shared brand name is not a compatibility
guarantee.

[Project overview](../../README.md) | [ESP32 wiring guide](README.md) |
[Report a working model](#adding-your-device)

## Confirmed Working Models

| AC Model | WiFi Module |
|----------|-------------|
| Tornado TOP-INV-120A (WIFI) | AEH-W4F1 |
| Tornado MULTI-12A (WIFI) (AST-09UW4RVETV00D, 2020) | AEH-W4G2 |
| Hisense AST-12UW4RVETG00A | AEH-W4E1 |
| ACOND ASTI-09UW4RVEDC00 | AEH-W4B1 |

The Tornado TOP-INV-120A entry also covers the TOP-INV-140A (WIFI) and TOP-INV-180A (WIFI)
variants, which use the same AEH-W4F1 module.

## Expected compatible models

**We expect these models to be compatible based on publicly available information
about their use of the Hisense RS-485 protocol.** Confirmation with this component
is still pending; they are not yet hardware-tested entries in the confirmed list.
If you own one, your results can help confirm compatibility and any
model-specific limitations.

Original Wi-Fi module identifiers have not yet been confirmed for these entries.

| Expected compatible model | Status with this component |
| --- | --- |
| Hisense CITY DC Inverter AS-13UW4RYRCM04G/04W | Awaiting confirmation |
| Hisense SILVER CRYSTAL SUPER DC Inverter AS-13UW4RVETG01 | Awaiting confirmation |
| Newtek NT-77HSDC12 | Awaiting confirmation |
| Ballu iGreen Pro DC BSAGI-07HN8 | Awaiting confirmation |
| Ballu iGreen Pro DC BSAGI-12HN8 | Awaiting confirmation |
| Ballu iGreen Pro DC BSAGI-18HN8_V4 | Awaiting confirmation |
| Ballu Platinum DC BSEI-09HN8_V3 | Awaiting confirmation |
| Hisense Free Match Multi Split 4AMW81U4RJC | Awaiting confirmation |

Before installation, check the connector pinout, supply and logic voltages
against the [hardware guide](README.md). Shared protocol support is a promising
starting point, but status layouts and individual controls still need checking
on each model. This guide covers an external ESP32 adapter, not reflashing
original Wi-Fi modules.

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

To help confirm an additional model, start with the
[hardware safety guidance](README.md) and provide sanitized
[protocol logs](../configuration/README.md#debug-logging) when investigating.
Do not share passwords, API keys or other secrets. Once results from
**this component on that model** have been reviewed, the model can move to the
confirmed list with any feature-specific limitations recorded.
