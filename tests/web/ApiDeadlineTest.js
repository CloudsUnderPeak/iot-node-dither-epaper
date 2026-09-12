(async () => {
  const assert = (value, message) => { if (!value) throw new Error(message); };
  const flush = async () => { for (let i = 0; i < 12; ++i) await Promise.resolve(); };
  class Clock {
    now = 0;
    next = 1;
    timers = new Map();
    setTimeout = (callback, delay) => {
      const id = this.next++;
      this.timers.set(id, { callback, due: this.now + delay });
      return id;
    };
    clearTimeout = id => this.timers.delete(id);
    advance(ms) {
      this.now += ms;
      for (const [id, timer] of [...this.timers]) {
        if (timer.due <= this.now) { this.timers.delete(id); timer.callback(); }
      }
    }
  }
  try {
    for (const stage of ['fetch', 'body']) {
      const clock = new Clock();
      let calls = 0;
      let rejectLate;
      const never = new Promise((resolve, reject) => { rejectLate = reject; });
      window.fetch = async (path, options) => {
        ++calls;
        assert(!('timeoutMs' in options) && !('auth' in options) && !('clock' in options), 'custom option leaked');
        return stage === 'fetch' ? never : { ok: true, status: 200, json: () => never };
      };
      const controller = new AbortController();
      let listeners = 0;
      const signal = {
        get aborted() { return controller.signal.aborted; },
        addEventListener(...args) { ++listeners; controller.signal.addEventListener(...args); },
        removeEventListener(...args) { --listeners; controller.signal.removeEventListener(...args); }
      };
      const pending = api('/test', { method: 'PUT', body: {}, timeoutMs: 10, clock, signal }).catch(error => error);
      await flush();
      clock.advance(10);
      assert((await pending).code === 'request_timeout', `${stage} must time out`);
      assert(clock.timers.size === 0 && listeners === 0, 'timeout cleanup leaked');
      rejectLate(new Error('late failure'));
      await flush();
      assert(calls === 1, 'mutation must not retry');
    }
    for (const status of [200, 401]) {
      const clock = new Clock();
      let listeners = 0;
      const signal = { aborted: false, addEventListener() { ++listeners; }, removeEventListener() { --listeners; } };
      window.fetch = async () => ({ ok: status === 200, status,
        json: async () => ({ success: status === 200, data: { code: 'unauthorized' } }) });
      await api('/test', { clock, signal }).catch(error => error);
      assert(listeners === 0 && clock.timers.size === 0, 'success/API error cleanup leaked');
    }
    for (const preAborted of [true, false]) {
      const clock = new Clock();
      const controller = new AbortController();
      let calls = 0;
      window.fetch = () => { ++calls; return new Promise(() => {}); };
      if (preAborted) controller.abort();
      const pending = api('/test', { signal: controller.signal, clock }).catch(error => error);
      if (!preAborted) controller.abort();
      assert((await pending).code === 'request_cancelled', 'caller cancellation code');
      assert(clock.timers.size === 0, 'cancel timer leaked');
      assert(calls === (preAborted ? 0 : 1), 'pre-aborted request must not fetch');
    }
    {
      const clock = new Clock();
      const originalTimeout = window.setTimeout;
      const originalClear = window.clearTimeout;
      const originalNow = Date.now;
      const originalFailure = finishWifiTransitionFailure;
      window.setTimeout = clock.setTimeout;
      window.clearTimeout = clock.clearTimeout;
      Date.now = () => clock.now;
      let terminal = 0;
      let completed = false;
      finishWifiTransitionFailure = () => { ++terminal; };
      window.fetch = () => new Promise(() => {});
      wifiStatusPollGeneration = 42;
      const pending = pollWifiTransition({ payload: {}, submittedFormValue: '{}', generation: 42,
        apChanged: false, deadline: 25000 }).then(() => { completed = true; });
      for (let i = 0; i < 30 && !completed; ++i) {
        await flush();
        if (clock.timers.size) {
          const next = Math.min(...[...clock.timers.values()].map(timer => timer.due));
          clock.advance(next - clock.now);
        }
      }
      await pending;
      assert(clock.now === 25000 && terminal === 1 && clock.timers.size === 0, 'transition exceeded total deadline');
      window.setTimeout = originalTimeout;
      window.clearTimeout = originalClear;
      Date.now = originalNow;
      finishWifiTransitionFailure = originalFailure;
    }
    document.getElementById('result').textContent = 'PASS';
  } catch (error) { document.getElementById('result').textContent = error.stack || String(error); }
})();
