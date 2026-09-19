# Streaming DEFLATE dependency

`miniz_tinfl.c`, `miniz_tinfl.h`, `miniz_common.h` and `LICENSE` originate from
https://github.com/richgel999/miniz/tree/3.0.2 (MIT license).
The vendor changes are two marked checks in `miniz_tinfl.c` rejecting reserved
DEFLATE length symbols 286/287 and distance symbols 30/31, plus renaming the
exported decoder to `epaper_tinfl_decompress`. ESP32-C6 ROM
exports an older, ABI-incompatible `tinfl_decompress` state layout at the same
global symbol name, so using the upstream name silently corrupts dynamic
DEFLATE output on device.
The gzip wrapper uses non-wrapping output validation until the first 32 KiB
dictionary fills, so a backreference cannot read nonexistent initial history.
`miniz.h` and `miniz_export.h` are project-owned subset configuration.
Only raw streaming inflate is used. No compressor, archive API or remote runtime
dependency is included. Native tests and firmware compile the same decoder;
this deliberately avoids depending on a different ESP ROM implementation.
