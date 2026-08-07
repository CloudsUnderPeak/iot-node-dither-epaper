const MIN_VISIBLE_RSSI = -75;
let wifiFormBaseline = '';
let wifiFormBusy = false;
let wifiBusyMessageKey = 'savingWifi';

function createScanDialog() {
  const { el, icon, translated } = DeviceConsole.utils.dom;
  const filter = el('label', {
    children: [
      icon('filter-funnel'),
      el('input', {
        id: 'scanFilter',
        attrs: {
          'data-i18n-placeholder': 'filterBySsid',
          placeholder: 'Filter by SSID'
        }
      })
    ]
  });
  const rescan = el('button', {
    id: 'rescanButton',
    className: 'button',
    attrs: { type: 'button' },
    children: [icon('refresh'), translated('span', 'scanAgain', 'Scan again')]
  });
  return el('section', {
    id: 'scanDialog',
    className: 'dialog scan-dialog',
    attrs: {
      role: 'dialog',
      'aria-modal': 'true',
      'aria-labelledby': 'scanTitle'
    },
    props: { hidden: true },
    children: [
      el('header', {
        children: [
          translated('h2', 'chooseNetwork', 'Choose a network', { id: 'scanTitle' }),
          el('button', {
            id: 'closeScanButton',
            className: 'close-button',
            attrs: { type: 'button', 'aria-label': 'Close' },
            children: [icon('close')]
          })
        ]
      }),
      el('div', { className: 'scan-tools', children: [filter, rescan] }),
      el('div', {
        className: 'result-meta',
        children: [translated('span', 'scanEmpty', 'No scan yet.', { id: 'scanResultCount' })]
      }),
      el('div', {
        id: 'scanResults',
        className: 'network-results',
        children: [translated('p', 'scanEmpty', 'No scan yet.', { className: 'empty-state' })]
      }),
      el('footer', {
        children: [
          translated('button', 'done', 'Done', {
            id: 'doneScanButton',
            className: 'button primary',
            attrs: { type: 'button' }
          })
        ]
      })
    ]
  });
}

function mountWifiDialogs() {
  DeviceConsole.ui.dialog.add(createScanDialog());
}

function selectedWifiMode() {
  const selected = document.querySelector('input[name="wifiMode"]:checked');
  return selected ? selected.value : '';
}

function configurableWifiModeLabel(mode) {
  return ({ ap: 'AP', sta: 'STA', ap_sta: 'AP + STA' })[mode] || mode || '—';
}

function wifiFormValue() {
  return JSON.stringify(wifiPayloadFromForm());
}

function wifiFormHasChanges() {
  return Boolean($('wifiForm')) && Boolean(wifiFormBaseline) && wifiFormValue() !== wifiFormBaseline;
}

function markSavedWifiMode(mode) {
  $$('input[name="wifiMode"]').forEach((input) => {
    if (input.value === mode) input.dataset.savedMode = 'true';
    else delete input.dataset.savedMode;
  });
}

function captureWifiFormBaseline(value = wifiFormValue()) {
  wifiFormBaseline = typeof value === 'string' ? value : JSON.stringify(value);
  const baseline = typeof value === 'string' ? JSON.parse(value) : value;
  markSavedWifiMode(baseline.mode);
  updateWifiSaveAvailability();
}

function updateWifiSaveAvailability() {
  if (!$('wifiForm') || !wifiFormBaseline) return;
  const form = $('wifiForm');
  const dirty = wifiFormHasChanges();
  const valid = Boolean(selectedWifiMode()) && form.checkValidity();
  form.classList.toggle('form-edited', dirty);
  if (dirty) $('wifiRecovery').hidden = true;
  $('wifiSaveButton').disabled = wifiFormBusy || !dirty || !valid;
  setText('wifiSaveState', wifiFormBusy
    ? t(wifiBusyMessageKey)
    : !dirty ? t('noChanges') : valid ? t('readyToSave') : t('fixInvalidFields'));
}

