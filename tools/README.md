# Development tools

The `tools/` directory is organized into separate directories by function. Run all commands from the project root.

| Directory | Purpose | Entry point |
| --- | --- | --- |
| [`native-test/`](native-test/) | Validate pure C++ APIs, configuration, and route contracts on the development host | `native-test/test_native.sh` |
| [`web-test/`](web-test/) | Run source frontend lifecycle and API-boundary contracts in local headless Chrome or Edge | `web-test/run.py` |
| [`web-build/`](web-build/) | Select, process, publish, and verify builtin/user production web or the builtin demo under `build/latest/web/` | `web-build/build_web.py` |
| [`release-build/`](release-build/) | Compile current production web into firmware; snapshot, package, verify, clean, and flash latest or historical images | `release-build/build_release.py` |
| [`serial-monitor/`](serial-monitor/) | Capture ESP32 serial output for a bounded duration | `serial-monitor/monitor_serial.py` |

See the `README.md` in each function directory for requirements, options, and limitations.
