# IOT-Node Dither E-Paper

[繁體中文](README.zh-TW.md)

**Turn your pictures into six-color e-paper art, right from your browser.**

An ESP32-powered e-paper application that brings image editing, dithering, wireless display updates, and device management into one local web interface. Connect from your phone or computer, prepare a picture, and send it to the panel—no companion app or cloud account required.

Getting a picture onto e-paper brings together image processing, device connectivity, and panel updates. IOT-Node Dither E-Paper connects those steps in an application you can use and build on: your browser handles composition and dithering, while the ESP32 stores the image, drives the panel, and reports device status, leaving you to focus on what you want to display.

The current hardware pairing is the **DFRobot FireBeetle 2 ESP32-C6** and **Waveshare 7.3inch e-Paper HAT (E), 800 × 480, six colors**.

## From picture to paper

1. **Connect.** Join the device’s Wi-Fi and open its web interface.
2. **Create.** Import a PNG, JPEG, or WebP image, crop it, and adjust brightness, contrast, and saturation.
3. **Preview.** Apply dithering to turn your picture into the panel’s six-color palette.
4. **Display.** Choose **Draw to e-paper** to upload the result and refresh the screen.
5. **Keep editing.** Save a `.dither.png` image project to restore the original picture and editor settings later.

Use it as a personal photo display, an illustration frame, or a starting point for your own connected e-paper project.

## Device connectivity meets browser creativity

This e-paper edition builds on two projects, bringing device management and image creation into one continuous experience.

**[IOT-Node-Bedrock](https://github.com/CloudsUnderPeak/iot-node-bedrock) provides the device foundation.** This project extends it, retaining Wi-Fi setup, AP/STA modes, fallback connectivity, persistent settings, device status, and the REST API and serial console architecture. Users have a ready-made entry point for first-time connection and ongoing management; developers can add e-paper features on top of the existing connectivity capabilities.

**[Embedded Web Dithering](https://github.com/CloudsUnderPeak/embedded-web-dithering) provides the image creation interface.** This project uses its [six-color-epaper branch](https://github.com/CloudsUnderPeak/embedded-web-dithering/tree/six-color-epaper), bringing browser-based cropping, image adjustments, palette mapping, and dithering into the device web interface. In e-paper device mode, the editor matches the panel dimensions and six-color palette so the previewed image can be sent directly to the display.

**This project connects both to the physical panel.** The frontend ships with the firmware and uses the device API to send images, check drawing status, and run panel tests. The six-color calibration stored on the device feeds back into the editor for color selection and previews. From joining Wi-Fi and adjusting a picture to updating the panel, the workflow stays in one web interface.

## Why this project

- **The editor lives on the device.** The web interface ships inside the firmware. Image preparation runs in your browser, and device operation works over a local connection without internet access.
- **Made for six-color e-paper.** Device mode matches the panel’s dimensions and palette, with landscape and portrait cropping, dithering previews, and direct drawing.
- **A palette you can tune.** Adjust and save the six display RGB values on the panel test page. The editor uses that calibration for both color selection and preview.
- **Projects you can revisit.** A `.dither.png` project opens as an image in ordinary viewers and restores your editing session when imported back into the editor. Project downloads remain available while the device is offline or cooling down.
- **Wi-Fi setup is already included.** AP, STA, AP + STA, saved settings, and optional fallback AP give the display a reusable connectivity foundation.
- **Device information within reach.** Check network, storage, and hardware status, including measured battery voltage and estimated battery percentage on the target board.
- **Ready for your workflow.** Use the browser, REST API, serial console, or included Python image tool. The interface supports English and Traditional Chinese.

## Try it online

**[Open the demo](https://cloudsunderpeak.github.io/iot-node-dither-epaper/)**

## Try it locally without hardware

After downloading the project, open [`user-web-project/index.html`](user-web-project/index.html) in your browser: double-click the file, or choose **Open File** to try it through `file://`. A demo image is included, and no server is needed.

You can also start a local web server from the repository root:

```bash
python3 -m http.server 8000 --directory user-web-project
```

Open [localhost:8000](http://localhost:8000/) to try image editing and the simulated device pages. Both local preview methods use mock device data and do not operate a physical panel. The preview administrator credentials are `admin` / `password`.

The separate built-in management console also has a mock demo:

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

Open the same local address to explore the built-in management console.

## Get started

You need the target board and panel, a suitable power supply and USB data connection, plus **GNU Make, Python 3, and the PlatformIO CLI** on your computer. Confirm wiring and power requirements against the official hardware documentation before powering the panel; other boards and panel variants require a reviewed port.

Build the firmware with the e-paper product frontend:

```bash
make build
```

Find your board’s serial port, then build, verify, and flash:

```bash
pio device list
make deploy PORT=/dev/ttyACM0
```

Replace `/dev/ttyACM0` with your actual port. `make deploy` cleans and rebuilds before flashing.

On a fresh device:

1. Join `esp32-device-XXXX` from your phone or computer.
2. Open the captive portal, or visit `http://192.168.4.1/` directly.
3. Start preparing a picture, or sign in with `admin` / `password` to configure Wi-Fi and device settings.
4. Draw your picture when the panel reports it is ready.

The default build rebuilds `user-web-project/` and bundles it into the firmware; no separate web filesystem upload is needed.

## Build on it

Build your own e-paper photo frame or display from here: keep IOT-Node-Bedrock's connectivity and management capabilities, customize the Embedded Web Dithering editing interface, and connect your own image sources or automation tools through the e-paper API.

For projects centered on other sensors or controllers, start with the general-purpose [IOT-Node-Bedrock](https://github.com/CloudsUnderPeak/iot-node-bedrock) foundation. For image processing and limited-color displays, explore [Embedded Web Dithering](https://github.com/CloudsUnderPeak/embedded-web-dithering). This repository provides an application starting point that integrates both with six-color e-paper.

| What you want to do | Where to start |
| --- | --- |
| Customize the image editor and product interface | [`user-web-project/`](user-web-project/) |
| Use the built-in management console | `make build WEB=builtin` and [`builtin-web/`](builtin-web/) |
| Build firmware with REST and serial access only | `make build WEB=none` |
| Convert and send images from Python | [E-paper tool](tools/epaper/README.md) |
| Integrate device control and automation | [REST API reference](docs/SPEC_API_REFERENCE.md) |
| Work over a serial connection | [Console reference](docs/SPEC_CONSOLE_REFERENCE.md) |
| Understand builds, verification, and flashing | [Development tools](tools/README.md) and [release workflow](tools/release-build/README.md) |

Product frontend source lives in `user-web-project/`, maintained as a Git subtree for synchronization with the upstream [six-color-epaper branch](https://github.com/CloudsUnderPeak/embedded-web-dithering/tree/six-color-epaper). Make your customizations in that directory; `user-web/` and `build/` contain generated output. See the [Makefile](Makefile) for development and subtree commands, and the [specification index](docs/SPEC_INDEX.md) for architecture and behavior.

## Know before you connect

- **Refreshes are deliberately paced.** Each completed physical draw is followed by a 180-second cooldown. The interface shows remaining time; local editing can continue during the cooldown.
- **The panel target is specific.** Firmware accepts the fixed `EPDIMG` format, not raw PNG or JPEG uploads. The product editor and Python tool handle conversion for you.
- **Use a trusted local network.** The initial AP is open and the default administrator password is `password`.
