"""Read the independent user encoder; validate its actual bytes with firmware C++."""
import base64
import re
import subprocess
from pathlib import Path


def build_harness(root: Path, output: Path) -> Path:
    encoder = (root / 'user-web-project/src/core/encoders/epdimg-encoder.js').read_text()
    harness = output / 'epdimg-consumer.html'
    harness.write_text('''<!doctype html><meta charset="utf-8"><pre id="result">RUNNING</pre>
<pre id="payload"></pre><script>window.DitherApp={core:{}};</script><script>'''
        + encoder + '''</script><script>
try {
  const image = new ImageData(800,480);
  const colors = [[0,0,0],[255,255,255],[255,255,0],[255,0,0],[0,0,255],[0,255,0]];
  for(let pixel=0; pixel<800*480; pixel++) image.data.set([...colors[pixel%6],255],pixel*4);
  const encoded = DitherApp.core.epdimgEncoder.encode(image).payload;
  let binary=''; for(const byte of encoded) binary += String.fromCharCode(byte);
  document.getElementById('payload').textContent=btoa(binary);
  document.getElementById('result').textContent='PASS';
} catch(error) { document.getElementById('result').textContent=error.stack; }
</script>''')
    return harness


def validate_bytes(root: Path, output: Path, dom: str) -> None:
    match = re.search(r'<pre id="payload">([A-Za-z0-9+/=]+)</pre>', dom)
    if not match:
        raise ValueError('User encoder produced no contract payload')
    payload = output / 'user-encoded.epd'
    payload.write_bytes(base64.b64decode(match.group(1), validate=True))
    binary = output / 'epdimg-consumer-contract'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'src'),
                    str(root / 'tests/native/EpaperConsumerContractTest.cpp'),
                    str(root / 'src/modules/epaper/EpaperImageFormat.cpp'), '-o', str(binary)],
                   check=True, timeout=60)
    subprocess.run([str(binary), str(payload)], check=True, timeout=30)
