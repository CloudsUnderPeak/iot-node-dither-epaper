let scanCooldownTimer = 0;
let wifiStatusPollGeneration = 0;
const WIFI_TRANSITION_POLL_MS = 1000;
const WIFI_TRANSITION_POLL_DEADLINE_MS = 25000;

function comparableApSettings(ap = {}) {
  const ip = ap.ip_config || {};
  return JSON.stringify({
    ssid: ap.ssid || '',
    password_enabled: Boolean(ap.password_enabled),
    ip_config: ip.mode === 'static'
      ? { mode: 'static', address: ip.address || '', netmask: ip.netmask || '' }
      : { mode: 'default' }
  });
}

function acceptSavedWifiForm(submittedFormValue) {
  const savedForm = JSON.parse(submittedFormValue);
  delete savedForm.interfaces.sta.password;
  if ($('wifiForm') && wifiFormValue() === submittedFormValue) $('staPassword').value = '';
  captureWifiFormBaseline(savedForm);
}

function finishWifiTransitionFailure(generation, messageKey) {
  if (generation !== wifiStatusPollGeneration) return;
  wifiFormBusy = false;
  wifiBusyMessageKey = 'savingWifi';
  updateWifiSaveAvailability();
  const message = t(messageKey);
  const preserveMessage = () => {
    if (generation !== wifiStatusPollGeneration) return;
    setNotice(message, true);
    if ($('wifiSaveState')) setText('wifiSaveState', message);
  };
  preserveMessage();
  showWifiRecovery('wifiReconnectHelp');
  invalidateResourceRequests('wifi');
  refreshWifiSnapshot({ formSync: 'none' })
    .then(preserveMessage)
    .catch((error) => reportBackgroundError('Wi-Fi rollback refresh', error));
}

async function pollWifiTransition({
  payload,
  submittedFormValue,
  generation,
  apChanged,
  deadline = Date.now() + WIFI_TRANSITION_POLL_DEADLINE_MS
}) {
  while (generation === wifiStatusPollGeneration && Date.now() < deadline) {
    try {
      const status = await resources.wifi.connectionStatus();
      if (generation !== wifiStatusPollGeneration) return;
      if (status.state === 'connected') {
        wifiFormBusy = false;
        wifiBusyMessageKey = 'savingWifi';
        acceptSavedWifiForm(submittedFormValue);
        invalidateResourceRequests('wifi');

        const shutdownSeconds = Number(status.ap_shutdown_in_seconds) || 0;
        let messageKey = 'wifiConnectionReady';
        let message = t(messageKey);
        if (payload.mode === 'sta' && shutdownSeconds > 0) {
          messageKey = 'wifiStaGrace';
          message = tf(messageKey, {
            ip: status.ip || '—',
            seconds: shutdownSeconds
          });
        } else if (payload.mode === 'ap_sta' && apChanged) {
          messageKey = 'wifiApChangedReconnect';
          message = t(messageKey);
        }
        setTransientNotice(message);
        if ($('wifiSaveState')) setText('wifiSaveState', message);
        showWifiRecovery(payload.mode === 'sta' ? 'wifiRecoveryAvailable' : 'wifiReconnectHelp');
        refreshWifiSnapshot({ formSync: 'none' }).then(() => {
          if (generation === wifiStatusPollGeneration && $('wifiSaveState')) {
            setText('wifiSaveState', message);
          }
        }).catch((error) => reportBackgroundError('Wi-Fi terminal refresh', error));
        return;
      }
      if (status.state === 'failed') {
        finishWifiTransitionFailure(generation, 'wifiRollbackComplete');
        return;
      }
    } catch (error) {
      // The retained AP can briefly miss a request; retry within the bounded
      // window before asking the user to reconnect manually. Transport errors
      // are expected here; API contract errors remain diagnostic.
      reportBackgroundError(
        'Wi-Fi transition polling',
        error,
        ['transport_error']
      );
    }
    await new Promise((resolve) => window.setTimeout(resolve, WIFI_TRANSITION_POLL_MS));
  }
  finishWifiTransitionFailure(generation, 'wifiTransitionRetryEnded');
}

