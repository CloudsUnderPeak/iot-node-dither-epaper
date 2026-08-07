#!/usr/bin/env sh
set -eu

if [ -d ".deps/custom_ensurepip" ]; then
    export PYTHONPATH="${PWD}/.deps/custom_ensurepip${PYTHONPATH:+:${PYTHONPATH}}"
fi

if [ -d ".deps/python-wheels/usr/share/python-wheels" ]; then
    export LOCAL_PYTHON_WHEELS="${PWD}/.deps/python-wheels/usr/share/python-wheels"
fi

if command -v pio >/dev/null 2>&1; then
    exec pio "$@"
fi

if [ -x "$HOME/.local/bin/pio" ]; then
    exec "$HOME/.local/bin/pio" "$@"
fi

echo "PlatformIO CLI was not found. Install PlatformIO or add pio to PATH." >&2
exit 127