function fillWifiForm(wifi) {
  if (!$('wifiForm') || !wifi) return;
  const mode = wifi.configured_mode || wifi.mode;
  const modeInput = document.querySelector(`input[name="wifiMode"][value="${mode}"]`);
  $$('input[name="wifiMode"]').forEach((input) => { input.checked = input === modeInput; });
  const sta = wifi.interfaces && wifi.interfaces.sta ? wifi.interfaces.sta : {};
  const ap = wifi.interfaces && wifi.interfaces.ap ? wifi.interfaces.ap : {};
  const staIp = sta.ip_config || {};
  const apIp = ap.ip_config || {};
  $('fallbackToAp').checked = wifi.fallback_to_ap !== false;
  $('staSsid').value = sta.ssid || '';
  $('staSecurity').value = sta.security === 'open' ? 'open' : 'wpa';
  $('staPassword').value = '';
  $('apSsid').value = ap.ssid || '';
  $('apPasswordEnabled').checked = Boolean(ap.password_enabled);
  $('staIpMode').value = staIp.mode || 'dhcp';
  $('staIpAddress').value = staIp.address || '';
  $('staIpGateway').value = staIp.gateway || '';
  $('staIpNetmask').value = staIp.netmask || '';
  $('staDns1').value = Array.isArray(staIp.dns) ? staIp.dns[0] || '' : '';
  $('staDns2').value = Array.isArray(staIp.dns) ? staIp.dns[1] || '' : '';
  $('apIpMode').value = apIp.mode || 'default';
  $('apIpAddress').value = apIp.address || '192.168.4.1';
  $('apIpNetmask').value = apIp.netmask || '255.255.255.0';
  renderWifiRuntimeStatus(wifi);
  syncWifiFormState();
  captureWifiFormBaseline();
}

function syncWifiFormState() {
  if (!$('wifiForm')) return;
  updateModeSections();
  updateSecurityField();
  updateWifiSaveAvailability();
}

function renderWifiRuntimeStatus(wifi) {
  const ap = wifi && wifi.interfaces && wifi.interfaces.ap ? wifi.interfaces.ap : {};
  setText('settingsApIp', ap.ip);
}

function updateModeSections() {
  const mode = selectedWifiMode();
  const showSta = mode === 'sta' || mode === 'ap_sta';
  const showAp = mode === 'ap' || mode === 'ap_sta';
  $('staSettings').hidden = !showSta;
  $('apSettings').hidden = !showAp;
  $('wifiSettingsLayout').classList.toggle('single', showSta !== showAp);
  $('fallbackField').hidden = mode !== 'sta';
  $('staAdvancedSettings').hidden = !showSta;
  $('apAdvancedSettings').hidden = !showAp;
  $('staIpModeField').hidden = !showSta;
  $('staIpMode').disabled = !showSta;
  $('apIpModeField').hidden = !showAp;
  $('apIpMode').disabled = !showAp;
  $('staSsid').disabled = !showSta;
  $('staSsid').required = showSta;
  $('staSecurity').disabled = !showSta;
  $('apSsid').disabled = !showAp;
  $('apSsid').required = showAp;
  $('apPasswordEnabled').disabled = !showAp;
  updateIpModeFields();
  setText('selectedModeLabel', configurableWifiModeLabel(mode));
  const descriptionKey = mode === 'ap'
    ? 'apModeDescription'
    : mode === 'sta' ? 'staModeDescription' : mode === 'ap_sta' ? 'apstaModeDescription' : 'chooseConnectionMode';
  setText('modeDescription', t(descriptionKey));
  updateWifiSaveAvailability();
}

function updateIpModeFields() {
  const staticSta = $('staIpMode').value === 'static' && !$('staIpMode').disabled;
  $$('.staStaticIpField').forEach((label) => {
    label.hidden = !staticSta;
    const input = label.querySelector('input');
    input.disabled = !staticSta;
    input.required = staticSta && !['staDns1', 'staDns2'].includes(input.id);
  });
  const staticAp = $('apIpMode').value === 'static' && !$('apIpMode').disabled;
  $$('.apStaticIpField').forEach((label) => {
    label.hidden = !staticAp;
    const input = label.querySelector('input');
    input.disabled = !staticAp;
    input.required = staticAp;
  });
}

