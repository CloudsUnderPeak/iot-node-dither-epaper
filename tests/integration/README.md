# Pinned transport acceptance

The native streaming bridge and weak-slot suites do not run the ESPAsyncWebServer
3.11.1 request implementation. Keep this separate acceptance check for an
explicitly authorized device or a transport environment running that library.
Do not substitute source checks or fake request objects for these results.

With the exact firmware/source revision, library versions and client captured
under `tmp/verification/`, exercise:

1. Start authenticated `GET /api/wifi/scan`; close the client socket before the
   scan ends. Query public alive/runtime concurrently. Confirm one terminal
   operation, no response to a destroyed request, and no leaked response slot.
2. Repeat with a second concurrent scan (409), token rotation (401 on completion),
   normal completion (200), timeout (500) and a disconnect at completion. The
   next scan after cooldown must work if the driver acknowledged cleanup.
3. Interrupt raw generic and EPDIMG uploads during a chunk and after the final
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
