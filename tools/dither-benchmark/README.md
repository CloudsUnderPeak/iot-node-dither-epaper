# Dither Benchmark

Internal benchmark tool for measuring Dither algorithms with the project runtime JavaScript.

The tool has two entry points:

- `index.html`: browser UI for manual inspection.
- `run.py`: headless runner for repeatable before/after benchmark numbers.

## Requirements

- Python 3.
- Chrome, Chromium, or Edge.
- No dev server is required.

`run.py` finds a browser in this order:

1. `--chrome <browser-path>`
2. `DITHER_BENCHMARK_BROWSER`
3. Browser command available in `PATH`
4. Windows App Paths registry lookup

## Manual UI

Open `tools/dither-benchmark/index.html` in a browser.

Use the controls to select:

- image type
- width / height
- iterations / warm-up
- palette
- palette mapping

Click `Run Benchmark` to render results in the table.

## Headless Runner

Run from the repository root:

```bash
python3 tools/dither-benchmark/run.py
```

Typical focused benchmark:

```bash
python3 tools/dither-benchmark/run.py \
  --width 800 \
  --height 480 \
  --iterations 5 \
  --warmup 1 \
  --mapping nearest-color \
  --algorithm bayer-8 \
  --backend auto
```

Raw JSON output:

```bash
python3 tools/dither-benchmark/run.py --algorithm bayer-8 --json
```

## Backend Modes

`--backend auto`

Uses GPU fast paths when available, otherwise falls back to CPU.

`--backend cpu`

Forces CPU paths. Use this for baseline numbers.

`--backend gpu`

Forces GPU paths. If the algorithm or mapping is not supported by GPU, the run reports an error instead of silently falling back.

## Useful Comparisons

CPU baseline:

```bash
python3 tools/dither-benchmark/run.py \
  --width 800 \
  --height 480 \
  --iterations 5 \
  --warmup 1 \
  --mapping nearest-color \
  --algorithm bayer-8 \
  --backend cpu
```

GPU / auto result:

```bash
python3 tools/dither-benchmark/run.py \
  --width 800 \
  --height 480 \
  --iterations 5 \
  --warmup 1 \
  --mapping nearest-color \
  --algorithm bayer-8 \
  --backend auto
```

Compare `Avg ms`, `Pixels/ms`, `Backend`, and `Checksum`.

## Result Columns

- `Algorithm`: dither algorithm id.
- `Mapping`: palette mapping mode.
- `Avg ms`: average runtime across measured runs.
- `Min ms` / `Max ms`: fastest and slowest measured run.
- `Runs`: measured iteration count, excluding warm-up.
- `Pixels/ms`: processed pixels per millisecond; higher is faster.
- `Backend`: `cpu` or `gpu` for the last measured run.
- `Checksum`: output image checksum. CPU/GPU comparisons should match before treating a speedup as valid.

## Supported Options

Images:

- `gradient`
- `noise`
- `bands`

Palettes:

- `e6`
- `gameboy`
- `sixteen`

Palette mappings:

- `nearest-color`
- `pair-mix`
- `tri-mix`
- `all`

Backends:

- `auto`
- `cpu`
- `gpu`

Algorithms use the same ids registered by `dither-algorithm-registry.js`, such as:

- `bayer-8`
- `blue-noise-64`
- `pattern-dots`
- `floyd-steinberg`

## Notes

- The benchmark loads project scripts directly from `src/`; it is not a separate implementation.
- Warm-up is important for GPU tests because shader/context setup can dominate the first run.
- GPU fast paths currently target threshold-style algorithms with `nearest-color`; other mappings may intentionally remain on CPU.
- The runner sanitizes browser error text so local file paths are not printed in benchmark errors.
