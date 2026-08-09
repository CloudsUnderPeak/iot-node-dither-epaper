# Static Build

Internal release tool for producing timestamped static app output under `build/`.

The build output is meant for HTTP server or device storage. It is not a `file://` standalone package.

## Requirements

- Python 3.
- No npm install, bundler, or dev server is required.

## Basic Usage

Run from the repository root:

```bash
python3 tools/build/run.py
```

This creates a new timestamped folder under `build/`, for example:

```text
build/20260628-031506/
```

If another build is created in the same second, the tool appends a numeric suffix instead of overwriting an existing folder.

## What Gets Copied

The tool copies:

- `index.html`
- `assets/`
- `src/`
- `LICENSE`

Generated `file://` demo fallback data is excluded from release output. The demo source image and `assets/demo/demo-manifest.js` stay in the build so server/device runtime can still load the built-in demo.

## Defaults

By default, the tool:

- minifies HTML, CSS, JS, and SVG files
- replaces copied files with gzip-only `.gz` files

The default output is suitable for servers or devices configured to serve pre-compressed gzip files.

## Options

Custom output root:

```bash
python3 tools/build/run.py --output output/builds
```

Remove the output root before building:

```bash
python3 tools/build/run.py --clean
```

Keep normal files instead of gzip-only files:

```bash
python3 tools/build/run.py --no-gzip
```

Skip minification:

```bash
python3 tools/build/run.py --no-minify
```

Copy files without minification or gzip replacement:

```bash
python3 tools/build/run.py --no-minify --no-gzip
```

## Related Make Targets

From the repository root:

```bash
make build
make minify
make gzip
```

`make build` uses the default minify plus gzip-only behavior. `make minify` disables gzip replacement. `make gzip` copies raw files and keeps gzip-only output.

## Verification

After a build, check the printed output folder and inspect the demo assets:

```bash
find build/<timestamp>/assets/demo -maxdepth 1 -type f -printf '%f\n' | sort
```

Expected demo release output includes the source image and `demo-manifest.js` only. It should not include `demo-data.js`.

## Notes

- Do not use the project root as `--output`; the tool rejects it.
- The tool does not transform the app into a different runtime architecture.
- The original project remains runnable without this build step.
