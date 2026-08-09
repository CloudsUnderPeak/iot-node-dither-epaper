# Frontend build

`build_web.py` is the only frontend output tool. It imports the tracked
`user-web-project/` release into ignored `user-web/`, selects that generated
frontend, the tracked `builtin-web/` console, or an explicit no-frontend
production state, validates it, and atomically replaces:

```text
build/latest/
├── web-manifest.json
└── web/
```

Because this is a web-only latest state, any older `binary/` and
`firmware.img` are removed. Run `make esp` afterward to create a matching full
snapshot.

## Source selection

- `WEB=user` (default from the Makefile): `make build` and
  `make build WEB=user` first rebuild `user-web-project/`, atomically import its
  gzip-only `build/latest/` into `user-web/`, then use that frontend.
- `WEB=auto`: use user web when `user-web/index.html` or
  `index.html.gz` exists; otherwise use builtin web.
- `WEB=builtin`: require and use `builtin-web/`.
- `WEB=user`: require and use `user-web/`.
- `WEB=none`: publish an empty production `web/` for API-only firmware.

`user-web/` is generated import state and must not be edited manually. A
nonempty directory without an index is an error. Raw and `.gz` files that map
to the same logical URL are also rejected. User web may reference external
resources; local references must exist. `make web WEB=user` consumes the most
recent import without rebuilding it, while `make prepare-user-web` performs
only the nested build and import. `WEB=auto` never selects `none`.

`user-web-project/` is synced separately as the `embedded-web-dithering` Git
subtree on the `six-color-epaper` branch. Run `make user-web pull` before
frontend work and `make user-web push` only after committing the intended
subtree change; see the root README for one-time remote and SSH setup. Neither
target treats `user-web/` as source or transfers generated release output.

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
make prepare-user-web
make user-web pull
make user-web push
make web
make web WEB=builtin
make web WEB=user
make web WEB=none
make web WEB=user WEB_PROCESS=none
make verify-web
```

The nested project build minifies and deterministically compresses every file,
so the imported tree must contain `index.html.gz` and gzip-only files. The main
pipeline therefore consumes it with `WEB_PROCESS=auto` (resolved to `none`) or
explicit `WEB_PROCESS=none`; `WEB_PROCESS=minify-gzip` rejects this
precompressed input. `make clean` removes imported `user-web/` content while
preserving its tracked `.gitignore`, but does not remove timestamped output
under `user-web-project/build/`.

The demo always uses builtin web and retains the mock adapter. Its normal
default remains minify plus gzip; use unprocessed output for ordinary static
hosting:

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

GitHub Pages uses that same unprocessed demo command and publishes only
`build/latest/web/`.
