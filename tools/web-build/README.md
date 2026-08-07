# Frontend build

`build_web.py` is the only frontend output tool. It selects the tracked
`builtin-web/` console, a complete user-supplied static site under ignored
`user-web/`, or an explicit no-frontend production state; validates it; and
atomically replaces:

```text
build/latest/
├── web-manifest.json
└── web/
```

Because this is a web-only latest state, any older `binary/` and
`firmware.img` are removed. Run `make esp` afterward to create a matching full
snapshot.

## Source selection

- `WEB=auto` (default): use user web when `user-web/index.html` or
  `index.html.gz` exists; otherwise use builtin web.
- `WEB=builtin`: require and use `builtin-web/`.
- `WEB=user`: require and use `user-web/`.
- `WEB=none`: publish an empty production `web/` for API-only firmware.

A nonempty `user-web/` without an index is an error. Raw and `.gz` files that
map to the same logical URL are also rejected. User web may reference external
resources; local references must exist. The tool does not run npm, Vite,
React, or another application build. `WEB=auto` never selects `none`.

## Processing

- `WEB_PROCESS=auto` (default): builtin uses `minify-gzip`; user and none use
  `none`.
- `WEB_PROCESS=minify-gzip`: minify raw HTML/CSS/JavaScript and
  deterministically gzip every file.
- `WEB_PROCESS=none`: preserve raw or precompressed input.

Precompressed input cannot be passed to `minify-gzip`. Production builtin
output always removes the marked preview block and preview directory,
regardless of processing mode. `WEB=none` accepts only `auto` or `none`
processing and only the production target.

## Commands

```bash
make web
make web WEB=builtin
make web WEB=user
make web WEB=none
make web WEB=user WEB_PROCESS=minify-gzip
make verify-web
```

The demo always uses builtin web and retains the mock adapter. Its normal
default remains minify plus gzip; use unprocessed output for ordinary static
hosting:

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

GitHub Pages uses that same unprocessed demo command and publishes only
`build/latest/web/`.
