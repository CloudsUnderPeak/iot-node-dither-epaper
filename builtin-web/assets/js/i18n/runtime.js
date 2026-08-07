function t(key) {
  return (messages[state.lang] && messages[state.lang][key]) || messages.en[key] || key;
}

function tf(key, values) {
  return Object.keys(values).reduce((text, name) => text.replace(`{${name}}`, values[name]), t(key));
}

function applyLanguage() {
  document.documentElement.lang = state.lang;
  $$('[data-i18n]').forEach((node) => { node.textContent = t(node.dataset.i18n); });
  $$('[data-i18n-placeholder]').forEach((node) => { node.placeholder = t(node.dataset.i18nPlaceholder); });
  $$('[data-i18n-title]').forEach((node) => { node.title = t(node.dataset.i18nTitle); });
  $$('[data-i18n-aria-label]').forEach((node) => { node.setAttribute('aria-label', t(node.dataset.i18nAriaLabel)); });
  $$('[data-language]').forEach((button) => button.classList.toggle('selected', button.dataset.language === state.lang));
  if (DeviceConsole.app.router) DeviceConsole.app.router.refreshUi();
  if (typeof syncWifiFormState === 'function') syncWifiFormState();
  if (typeof updateSystemSaveAvailability === 'function') updateSystemSaveAvailability();
  if (typeof renderScanResults === 'function') renderScanResults();
  if (typeof showWifiRecovery === 'function' && $('wifiRecovery') && !$('wifiRecovery').hidden) {
    showWifiRecovery($('wifiRecovery').dataset.messageKey || 'wifiReconnectHelp');
  }
}
