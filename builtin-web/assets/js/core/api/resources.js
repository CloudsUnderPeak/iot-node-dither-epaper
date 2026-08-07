const resources = {
  device: {
    get: () => api('/api/device', { auth: false })
  },
  storage: {
    get: () => api('/api/storage', { auth: false })
  },
  wifi: {
    get: () => api('/api/wifi', { auth: false }),
    scan: () => api('/api/wifi/scan'),
    connectionStatus: () => api('/api/wifi/connect'),
    update: (body) => api('/api/wifi', { method: 'PUT', body })
  },
  auth: {
    get: () => api('/api/auth', { auth: false }),
    login: (body) => api('/api/auth/login', { method: 'POST', auth: false, body }),
    session: () => api('/api/auth/session'),
    logout: () => api('/api/auth/logout', { method: 'POST' }),
    changePassword: (password) => api('/api/auth/password', { method: 'PUT', body: { password } })
  },
  system: {
    update: (hostname) => api('/api/system', { method: 'PUT', body: { hostname } }),
    reset: () => api('/api/system/reset', { method: 'POST', body: {} }),
    resetSettings: () => api('/api/system/reset/settings', { method: 'POST', body: {} }),
    resetData: () => api('/api/system/reset/data', { method: 'POST', body: {} })
  }
};
