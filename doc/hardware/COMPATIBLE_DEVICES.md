# Compatible Devices

This document lists the Hisense AC models that have been tested and confirmed to work with this ESPHome component.

## Confirmed Working Models

| AC Model | WiFi Module |
|----------|-------------|
| Tornado TOP-INV-120A | AEH-W4F1 |
| Hisense AST-12UW4RVETG00A | AEH-W4E1 |
| ACOND ASTI-09UW4RVEDC00 | AEH-W4B1 |

These are existing community-reported compatibility entries, not a claim that
every subsequent firmware change has been retested on each unit. Display
feedback via status byte 37 bit `0x80` is verified on the listed ACOND
model/module and independently confirmed on the maintainer's device. The
additional report did not identify its model/module, so it does not establish
another named entry or universal compatibility.

The parser also has public packet fixtures for 82-byte and 160-byte status
responses. In particular, the ADT-09UX4RBL8 packet in
[issue #6](https://github.com/akrabi/hisense_ac_esphome/issues/6) establishes a
valid frame, **not a verified status layout or control compatibility**.
This experimental branch enables shared-prefix decoding for a trial on that
unit; the model is not added to the confirmed list. See
[testing instructions](../adt-09ux4rbl8-testing.md) and
[protocol evidence](../protocol.md) before adding a model based on a successful
packet decode. No hybrid IR controller or zoffypal v3 model-specific support is
included.

## Adding Your Device

If you've successfully used this component with a Hisense AC model not listed here, please consider contributing by:

1. Fork the repository
2. Add your device to this list with the following information:
   - Model number
   - Year of manufacture (if known)
   - Connection details (pin layout, voltage levels, etc.)
   - Any special configuration requirements
   - Any limitations or issues encountered
3. Submit a pull request
