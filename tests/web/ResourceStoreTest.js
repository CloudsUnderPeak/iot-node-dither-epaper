(async function runResourceStoreTests() {
  function assert(condition, message) {
    if (!condition) throw new Error(message);
  }

  const first = loadResourceSnapshot('device');
  const second = loadResourceSnapshot('device');
  deviceRequests[1]({ hostname: 'newer' });
  const secondResult = await second;
  deviceRequests[0]({ hostname: 'older' });
  const firstResult = await first;

  assert(secondResult.committed, 'the newest request must commit');
  assert(!firstResult.committed, 'an older request must be rejected as stale');
  assert(state.device.hostname === 'newer', 'a stale response must not replace the snapshot');

  const pending = loadResourceSnapshot('device');
  mergeResourceSnapshot('device', { hostname: 'local-commit' });
  deviceRequests[2]({ hostname: 'late-response' });
  const pendingResult = await pending;

  assert(!pendingResult.committed, 'a local commit must invalidate an in-flight request');
  assert(state.device.hostname === 'local-commit', 'an invalidated response must preserve the local commit');
  document.getElementById('result').textContent = 'PASS';
})().catch((error) => {
  document.getElementById('result').textContent = `FAIL: ${error.message}`;
});
