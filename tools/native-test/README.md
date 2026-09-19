# Native test

`test_native.sh` uses the host `g++` compiler to build firmware logic that can run without an ESP32. No development board is required.
After the compiled suites pass, it runs the plain-Python source contract checks
from `contract_checks.py`.

## Test coverage

- IPv4, configuration field, and cross-field validation
- API enums, JSON payloads, response serialization, and error field paths
- HTTP JSON content type, empty/malformed body, and payload-size error contract
- User-file filename, MIME, strict size, byte Range, cursor, and console path policies
- EPDIMG header/CRC/palette streaming validation across arbitrary chunk boundaries
- Strict gzip decoding, compressed atomic storage, logical downloads and bounded image mirroring
- Framebuffer-free white/six-color packed frame generation and wrap-safe 180-second cooldown policy
- FireBeetle 2 ESP32-C6 restricted-pin policy, atomic pin claims, safe e-paper boot levels, and write-only shared SPI ownership
- 4 KiB streaming e-paper driver, initialization/refresh sequence, BUSY and operation timeouts, and protocol shutdown failure paths
- E-paper 80 MHz frequency read-back/restore, protection-marker fault injection, power-only/single-refresh probes, protocol shutdown, and coordinated-restart denial
- Typed console config staging, overlay, group/key revert, and secret replacement behavior
- Atomic ConfigService field-group updates, concurrent mutation serialization, Wi-Fi rollback merging, and storage failure isolation
- PreferencesConfigStore empty/active/fallback slots, every write/marker failure, readback mismatch, unsupported schema, and corruption
- Actual AuthService login, token rotation, logout, password/reboot lifecycle, and concurrent operations
- Actual WifiManager timeout, IPv4 readiness, disconnect, subnet overlap, commit/finalize/rollback failure, grace period, and transition blocking state paths with fake driver and clock
- FreeRTOS semaphore guard acquisition and destructor release lifecycle
- Fixed-size subsystem registry start order, startup readiness, and dynamic health lifecycle
- User-file list query, response ownership, upload/delete shape, and storage error mapping
- Actual Auth/System/Wi-Fi endpoint failure paths with fake config, auth, Wi-Fi, and runtime dependencies
- Static catalogue metadata and runtime authorization matrix for every exact and dynamic ApiRouter route
- Metadata-driven HTTP registration, including exact parent routes and dynamic user-file matcher bindings

## Requirements

- `g++`, Python 3, PlatformIO, and zlib development headers/library (`zlib1g-dev` on Debian/Ubuntu; used only to generate native gzip fixtures)
- ArduinoJson headers in `.pio/`; run a firmware build first if the dependencies have not been installed

## Usage

Run from the project root:

```bash
tools/native-test/test_native.sh
```

使用非預設 PlatformIO environment 時，傳入相同名稱以讀取對應 ArduinoJson headers：

```bash
PIO_ENV=<environment> tools/native-test/test_native.sh
```

Build artifacts are written to the ignored `tmp/native-tests/` directory. The test does not modify the firmware or product specifications.
