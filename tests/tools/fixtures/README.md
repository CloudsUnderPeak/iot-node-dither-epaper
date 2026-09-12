# Main-project regression fixtures

`css-semantics-v1.css` is the shared semantic fixture, also stored independently
in `user-web-project/tests/fixtures/`. The integration test compares its checksum;
each build tool transforms its own copy. Browser checks consume newly generated
minify/gzip output and compare CSSOM and concrete computed dimensions.
`css-semantics-v1.json` adds builtin-specific markup and expected values.

The browser integration also executes the user EPDIMG encoder on a deterministic
six-color raster. `EpaperConsumerContractTest.cpp` validates those actual bytes
with the firmware StreamingValidator, checking packed palette order, dimensions,
generation, CRC and invalid variants. Generation is intentionally random and
must be nonzero; there is no new file format or compatibility wrapper.
