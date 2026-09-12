"""Browser fixture prepared from newly built production gzip bytes."""
import gzip
import importlib.util
import json
from pathlib import Path
from unittest import mock


def build_css_harness(root: Path, output: Path) -> Path:
    spec = importlib.util.spec_from_file_location("css_web_build", root / "tools/web-build/build_web.py")
    web = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(web)
    directory = output / "css-production"
    latest = directory / "build/latest"
    with mock.patch.multiple(web, BUILD_ROOT=directory / "build", LATEST_ROOT=latest,
                             WEB_OUTPUT=latest / "web", WEB_MANIFEST=latest / "web-manifest.json"):
        web.build_web("builtin", "minify-gzip", "production")
    fixture = json.loads((root / "tests/tools/fixtures/css-semantics-v1.json").read_text())
    fixtures = directory / "fixture"
    fixtures.mkdir(parents=True, exist_ok=True)
    (fixtures / "fixture.css").write_text(fixture["css"])
    shared_css = (root / "tests/tools/fixtures/css-semantics-v1.css").read_text()
    (fixtures / "shared.css").write_text(shared_css)
    # Each run consumes freshly transformed bytes, never an old snapshot.
    for previous in fixtures.glob("*.gz"):
        previous.unlink()
    web.minify_files(fixtures)
    web.gzip_output_files(fixtures)
    processed = gzip.decompress((fixtures / "fixture.css.gz").read_bytes()).decode()
    source_app = (root / "builtin-web/assets/css/app.css").read_text()
    production_app = gzip.decompress((latest / "web/assets/css/app.css.gz").read_bytes()).decode()
    data = dict(fixture, processed=processed, sourceApp=source_app, productionApp=production_app)
    data.update(sharedCss=shared_css, sharedProcessed=gzip.decompress((fixtures / "shared.css.gz").read_bytes()).decode())
    payload = json.dumps(data).replace("<", "\\u003c")
    script = (root / "tests/web/CssSemanticsTest.js").read_text()
    harness = directory / "test.html"
    harness.write_text('<!doctype html><meta charset="utf-8"><pre id="result">RUNNING</pre>'
                       + '<script>const fixture=' + payload + ';\n' + script + '</script>')
    return harness