function wifiStatusIsSettled(wifi, configuredMode) {
  if (!wifi || wifi.configured_mode !== configuredMode) return false;
  const interfaces = wifi.interfaces || {};
  const staState = interfaces.sta ? interfaces.sta.state : 'disabled';
  const apState = interfaces.ap ? interfaces.ap.state : 'disabled';
  if (staState === 'connecting' || apState === 'starting') return false;
  if ((configuredMode === 'sta' || configuredMode === 'ap_sta')
    && !['connected', 'failed'].includes(staState)) return false;
  if ((configuredMode === 'ap' || configuredMode === 'ap_sta')
    && !['active', 'failed'].includes(apState)) return false;
  return true;
}

function retryWifiStatus(configuredMode, attempt = 0, generation = wifiStatusPollGeneration) {
  const delays = [2000, 5000, 10000];
  if (generation !== wifiStatusPollGeneration) return;
  if (attempt >= delays.length) {
    if ($('wifiSaveState')) setText('wifiSaveState', t('wifiRetryEnded'));
    showWifiRecovery('wifiReconnectHelp');
    return;
  }
  window.setTimeout(async () => {
    if (generation !== wifiStatusPollGeneration) return;
    try {
      const wifi = await refreshWifiSnapshot({ formSync: 'none' });
      if (!wifiStatusIsSettled(wifi, configuredMode)) {
        retryWifiStatus(configuredMode, attempt + 1, generation);
        return;
      }
      const interfaces = wifi.interfaces || {};
      const failed = (interfaces.sta && interfaces.sta.state === 'failed')
        || (interfaces.ap && interfaces.ap.state === 'failed');
      if ($('wifiSaveState')) setText('wifiSaveState', t(failed ? 'wifiApplyFailed' : 'wifiConnectionReady'));
      if (failed) setNotice(t('wifiApplyFailed'), true);
      else setTransientNotice(t('wifiConnectionReady'));
      showWifiRecovery(failed ? 'wifiReconnectHelp' : 'wifiRecoveryAvailable');
    } catch (error) {
      // A network handoff can make transport failures transient. Unexpected
      // API errors are logged without request bodies or credentials.
      reportBackgroundError('Wi-Fi status retry', error, ['transport_error']);
      retryWifiStatus(configuredMode, attempt + 1, generation);
    }
  }, delays[attempt]);
}

function showWifiRecovery(messageKey) {
  if (!$('wifiRecovery')) return;
  const hostname = state.device && state.device.hostname ? state.device.hostname : '';
  const recovery = $('wifiRecovery');
  recovery.dataset.messageKey = messageKey;
  setText('wifiRecoveryMessage', t(messageKey));
  if (hostname) {
    const url = `http://${hostname}.local/`;
    $('wifiRecoveryLink').textContent = url;
    $('wifiRecoveryLink').href = url;
    $('wifiRecoveryLink').hidden = false;
  } else {
    $('wifiRecoveryLink').hidden = true;
  }
  recovery.hidden = false;
}

async function saveWifi(event) {
  event.preventDefault();
  updateWifiSaveAvailability();
  if ($('wifiSaveButton').disabled) return;
  wifiFormBusy = true;
  wifiBusyMessageKey = 'savingWifi';
  $('wifiSaveButton').disabled = true;
  setNotice(t('savingWifi'));
  setText('wifiSaveState', t('savingWifi'));
  const payload = wifiPayloadFromForm();
  const submittedFormValue = JSON.stringify(payload);
  const previousAp = state.wifi && state.wifi.interfaces ? state.wifi.interfaces.ap : {};
  const apProtectionChanged = Boolean(previousAp.password_enabled)
    !== Boolean(payload.interfaces.ap.password_enabled);
  const apChanged = comparableApSettings(previousAp) !== comparableApSettings(payload.interfaces.ap);
  const generation = ++wifiStatusPollGeneration;
  try {
    const result = await resources.wifi.update(payload);
    if (result.state === 'connecting') {
      wifiBusyMessageKey = 'wifiVerifying';
      updateWifiSaveAvailability();
      setNotice(t('wifiVerifying'));
      pollWifiTransition({ payload, submittedFormValue, generation, apChanged });
      return;
    }
    invalidateResourceRequests('wifi');
    wifiFormBusy = false;
    acceptSavedWifiForm(submittedFormValue);
    if (apProtectionChanged) {
      setToken('');
      DeviceConsole.app.router.navigate('network');
      setNotice(t('restarting'));
      return;
    }
    setNotice(t('wifiSavedReconnect'));
    if ($('wifiSaveState')) setText('wifiSaveState', t('wifiSavedReconnect'));
    showWifiRecovery('wifiReconnectHelp');
    if (generation === wifiStatusPollGeneration) retryWifiStatus(payload.mode);
  } catch (error) {
    wifiFormBusy = false;
    wifiBusyMessageKey = 'savingWifi';
    updateWifiSaveAvailability();
    setNotice(error.message, true);
    setText('wifiSaveState', error.message);
  }
}

