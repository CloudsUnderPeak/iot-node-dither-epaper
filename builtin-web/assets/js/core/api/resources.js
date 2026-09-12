const resources = {
  device: {
    get: (options = {}) => api('/api/device', { ...options, auth: false })
  },
  storage: {
    get: (options = {}) => api('/api/storage', { ...options, auth: false })
  },
  wifi: {
    get: (options = {}) => api('/api/wifi', { ...options, auth: false }),
    scan: (options = {}) => api('/api/wifi/scan', { timeoutMs: DeviceConsole.utils.requests.defaults.scan, ...options }),
    connectionStatus: (options = {}) => api('/api/wifi/connect', {
      timeoutMs: DeviceConsole.utils.requests.defaults.connection, ...options
    }),
    update: (body, options = {}) => api('/api/wifi', { ...options, method: 'PUT', body })
  },
  auth: {
    get: (options = {}) => api('/api/auth', { ...options, auth: false }),
    login: (body, options = {}) => api('/api/auth/login', { ...options, method: 'POST', auth: false, body }),
    session: (options = {}) => api('/api/auth/session', options),
    logout: (options = {}) => api('/api/auth/logout', { ...options, method: 'POST' }),
    changePassword: (password, options = {}) => api('/api/auth/password', { ...options, method: 'PUT', body: { password } })
  },
  system: {
    update: (hostname, options = {}) => api('/api/system', { ...options, method: 'PUT', body: { hostname } }),
    reset: (options = {}) => api('/api/system/reset', { ...options, method: 'POST', body: {} }),
    resetSettings: (options = {}) => api('/api/system/reset/settings', { ...options, method: 'POST', body: {} }),
    resetData: (options = {}) => api('/api/system/reset/data', { ...options, method: 'POST', body: {} })
  }
};
