# E-paper Python tool

`epaper_tool.py` converts common images to the fixed 800×480 six-color
`EPDIMG` format and calls the public `/api/epaper/*` HTTP contract.

Install the image dependency:

```bash
python -m pip install -r tools/epaper/requirements.txt
```

Convert, upload, draw, and wait until the device enters cooldown:

```bash
python tools/epaper/epaper_tool.py --ip 192.168.4.1 --port 80 image photo.png
```

Save the exact upload payload while drawing:

```bash
python tools/epaper/epaper_tool.py --ip 192.168.4.1 image photo.jpg --output photo.epd
```

Convert without contacting a device:

```bash
python tools/epaper/epaper_tool.py convert photo.png --output photo.epd
```

Built-in device actions and status:

```bash
python tools/epaper/epaper_tool.py --ip 192.168.4.1 white
python tools/epaper/epaper_tool.py --ip 192.168.4.1 palette
python tools/epaper/epaper_tool.py --ip 192.168.4.1 refresh
python tools/epaper/epaper_tool.py --ip 192.168.4.1 status
```

EXIF orientation is corrected first. A portrait source is then automatically
rotated 90 degrees clockwise to match the 800×480 landscape panel; pass
`--no-auto-rotate` to keep it upright. The default `contain` mode preserves
the full image with a white letterbox. Use `--fit cover` to crop to the panel
or `--fit stretch` to ignore aspect ratio. Floyd–Steinberg dithering is
enabled by default; use `--dither none` for flat graphics. Transparent pixels
are composited over white.

Successful draw commands poll until `state: cooldown`; they do not wait out
the full 180-second cooldown. `--no-wait` returns immediately after the
accepted response. `--token` adds an optional Bearer token.

The firmware rejects another upload or draw while it is uploading, drawing,
or in the 180-second cooldown. The tool prints the API error code and retry
information returned by the device; it never bypasses the panel protection.
