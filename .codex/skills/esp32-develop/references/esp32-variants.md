# ESP32 Variant Guide

Use this after the board/chip variant is known, or when a beginner asks which ESP32 board to use.

## Key Lesson

"ESP32" is a family. Do not assume pin count, USB behavior, wireless support, ADC behavior, or flash pins transfer across variants.

## Quick Matrix

| Variant | Architecture | Wireless | Beginner Notes |
|---|---|---|---|
| ESP32 | dual-core Xtensa | Wi-Fi 4, BT Classic, BLE | Legacy workhorse; has ADC2/Wi-Fi conflict and input-only GPIO34-39. |
| ESP32-S2 | single-core Xtensa | Wi-Fi 4 | Native USB OTG; no Bluetooth; some high GPIOs are strapping/input-sensitive. |
| ESP32-S3 | dual-core Xtensa | Wi-Fi 4, BLE 5 | Good for displays, camera, ML-ish workloads; native USB; watch flash/PSRAM pins. |
| ESP32-C3 | single-core RISC-V | Wi-Fi 4, BLE 5 | Small IoT node; native USB-Serial/JTAG on many boards; no ADC2/Wi-Fi conflict. |
| ESP32-C6 | single-core RISC-V | Wi-Fi 6, BLE 5, 802.15.4 | Matter/Thread/Zigbee-capable; native USB-Serial/JTAG; no ADC2/Wi-Fi conflict. |
| ESP32-H2 | single-core RISC-V | BLE, 802.15.4 | No Wi-Fi; good for Thread/Zigbee endpoints. |
| ESP32-P4 | dual-core RISC-V | none | Multimedia/peripheral processor; needs external wireless if required. |

## Variant-Specific Pain Points

- Original ESP32: GPIO6-11 flash pins; GPIO34-39 input-only; GPIO12 boot flash-voltage trap; ADC2 unavailable while Wi-Fi is active.
- ESP32-S2/S3: native USB can change how serial monitor works; flash/PSRAM pins may be hidden or reserved; GPIO45/46 have boot/input constraints.
- ESP32-C3/C6: RISC-V toolchain; native USB-Serial/JTAG common; PlatformIO Arduino support may require a current third-party platform until official support catches up.
- ESP32-H2/P4: wireless assumptions differ; H2 has no Wi-Fi, P4 has no wireless.
