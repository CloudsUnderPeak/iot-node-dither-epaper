# Headless frontend tests

`run.py` loads the actual classic-script frontend in a local headless Chrome or
Edge. It verifies the existing ResourceStore race test, formal/mock API client
boundaries, login/session/password flows, dialog keyboard lifecycle, and Wi-Fi
safe-transition stale-response protection.

Run through the project entry:

```bash
make test-web
```

The runner discovers Chrome or Edge from the command path and common Windows
locations under WSL. An explicit browser can be supplied with
`DEVICE_CONSOLE_BROWSER=/path/to/browser` or `--browser`.

Temporary harness and browser-profile files are written under ignored
`tmp/web-tests/`.
