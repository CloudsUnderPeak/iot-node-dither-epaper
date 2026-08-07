# ESP32 release build

`build_release.py` consumes an already verified production
`build/latest/web/`, compiles selected raw or gzip assets into `firmware.bin`,
and creates a complete immutable snapshot. An explicit `WEB=none` input
generates a safe zero-count asset table instead.

```text
build/
├── latest/
│   ├── web-manifest.json
│   ├── web/
│   ├── binary/
│   │   ├── manifest.json
│   │   ├── bootloader.bin
│   │   ├── partitions.bin
│   │   ├── boot_app0.bin
│   │   └── firmware.bin
│   └── firmware.img
└── YYYYMMDD_HHMM/
    └── ...same structure as latest...
```

Timestamps use Asia/Taipei. Builds in the same minute receive `_02`, `_03`,
and so on. `latest/` is a real, byte-identical copy rather than a symlink.

`firmware.img` is a deterministic tar.gz file with a custom extension. Its
USTAR root contains `manifest.json` and the four binary images in a fixed
order, with deterministic member metadata. It does not contain a `binary/`
directory, frontend filesystem image, user partition image, outer archive,
flasher, README, signature, or padded factory image.

For `WEB=none`, the snapshot retains an empty `web/` directory so every
snapshot has the same shape. Both manifests record source/process/delivery as
`none`, zero files, zero frontend payload, and the empty-tree hash used for
artifact verification. The runtime `/api/web` response intentionally reports
`sha256: null` because no Web bundle exists.

Persistent project compression is limited to gzip. Frontend precompression
uses `.gz`, while firmware packaging uses gzip around a TAR container. Do not
add ZIP, Brotli, zstd, xz, bzip2, or another outer compressed package.

## Commands

```bash
make esp
make build WEB=none
make deploy WEB=none PORT=/dev/ttyACM0
make verify
make verify IMAGE=build/20260726_0428/firmware.img
make flash PORT=/dev/ttyACM0
make flash PORT=/dev/ttyACM0 IMAGE=build/20260726_0428/firmware.img
```

`make esp` rejects demo web. Verification is workspace-coupled: the selected
`firmware.img` must remain beside its `binary/`, `web/`, and
`web-manifest.json`. It verifies image hashes and offsets, protected user
partition ranges, web/firmware metadata, flat archive contents, and package
bytes. Flashing then uses only the verified sibling binaries, never `.pio/`
temporary images.

```bash
make clean      # remove latest/transient output, preserve timestamp snapshots
make clean all  # also remove all timestamp snapshots
```

The root `VERSION` contains `0.8.0`; manifests use that same firmware/web
version and display releases as `v0.8.0`. Local dirty builds are allowed and
recorded. CI builds reject a dirty worktree.
