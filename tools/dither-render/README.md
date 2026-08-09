# Dither Render

Internal image generation tool for rendering PNG files with this project's dither implementation.

The tool loads the same project scripts used by the app. It is not a separate dither implementation.

## Requirements

- Python 3.
- Chrome, Chromium, or Edge.
- No dev server is required.

`run.py` finds a browser in this order:

1. `--chrome <browser-path>`
2. `DITHER_RENDER_BROWSER`
3. Browser command available in `PATH`
4. Windows App Paths registry lookup

## Basic Usage

Run from the repository root:

```bash
python3 tools/dither-render/run.py --output output/dither.png
```

This renders a synthetic gradient source with the default settings.

## Render An Input Image

```bash
python3 tools/dither-render/run.py \
  --input samples/source.png \
  --output output/source-dithered.png \
  --algorithm bayer-8 \
  --mapping nearest-color \
  --palette e6
```

## Synthetic Sources

When `--input` is omitted, the tool generates a synthetic source image.

```bash
python3 tools/dither-render/run.py \
  --source noise \
  --width 800 \
  --height 480 \
  --output output/noise.png
```

Available sources:

- `gradient`
- `noise`
- `bands`

## Common Options

Algorithms use ids from `dither-algorithm-registry.js`, for example:

- `bayer-8`
- `blue-noise-64`
- `pattern-dots`
- `floyd-steinberg`

Palette mappings:

- `nearest-color`
- `pair-mix`
- `tri-mix`

Palettes:

- `e6`
- `gameboy`
- `sixteen`

Backends:

- `auto`
- `cpu`
- `gpu`

`--error-strength` uses the same shared strength value as the app: Error Diffusion and Dot Diffusion treat it as error strength, Bayer and Blue Noise treat it as dither strength for `thresholdScale` and Palette Mapping cutoff spread, and Dot Halftone treats it as clustered-dot density.

## Custom Palette

Use `--colors` for a comma-separated hex palette:

```bash
python3 tools/dither-render/run.py \
  --colors "#000000,#ffffff,#ff0000,#ffff00" \
  --algorithm blue-noise-64 \
  --output output/custom-palette.png
```

## Metadata

Use `--json` to print metadata:

```bash
python3 tools/dither-render/run.py --output output/dither.png --json
```

The metadata includes:

- output size
- algorithm
- palette mapping
- palette preset
- backend
- render duration
- checksum

## Notes

- The output is always PNG.
- Input image size is preserved.
- Synthetic source dimensions use `--width` and `--height`.
- `--backend gpu` is useful for checking whether an algorithm can use the GPU path.
- Browser error text is sanitized so local file paths are not printed in render errors.
