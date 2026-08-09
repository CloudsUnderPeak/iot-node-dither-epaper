# Generate Demo Data

Internal tool for generating the built-in demo metadata and `file://` fallback data.

Server and GitHub Pages load the selected demo source image directly. The generated data fallback is only needed when opening `index.html` directly with `file://` and using Load Demo.

## Requirements

- Python 3.
- No dev server is required.

## Demo Source Rule

`assets/demo/` must contain exactly one supported source image at the directory root.

Supported extensions:

- `.png`
- `.jpg`
- `.jpeg`
- `.webp`

The match is case-insensitive. The demo image can have any filename and any aspect ratio.

The tool ignores subdirectories, hidden files, non-image files, and generated data files. If no supported image is found, or if multiple supported images are found, the tool exits with an error instead of choosing silently.

## Basic Usage

Run from the repository root after replacing or renaming the demo image:

```bash
python3 tools/generate-demo-data/run.py
```

The tool prints the generated files and selected source image.

## Outputs

The tool writes:

- `assets/demo/demo-manifest.js`
- `assets/demo/demo-data.js`

`demo-manifest.js` records the selected source filename, source URL, and data fallback script path.

`demo-data.js` embeds the selected source image as a data URL for direct `file://` Load Demo fallback.

## Runtime Behavior

Load Demo uses these files in this order:

1. Load `assets/demo/demo-manifest.js`.
2. Try to load the selected source image directly.
3. If direct image pixel loading fails, load `assets/demo/demo-data.js` and use its data URL fallback.

## Replacing The Demo Image

1. Remove the old demo source image from `assets/demo/`.
2. Add exactly one supported image file to `assets/demo/`.
3. Run:

```bash
python3 tools/generate-demo-data/run.py
```

The source image does not need to be named `demo-16x9.png` and does not need to use a 16:9 aspect ratio.

## Verification

Confirm the current selected source:

```bash
python3 - <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'tools/shared')
from demo_assets import discover_demo_source
print(discover_demo_source(Path('.')).as_posix())
PY
```

Check generated metadata:

```bash
sed -n '1,12p' assets/demo/demo-manifest.js
```

## Notes

- Do not edit generated manifest or data files by hand.
- Run this tool again after changing, renaming, adding, or removing demo source images.
- Server/device build output excludes `demo-data.js` because it is only for `file://` fallback.
