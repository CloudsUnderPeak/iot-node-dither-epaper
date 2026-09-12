# Compatibility fixtures

- `project-v1.dither.png` and `project-v1.json` were captured from the version 1 workspace exporter before feature persistence was moved. They contain nondefault resize/adjust/dither settings and reversed effects order. Compare stable manifest fields and recomputed pixels, not PNG compression bytes or creation timestamps.
- `color-v1.json` contains distances captured before the core ownership change, for all five metrics and three fixed RGB pairs. Runtime truth-table tests also cover every alias and invalid fallback.
- `css-semantics-v1.css` is semantic fixture version 1. It covers math whitespace, variables/nesting, combinators, comment boundaries, strings and data URLs. Browser tests compare computed values against source and concrete expectations. A parent project may retain a matching fixture for integration; this subtree does not import parent tools.

Run `make test` from this project. Hardware and target-phone measurements are separate from these fixtures.
