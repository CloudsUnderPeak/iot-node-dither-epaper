# E-paper Python tool

`epaper_tool.py` converts common images to the six-color `EPDIMG` format using
the device panel capability and calls the public `/api/epaper/*` HTTP contract.

Install the image dependency:

```bash
python -m pip install -r tools/epaper/requirements.txt
```

Convert, upload, draw, and wait until the device enters cooldown:

```bash
python tools/epaper/epaper_tool.py --ip 192.168.4.1 --port 80 image photo.png
```

Save the logical, uncompressed EPDIMG while uploading gzip and drawing:

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

Monitor cooldown cleanup and wait for automatic recovery without sending any
write, draw, restart, or power-cycle command:

```bash
python tools/epaper/epaper_recovery_monitor.py --ip 192.168.4.1 --interval 5 --log epaper-recovery.log
```

The monitor exits successfully when the device reports `idle` and
`can_draw: true`. It exits with code 3 when the device reports a non-retryable
`unavailable` condition such as brownout or `full_power_cycle` recovery.
`marker_clear_failed` is the retryable exception. Requests are at least five
seconds apart; the default 600-second deadline exits with code 4 if recovery
is not confirmed, including when requests keep failing. `--duration 0` opts
into unlimited observation. The script observes firmware recovery; it does
not itself repair or unlock the panel.

EXIF orientation is corrected first. A portrait source is then automatically
rotated 90 degrees clockwise to match a landscape panel; pass
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

Connected `image` reads and validates `/api/epaper` before conversion. Offline
`convert` and `image --output-only` default explicitly to 800×480; select another
geometry with paired `--width` and `--height` (positive, even width, each at most
4096). In connected mode an explicit pair must match the capability exactly:

```bash
python tools/epaper/epaper_tool.py convert photo.png --width 1600 --height 1200 --output large.epd
```

The `.epd` output remains raw EPDIMG v1 (40-byte header + width×height/2 frame).
Upload validates the capability and logical header/CRC, compresses with gzip
and fixed mtime, and sends `Content-Encoding: gzip` with the compressed
Content-Length. Firmware stores gzip and serves logical raw downloads. Old raw
upload clients receive HTTP 415; the tool never falls back to raw. Hardware
mounting flips are firmware draw parameters and must not be applied again here.
