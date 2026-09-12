# IOT-Node-Bedrock

[繁體中文](README.zh-TW.md)

**[Open the live demo](https://cloudsunderpeak.github.io/iot-node-bedrock/)**

A reusable Wi-Fi and device-monitoring foundation for ESP32 IoT products.

IOT-Node-Bedrock is a foundation for products that use ESP32 as an IoT node. It provides the baseline every connected device needs—AP and STA mode configuration, first-use onboarding, fallback connectivity, persistent settings, and local device monitoring—so each product can extend it with its own sensors, controls, automation, and interface.

The current development and release target is the **DFRobot FireBeetle 2 ESP32-C6**.

## Why This Project

Before an ESP32 can deliver its product-specific features, it needs a reliable way to get online, stay reachable, preserve network settings, and expose basic device status. Rebuilding those layers for every sensor, controller, gateway, or appliance wastes effort and produces inconsistent behavior.

IOT-Node-Bedrock packages those shared needs into a reusable base platform. Start with its Wi-Fi modes, local management interface, REST API, serial console, and device monitoring, then build the hardware and software features that make your IoT product unique.

## How It Feels to Use

1. The ESP32 starts its own Wi-Fi network on first boot.
2. The user connects from a phone or computer and opens the built-in interface.
3. They scan for a nearby Wi-Fi network and enter the connection details.
4. The new settings are applied immediately; if the connection fails, a fallback AP can keep the device reachable.

For everyday use, network, hardware, and storage status remain visible without signing in. Authentication is requested only when someone enters the protected settings area.

## Highlights

- **Fully local**: Complete setup without a CDN, cloud account, or internet connection.
- **No app required**: Use any modern browser on desktop or mobile.
- **Friendly first-time setup**: A default AP, captive portal, and guided Wi-Fi flow lower the barrier for new users.
- **Designed to stay reachable**: A fallback AP can preserve access when the STA connection fails or disconnects.
- **More than Wi-Fi settings**: View device status, update the hostname, manage the administrator password, and perform a factory reset.
- **Battery-ready device API**: The FireBeetle target reports onboard ADC voltage and an estimated battery percentage through `/api/device`, without guessing battery presence or charging state.
- **Persistent e-paper calibration**: Tune the six display RGB values live, save them in device NVS, and use the same calibrated palette in the dither editor.
- **Bilingual interface**: Switch instantly between English and Traditional Chinese.
- **Built for integration**: The web UI, REST API, and serial console share the same behavior for easier customization and automation.
- **Replaceable frontend**: The interface is bundled into the firmware image and can be replaced with your own branding, layout, and product features.

## Who It Is For

- Makers and product teams building ESP32 IoT devices that need reusable Wi-Fi configuration and basic monitoring.
- Projects that do not want to build a separate mobile app for device setup.
- Devices used at exhibitions, in classrooms, laboratories, or isolated networks.
- Firmware developers who want to add their own sensors, controls, or product logic on top of a reusable connectivity foundation.
- Applications managed through a REST API, AI agent, or automation tool.

## Try the Demo

The static demo uses simulated device data, so you can explore public status pages, authentication, Wi-Fi scanning, and every settings flow without an ESP32.

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

Open [http://localhost:8000/](http://localhost:8000/) and use the following credentials to enter Settings:

```text
Username: admin
Password: password
```

The repository also includes a GitHub Pages workflow. Select **GitHub Actions** under **Settings → Pages → Build and deployment → Source** to publish a shareable online demo.

## Quick Start

GNU Make, Python 3, and the PlatformIO CLI are required. Build the web interface, firmware, and flashable images with:

```bash
make build
```

Confirm the current port, then clean, rebuild, verify, and flash with one command:

```bash
pio device list
make deploy PORT=/dev/ttyACM0
```

After flashing, connect to a Wi-Fi network named like `esp32-device-XXXX`, then open the captive portal or device AP address to begin setup.

> Ports, pins, flash settings, and USB settings are board-specific. Check the official documentation for the target board before porting.

## Make It Your Own

This repository is both a complete application you can fork and a reusable ESP32 Wi-Fi foundation:

- Edit `builtin-web/` when changing this project's built-in console.
- Develop the product frontend in `user-web-project/`. The default `make build`
  (and explicit `make build WEB=user`) rebuilds that project, atomically imports
  its minified gzip-only release into ignored `user-web/`, and bundles it.
- `user-web-project/` is a Git subtree of
  [`CloudsUnderPeak/embedded-web-dithering`](https://github.com/CloudsUnderPeak/embedded-web-dithering),
  following its `six-color-epaper` branch. Configure this repository-local remote
  once per clone, then use the Make targets below to sync it without using a submodule:

  ```bash
  git remote add embedded-web-dithering https://github.com/CloudsUnderPeak/embedded-web-dithering.git
  git remote set-url --push embedded-web-dithering git@github.com:CloudsUnderPeak/embedded-web-dithering.git
  ```

  Pull before editing; commit only the intended `user-web-project/` change before
  pushing. `make user-web pull` requires a clean worktree and creates the subtree
  merge commit. `make user-web push` rejects uncommitted changes under that prefix,
  pushes only its committed subtree history, and requires GitHub SSH write access.
- Use `make build WEB=builtin` when the built-in console is required. `WEB=auto`
  remains available to consume an existing user import or fall back to builtin.
- For API-only firmware, explicitly run `make build WEB=none`. This keeps all
  REST and serial APIs but does not serve a setup page.
- Build product features on top of the existing authentication, configuration, and REST API foundation.
- Connect directly to the REST API for automated management without a browser.

Common development commands:

```bash
make build                              # Rebuild user web and a full firmware snapshot
make build WEB=builtin                  # Built-in frontend plus firmware
make build WEB=user                     # Rebuild user frontend plus firmware
make build WEB=none                     # Firmware without any frontend
make prepare-user-web                   # Rebuild/import only user-web-project
make user-web pull                      # Pull the six-color-epaper subtree branch
make user-web push                      # Push committed subtree-only changes upstream
make deploy WEB=none PORT=/dev/ttyACM0  # Build, verify, and flash API-only firmware
make web [WEB=...] [WEB_PROCESS=...]    # Process frontend only
make demo WEB_PROCESS=none              # Static mock demo under build/latest/web
make esp                                # Firmware from current production web
make verify [IMAGE=build/.../firmware.img]
make flash PORT=/dev/ttyACM0 [IMAGE=...] # Flash an existing verified snapshot
make clean                              # Remove latest and imported user web; keep snapshots
make clean all                          # Remove latest and timestamp snapshots
make test                               # Native, tools, browser tests; no firmware build
make test-native                        # C++ service and endpoint regressions
make test-tools                         # Python build/release tests
make test-all                           # Build, verify, then run all tests
make test-web                           # Run frontend browser contracts
```

Native tests require a C++17 compiler and the pinned ArduinoJson headers. Run
`pio pkg install -e firebeetle2_esp32c6` to prepare dependencies without compiling
firmware, or set `ARDUINOJSON_INCLUDE` to that pinned library’s `src` directory.
Browser tests require Chrome/Chromium/Edge; set `DEVICE_CONSOLE_BROWSER` when it
is not discovered automatically. Missing dependencies fail the test command.
PR verification runs host/browser checks and separate builtin/user/none builds;
user frontend checks use its own `test` and `test-production` targets.


`user-web/` is generated and must not be edited manually. Its nested build is
already minified and gzipped, so `WEB_PROCESS=auto` preserves it (`none`), while
the built-in frontend resolves to `minify-gzip`. Explicitly applying
`minify-gzip` to the precompressed user import is rejected. `WEB=auto` never
selects the no-frontend mode.

Every complete firmware build creates an immutable Taipei-time snapshot and
replaces `latest/` with an identical real copy:

```text
build/
├── latest/
│   ├── web-manifest.json
│   ├── web/
│   ├── binary/
│   │   ├── manifest.json
│   │   ├── bootloader.bin
│   │   ├── partitions.bin
│   │   ├── boot_app0.bin
│   │   └── firmware.bin
│   └── firmware.img
└── 20260726_0428/
    └── ...same structure as latest...
```

`firmware.img` is a deterministic tar.gz package with a custom extension. It
contains the manifest and four binary images at the archive root; there is no
separate frontend binary because selected web assets are compiled into
`firmware.bin`. A `WEB=none` snapshot keeps an empty `web/` directory and
records the no-frontend state in its manifests. Persistent project compression
is deliberately limited to gzip: frontend `.gz` assets and this gzip-compressed
TAR package.

## Documentation

Start with the [specification index](docs/SPEC_INDEX.md) for product behavior, frontend experience, REST API details, and engineering design. You do not need to understand the firmware internals just to try the project or customize its interface.

## Before You Deploy

This project is currently intended as a connectivity foundation for prototypes, controlled laboratories, and trusted local networks. It is not a hardened product ready for untrusted environments. Before a production launch, evaluate first-use authentication, HTTPS, login throttling, Secure Boot, Flash Encryption, and NVS Encryption for your threat model.