function updateSecurityField() {
  const open = $('staSecurity').value === 'open';
  const staVisible = !$('staSecurity').disabled;
  const savedSta = state.wifi && state.wifi.interfaces ? state.wifi.interfaces.sta || {} : {};
  const savedSsidSelected = Boolean(savedSta.ssid) && savedSta.ssid === $('staSsid').value.trim();
  const passwordDisabled = open || !staVisible;
  $('staPassword').disabled = passwordDisabled;
  document.querySelector('[data-toggle-password="staPassword"]').disabled = passwordDisabled;
  $('staPassword').required = staVisible && !open && !savedSsidSelected;
  $('staPassword').placeholder = staVisible && !open && savedSsidSelected ? '********' : '';
}

function wifiPayloadFromForm() {
  const mode = selectedWifiMode();
  const staIp = {
    mode: $('staIpMode').value,
    address: $('staIpMode').value === 'static' ? $('staIpAddress').value.trim() : '',
    gateway: $('staIpMode').value === 'static' ? $('staIpGateway').value.trim() : '',
    netmask: $('staIpMode').value === 'static' ? $('staIpNetmask').value.trim() : '',
    dns: $('staIpMode').value === 'static'
      ? [$('staDns1').value.trim(), $('staDns2').value.trim()].filter(Boolean)
      : []
  };
  const apIp = { mode: $('apIpMode').value };
  if (apIp.mode === 'static') {
    apIp.address = $('apIpAddress').value.trim();
    apIp.netmask = $('apIpNetmask').value.trim();
  }
  const interfaces = {
    sta: {
      ssid: $('staSsid').value.trim(),
      security: $('staSecurity').value,
      ip_config: staIp
    },
    ap: {
      ssid: $('apSsid').value.trim(),
      password_enabled: $('apPasswordEnabled').checked,
      ip_config: apIp
    }
  };
  if ($('staPassword').value && $('staSecurity').value !== 'open') {
    interfaces.sta.password = $('staPassword').value;
  }
  return { mode, fallback_to_ap: $('fallbackToAp').checked, interfaces };
}

function filteredScanNetworks() {
  const query = $('scanFilter').value.trim().toLowerCase();
  return state.scanNetworks
    .filter((network) => network.ssid && !network.hidden && Number(network.rssi) >= MIN_VISIBLE_RSSI)
    .filter((network) => !query || network.ssid.toLowerCase().includes(query))
    .sort((left, right) => Number(right.rssi) - Number(left.rssi));
}

function wifiSignalLevel(rssi) {
  if (rssi >= -50) return 4;
  if (rssi >= -60) return 3;
  if (rssi >= -68) return 2;
  return 1;
}

function renderScanResults() {
  if (!$('scanResults')) return;
  const networks = filteredScanNetworks();
  setText('scanResultCount', state.scanDetectedCount === null ? t('scanEmpty') : tf('detectedResults', { count: networks.length }));
  if (!networks.length) {
    const emptyMessage = state.scanDetectedCount === null
      ? t('scanEmpty')
      : t('scanNoVisibleNetworks');
    renderScanMessage(emptyMessage);
    return;
  }
  const { el, icon } = DeviceConsole.utils.dom;
  const options = networks.map((network) => {
    const selected = network.ssid === state.selectedSsid;
    const security = network.encryption === 'open' ? t('open') : String(network.encryption || network.encryption_type).toUpperCase();
    const signalLevel = wifiSignalLevel(Number(network.rssi));
    const securityValue = network.encryption === 'open' ? 'open' : 'wpa';
    return el('button', {
      className: `network-option${selected ? ' selected' : ''}`,
      attrs: { type: 'button' },
      dataset: { ssid: network.ssid, security: securityValue },
      children: [
        icon(`wifi-strength-${signalLevel}`, 'signal-mark'),
        el('span', {
          children: [
            el('b', { text: network.ssid }),
            el('small', {
              text: `${network.rssi} dBm · ${t('channel')} ${network.channel} · ${security}`
            })
          ]
        }),
        el('em', { text: selected ? t('selected') : t('use') })
      ]
    });
  });
  $('scanResults').replaceChildren(...options);
}

function renderScanMessage(message) {
  const { el } = DeviceConsole.utils.dom;
  $('scanResults').replaceChildren(el('p', { className: 'empty-state', text: message }));
}
