# Pinned transport acceptance

The native streaming bridge and weak-slot suites do not run the ESPAsyncWebServer
3.11.1 request implementation. Keep this separate acceptance check for an
explicitly authorized device or a transport environment running that library.
Do not substitute source checks or fake request objects for these results.

For static asset cache acceptance, use a freshly built frontend and the pinned
device server. Confirm each asset's 200 body matches the build, has a quoted
web-content/encoding ETag and `Cache-Control: private, no-cache`, and returns a
bodyless 304 for a matching strong/weak/list/wildcard If-None-Match. A 304 must
retain ETag, Cache-Control and gzip Vary; any Content-Length describes the
corresponding 200, not a nonexistent 304 body. Stale tags (including the previous
web build), a different encoding, and malformed/oversized fields must return
the full current representation. Unknown assets and JSON APIs must never enter
asset 304 handling. Compare real browser cold/warm loads and HTTP wire statuses;
after a restart the same web content must retain its validator.

With the exact firmware/source revision, library versions and client captured
under `tmp/verification/`, exercise:

1. Start authenticated `GET /api/wifi/scan`; close the client socket before the
   scan ends. Query public alive/runtime concurrently. Confirm one terminal
   operation, no response to a destroyed request, and no leaked response slot.
2. Repeat with a second concurrent scan (409), token rotation (401 on completion),
   normal completion (200), timeout (500) and a disconnect at completion. The
   next scan after cooldown must work if the driver acknowledged cleanup.
3. Interrupt raw generic and gzip EPDIMG uploads during a chunk and after the final
   chunk. Verify the previous committed file and admission/session reuse.
4. Interrupt full and ranged downloads during a filler callback; check handle
   release. Complete zero-byte generic downloads without relying on disconnect.
5. Record scan-time alive/runtime latency p50/p95/max, errors and heap under the
   same before/after workload. Inspect serial command completion and bounded
   input/overflow while a scan is pending.

Use controlled stop/ACK failure injection only where the transport environment
supports it. Unknown radio state must stay unavailable, and restart timeout must
not force a reset. Panel/restart tests additionally require the project's normal
hardware authorization and safety procedure; never shorten cooldown for testing.

For e-paper gzip acceptance, exercise the pinned HTTP parser with missing/invalid
Content-Length, Transfer-Encoding, missing/unsupported Content-Encoding,
compressed over-limit bodies, optional headers split across callbacks, corrupt
CRC/ISIZE, concatenated members, disconnect and storage failures. Confirm the
prior gzip remains byte-identical and no failed request queues a draw. Full and
late logical Range downloads must match the source raw EPDIMG, including response
length/range headers; corruption after response start must terminate as a short
fixed-length transfer. Run all four mounting flip combinations with asymmetric
corner markers, keeping cooldown intact. Capture bounded heartbeat timings and
peak heap for upload validation, pre-draw validation, frame read, transfer and
total operation; vertical flip repeats inflate per bounded row band. Validate
both the default profile and any replacement panel's real controller, capacity
and watchdog budget before declaring hardware support.
