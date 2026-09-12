async function api(path, options = {}) {
  const headers = Object.assign({ Accept: 'application/json' }, options.headers || {});
  if (options.body !== undefined) {
    headers['Content-Type'] = 'application/json';
  }
  if (options.auth !== false && state.token) {
    headers.Authorization = `Bearer ${state.token}`;
  }

  const controller = new AbortController();
  const caller = options.signal;
  const clock = options.clock || window;
  const timeoutMs = Number.isFinite(options.timeoutMs)
    ? Math.max(0, options.timeoutMs) : DeviceConsole.utils.requests.defaults.json;
  let timer = 0;
  let stop;
  function localError(code, message) {
    return Object.assign(new Error(message), { code, status: 0, data: {}, fields: [] });
  }
  const deadline = new Promise((resolve, reject) => { stop = reject; });
  const cancel = () => {
    stop(localError('request_cancelled', 'Request cancelled'));
    controller.abort();
  };
  if (caller && caller.aborted) {
    throw localError('request_cancelled', 'Request cancelled');
  }
  if (caller) caller.addEventListener('abort', cancel, { once: true });
  timer = clock.setTimeout(() => {
    stop(localError('request_timeout', 'Request timed out; the result may be unknown'));
    controller.abort();
  }, timeoutMs);
  let response;
  let payload;
  try {
    const operation = (async () => {
      try {
        const received = await fetch(path, {
          method: options.method || 'GET',
          headers,
          body: options.body === undefined ? undefined : JSON.stringify(options.body),
          signal: controller.signal
        });
        const body = await received.json().catch(() => null);
        return { received, body };
      } catch (cause) {
        throw localError('transport_error', cause && cause.message ? cause.message : 'Network request failed');
      }
    })();
    // Race also bounds mocks/transports that ignore AbortSignal. Promise.race
    // attaches rejection handlers to late fetch/body failures.
    const result = await Promise.race([operation, deadline]);
    response = result.received;
    payload = result.body;
  } finally {
    clock.clearTimeout(timer);
    if (caller) caller.removeEventListener('abort', cancel);
  }
  if (!response.ok || !payload || payload.success !== true) {
    const message = payload && payload.message ? payload.message : `HTTP ${response.status}`;
    const error = new Error(message);
    error.data = payload && payload.data ? payload.data : {};
    error.code = error.data.code || '';
    error.fields = Array.isArray(error.data.fields) ? error.data.fields : [];
    error.status = response.status;
    throw error;
  }
  return payload.data || {};
}

function reportBackgroundError(context, error, expectedCodes = []) {
  const code = error && error.code ? error.code : '';
  if (code === 'request_cancelled' || expectedCodes.includes(code)) return;
  console.warn(`[DeviceConsole] ${context}`, {
    code,
    status: error && Number.isFinite(error.status) ? error.status : 0
  });
}

(function defineRequestPolicy(app) {
  let controller = new AbortController();
  app.utils.requests = {
    defaults: Object.freeze({ json: 10000, scan: 20000, connection: 3000 }),
    signal: () => controller.signal,
    rotate() {
      controller.abort();
      controller = new AbortController();
    },
    sleep(ms, signal) {
      return new Promise((resolve) => {
        if (signal.aborted) { resolve(); return; }
        let timer;
        const done = () => {
          window.clearTimeout(timer);
          signal.removeEventListener('abort', done);
          resolve();
        };
        timer = window.setTimeout(done, ms);
        signal.addEventListener('abort', done, { once: true });
      });
    },
    message(error) {
      return error && error.code === 'request_timeout' ? t('requestTimedOut') : error.message;
    }
  };
})(window.DeviceConsole);
