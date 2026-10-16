# Static Build

Internal release tool for producing timestamped static app output under `build/`.

The build output is meant for HTTP server or device storage. Gzip-only output needs an HTTP server; non-gzip output (`--no-gzip`, `--demo`) also opens with `file://`, except Load Demo, whose `file://` fallback data is excluded from builds.

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

## Bundling

Every build bundles the output so startup needs only a few requests, reducing cold-load overhead independently of the HTTP server cache policy:

- `bundle/app.css`: the stylesheets `index.html` links, in order.
- `bundle/app.js`: the deferred scripts `index.html` loads, in order, plus a `DitherApp.bundle` registry holding every other `src/` module (wrapped in a function) and the `assets/icons/` SVGs as data URLs. `script-loader.js` runs registry modules in the same order the page `entry.js` files request them.
- `bundle/help.js`: the Help page modules, loaded the first time Help is opened or preloaded.
- The worker keeps its path with its `importScripts(...)` dependencies inlined.

Merged source files are removed from the output. The build fails if an `index.html` script is not deferred, a bundled module is not a single IIFE, or a merged file is still referenced directly. Source files are never changed; opening the source `index.html` still loads one classic script per module.

## Defaults

By default, the tool:

- minifies HTML, CSS, JS, and SVG files
- bundles startup scripts, styles, and icons
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
- Bundling only changes the output; the source keeps its classic-script architecture.
- The original project remains runnable without this build step.