function openScanDialog() {
  DeviceConsole.ui.dialog.open('scanDialog');
  renderScanResults();
  if (!state.scanNetworks.length) scanWifi();
}

function setScanDisabled(disabled) {
  $('rescanButton').disabled = disabled;
}

function startScanCooldown(seconds = 10) {
  let remaining = Math.max(1, Math.ceil(Number(seconds) || 10));
  setScanDisabled(true);
  window.clearInterval(scanCooldownTimer);
  scanCooldownTimer = window.setInterval(() => {
    remaining -= 1;
    if (remaining > 0) {
      $('rescanButton').querySelector('span').textContent = tf('scanWait', { seconds: remaining });
      return;
    }
    window.clearInterval(scanCooldownTimer);
    setScanDisabled(false);
    $('rescanButton').querySelector('span').textContent = t('scanAgain');
  }, 1000);
}

async function scanWifi() {
  setScanDisabled(true);
  setText('scanResultCount', t('scanning'));
  renderScanMessage(t('scanning'));
  let cooldownSeconds = 10;
  try {
    const data = await resources.wifi.scan();
    state.scanNetworks = data.networks || [];
    state.scanDetectedCount = state.scanNetworks.length;
    renderScanResults();
    setTransientNotice(t('scanComplete'));
  } catch (error) {
    cooldownSeconds = error.data?.retry_after_seconds || cooldownSeconds;
    const message = error.code === 'wifi_scan_busy' || error.code === 'wifi_connect_busy'
      ? t('connecting')
      : error.message;
    renderScanMessage(message);
    setNotice(message, true);
  } finally {
    startScanCooldown(cooldownSeconds);
  }
}

function selectScannedNetwork(event) {
  const option = event.target.closest('[data-ssid]');
  if (!option) return;
  state.selectedSsid = option.dataset.ssid;
  $('staSsid').value = option.dataset.ssid;
  $('staSecurity').value = option.dataset.security;
  updateSecurityField();
  updateWifiSaveAvailability();
  renderScanResults();
}

function bindWifi() {
  $('wifiForm').addEventListener('submit', saveWifi);
  $$('input[name="wifiMode"]').forEach((input) => input.addEventListener('change', updateModeSections));
  $('fallbackToAp').addEventListener('change', updateModeSections);
  $('staSecurity').addEventListener('change', updateSecurityField);
  $('staSsid').addEventListener('input', updateSecurityField);
  $('staIpMode').addEventListener('change', updateIpModeFields);
  $('apIpMode').addEventListener('change', updateIpModeFields);
  $('chooseNetworkButton').addEventListener('click', openScanDialog);
  $('rescanButton').addEventListener('click', scanWifi);
  $('scanFilter').addEventListener('input', renderScanResults);
  $('scanResults').addEventListener('click', selectScannedNetwork);
  $('closeScanButton').addEventListener('click', DeviceConsole.ui.dialog.close);
  $('doneScanButton').addEventListener('click', DeviceConsole.ui.dialog.close);
  $('wifiForm').addEventListener('input', updateWifiSaveAvailability);
  $('wifiForm').addEventListener('change', updateWifiSaveAvailability);
}
