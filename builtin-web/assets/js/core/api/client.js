async function api(path, options = {}) {
  const headers = Object.assign({ Accept: 'application/json' }, options.headers || {});
  if (options.body !== undefined) {
    headers['Content-Type'] = 'application/json';
  }
  if (options.auth !== false && state.token) {
    headers.Authorization = `Bearer ${state.token}`;
  }

  let response;
  try {
    response = await fetch(path, Object.assign({}, options, {
      headers,
      body: options.body === undefined ? undefined : JSON.stringify(options.body)
    }));
  } catch (cause) {
    const error = new Error(cause && cause.message ? cause.message : 'Network request failed');
    error.code = 'transport_error';
    error.status = 0;
    error.data = {};
    error.fields = [];
    throw error;
  }
  const payload = await response.json().catch(() => null);
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
  if (expectedCodes.includes(code)) return;
  console.warn(`[DeviceConsole] ${context}`, {
    code,
    status: error && Number.isFinite(error.status) ? error.status : 0
  });
}
