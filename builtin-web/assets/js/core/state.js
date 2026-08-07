const state = {
  lang: localStorage.getItem('ui.lang') || 'en',
  token: localStorage.getItem('auth.token') || '',
  activePage: 'network',
  settingsPage: 'wifi',
  lastAcceptedHash: '#/network',
  pendingHash: '',
  device: null,
  wifi: null,
  storage: null,
  auth: null,
  scanNetworks: [],
  scanDetectedCount: null,
  selectedSsid: ''
};

function setToken(token) {
  state.token = token || '';
  if (state.token) localStorage.setItem('auth.token', state.token);
  else localStorage.removeItem('auth.token');
}

function setLanguage(language) {
  state.lang = language === 'zh-Hant' ? language : 'en';
  localStorage.setItem('ui.lang', state.lang);
  window.dispatchEvent(new CustomEvent('device-console-language-change', {
    detail: { language: state.lang }
  }));
}
