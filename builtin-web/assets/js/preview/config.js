(function configureSourcePreview() {
  const SOURCE_PREVIEW_DEFAULT = true;
  const existingFlags = window.DEVICE_CONSOLE_FLAGS || {};
  const queryValue = new URLSearchParams(window.location.search).get('mock');
  let mockApi = typeof existingFlags.mockApi === 'boolean'
    ? existingFlags.mockApi
    : SOURCE_PREVIEW_DEFAULT;

  if (queryValue !== null) {
    const normalizedValue = queryValue.trim().toLowerCase();
    if (['1', 'true', 'on'].includes(normalizedValue)) mockApi = true;
    if (['0', 'false', 'off'].includes(normalizedValue)) mockApi = false;
  }

  window.DEVICE_CONSOLE_FLAGS = Object.freeze(Object.assign({}, existingFlags, { mockApi }));
})();
