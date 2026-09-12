(async function runApplicationTests() {
  const result = document.getElementById('result');

  function assert(condition, message) {
    if (!condition) throw new Error(message);
  }

  function delay(milliseconds) {
    return new Promise((resolve) => window.setTimeout(resolve, milliseconds));
  }

  async function waitFor(check, message, timeout = 8000) {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      if (check()) return;
      await delay(25);
    }
    throw new Error(message);
  }

  function submit(form) {
    form.dispatchEvent(new Event('submit', {
      bubbles: true,
      cancelable: true
    }));
  }

  await waitFor(
    () => state.device && state.wifi && document.querySelector('.preview-badge'),
    `application bootstrap did not finish: ${
      (window.__webTestErrors || []).join(' | ') || 'no browser error captured'
    }`
  );

  assert(
    DeviceConsole.app.router && DeviceConsole.ui.dialog,
    'application public interfaces are missing'
  );
  assert(
    ['network', 'hardware', 'settings'].every((name) => DeviceConsole.pages[name]),
    'page interfaces are missing after classic-script bootstrap'
  );
  assert(
    ['detailRow', 'el', 'icon', 'svg', 'translated']
      .every((name) => typeof DeviceConsole.utils.dom[name] === 'function'),
    'DOM utility interfaces are missing after classic-script bootstrap'
  );

  const originalWifiMode = state.wifi.mode;
  state.wifi.mode = 'off';
  DeviceConsole.pages.network.render();
  assert(
    document.getElementById('networkMode').textContent === 'Off',
    'Network runtime mode label lost off-state semantics'
  );
  state.wifi.mode = originalWifiMode;
  DeviceConsole.pages.network.render();

  const warnings = [];
  const originalWarn = console.warn;
  console.warn = (...items) => warnings.push(items);
  reportBackgroundError(
    'expected transport',
    { code: 'transport_error', status: 0 },
    ['transport_error']
  );
  reportBackgroundError('unexpected API response', {
    code: 'invalid_contract',
    status: 500
  });
  console.warn = originalWarn;
  assert(warnings.length === 1, 'unexpected background errors must be diagnostic');

  const trigger = document.createElement('button');
  trigger.type = 'button';
  trigger.textContent = 'Dialog trigger';
  document.getElementById('app').append(trigger);
  trigger.focus();
  DeviceConsole.ui.dialog.open('loginDialog');
  await waitFor(
    () => document.activeElement === document.getElementById('loginPassword'),
    'dialog did not focus its initial control'
  );
  assert(
    document.getElementById('app').hasAttribute('inert'),
    'dialog background is not inert'
  );

  const firstDialogControl = document.getElementById('loginUsername');
  const lastDialogControl = document.getElementById('cancelLoginButton');
  lastDialogControl.focus();
  lastDialogControl.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Tab',
    bubbles: true,
    cancelable: true
  }));
  assert(
    document.activeElement === firstDialogControl,
    'forward Tab did not wrap inside the dialog'
  );
  firstDialogControl.focus();
  firstDialogControl.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Tab',
    shiftKey: true,
    bubbles: true,
    cancelable: true
  }));
  assert(
    document.activeElement === lastDialogControl,
    'reverse Tab did not wrap inside the dialog'
  );
  DeviceConsole.ui.dialog.close();
  assert(document.activeElement === trigger, 'dialog did not restore trigger focus');
  assert(
    !document.getElementById('app').hasAttribute('inert'),
    'dialog close did not restore the background'
  );
  trigger.remove();

  const previewBadge = document.querySelector('.preview-badge');
  previewBadge.focus();
  previewBadge.click();
  await waitFor(
    () => !document.getElementById('previewDialog').hidden,
    'preview dialog did not open'
  );
  document.getElementById('dialogBackdrop').click();
  assert(
    document.getElementById('previewDialog').hidden,
    'backdrop did not close a non-login dialog'
  );
  assert(
    document.activeElement === previewBadge,
    'backdrop close did not restore focus'
  );
  previewBadge.focus();
  previewBadge.click();
  document.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Escape',
    bubbles: true
  }));
  assert(
    document.getElementById('previewDialog').hidden,
    'Escape did not close the dialog'
  );

  document.querySelector('[data-page="settings"]').click();
  await waitFor(
    () => !document.getElementById('loginDialog').hidden,
    'Settings did not request login'
  );
  document.getElementById('loginPassword').value = 'wrongpass';
  submit(document.getElementById('loginForm'));
  await waitFor(
    () => document.getElementById('loginNotice').textContent.includes('incorrect'),
    'invalid login was not reported'
  );
  document.getElementById('loginPassword').value = 'password';
  submit(document.getElementById('loginForm'));
  await waitFor(
    () => state.activePage === 'settings'
      && document.getElementById('loginDialog').hidden,
    'valid login did not unlock Settings'
  );

  await resources.auth.logout();
  assert(!(await verifySession()), 'invalidated server session remained valid');
  assert(!state.token, 'invalid session did not clear the local token');

  DeviceConsole.app.router.navigate('network');
  await waitFor(() => state.activePage === 'network', 'Network route did not open');
  document.querySelector('[data-page="settings"]').click();
  await waitFor(
    () => !document.getElementById('loginDialog').hidden,
    'login did not reopen after session invalidation'
  );
  document.getElementById('loginPassword').value = 'password';
  submit(document.getElementById('loginForm'));
  await waitFor(() => state.activePage === 'settings', 'second login failed');

  document.querySelector('[data-settings-page="admin"]').click();
  await waitFor(
    () => !document.getElementById('settings-admin').hidden,
    'Admin Settings did not open'
  );
  document.getElementById('newAdminPassword').value = 'Newpass1!';
  document.getElementById('confirmAdminPassword').value = 'Newpass1!';
  submit(document.getElementById('passwordForm'));
  await waitFor(
    () => state.activePage === 'network' && !state.token,
    'password change did not invalidate the session'
  );

  document.querySelector('[data-page="settings"]').click();
  await waitFor(
    () => !document.getElementById('loginDialog').hidden,
    'login did not reopen after password change'
  );
  document.getElementById('loginPassword').value = 'password';
  submit(document.getElementById('loginForm'));
  await waitFor(
    () => document.getElementById('loginNotice').textContent.includes('incorrect'),
    'old password remained valid'
  );
  document.getElementById('loginPassword').value = 'Newpass1!';
  submit(document.getElementById('loginForm'));
  await waitFor(() => state.activePage === 'settings', 'new password was rejected');

  await resources.system.resetSettings();
  await delay(500);
  assert(!(await verifySession()), 'settings reset did not invalidate the session');
  DeviceConsole.app.router.navigate('network');
  await waitFor(() => state.activePage === 'network', 'reset did not return to Network');
  document.querySelector('[data-page="settings"]').click();
  await waitFor(
    () => !document.getElementById('loginDialog').hidden,
    'login did not open after settings reset'
  );
  document.getElementById('loginPassword').value = 'password';
  submit(document.getElementById('loginForm'));
  await waitFor(() => state.activePage === 'settings', 'factory password was not restored');
  await refreshResources(['wifi'], {
    showFeedback: false,
    formSync: 'force'
  });

  const formRect = document.getElementById('wifiForm').getBoundingClientRect();
  const dockRect = document.querySelector('.save-dock-inner').getBoundingClientRect();
  assert(
    Math.abs(formRect.left - dockRect.left) <= 1
      && Math.abs(formRect.right - dockRect.right) <= 1,
    'fixed Wi-Fi save dock does not align with the form content width'
  );

  const apProtection = document.getElementById('apPasswordEnabled');
  apProtection.checked = true;
  apProtection.dispatchEvent(new Event('change', { bubbles: true }));
  submit(document.getElementById('wifiForm'));
  await waitFor(
    () => state.activePage === 'network' && !state.token,
    'AP protection change did not enter the restart flow'
  );
  await delay(500);
  assert(!(await verifySession()), 'AP protection restart did not invalidate the session');

  document.querySelector('[data-page="settings"]').click();
  await waitFor(
    () => !document.getElementById('loginDialog').hidden,
    'login did not reopen after AP protection restart'
  );
  document.getElementById('loginPassword').value = 'password';
  submit(document.getElementById('loginForm'));
  await waitFor(() => state.activePage === 'settings', 'login after AP protection restart failed');
  await delay(800);
  await refreshResources(['wifi'], {
    showFeedback: false,
    formSync: 'force'
  });

  const staMode = document.querySelector('input[name="wifiMode"][value="sta"]');
  staMode.checked = true;
  staMode.dispatchEvent(new Event('change', { bubbles: true }));
  document.getElementById('staSsid').value = 'Browser-Test';
  document.getElementById('staSsid').dispatchEvent(
    new Event('input', { bubbles: true })
  );
  document.getElementById('staSecurity').value = 'wpa';
  document.getElementById('staSecurity').dispatchEvent(
    new Event('change', { bubbles: true })
  );
  document.getElementById('staPassword').value = 'station-pass';
  document.getElementById('staPassword').dispatchEvent(
    new Event('input', { bubbles: true })
  );
  assert(
    !document.getElementById('wifiSaveButton').disabled,
    'valid Wi-Fi replacement was not enabled'
  );

  const transitionPayload = wifiPayloadFromForm();
  const submittedFormValue = JSON.stringify(transitionPayload);
  const baselineBeforeStaleResponse = wifiFormBaseline;
  const originalConnectionStatus = resources.wifi.connectionStatus;
  let resolveStaleStatus;
  resources.wifi.connectionStatus = () => new Promise((resolve) => {
    resolveStaleStatus = resolve;
  });
  const staleGeneration = ++wifiStatusPollGeneration;
  wifiFormBusy = true;
  const stalePoll = pollWifiTransition({
    payload: transitionPayload,
    submittedFormValue,
    generation: staleGeneration,
    apChanged: false
  });
  wifiStatusPollGeneration += 1;
  resolveStaleStatus({
    state: 'connected',
    ip: '192.168.50.90',
    ap_shutdown_in_seconds: 0
  });
  await stalePoll;
  assert(wifiFormBusy, 'stale Wi-Fi response changed the busy state');
  assert(
    wifiFormBaseline === baselineBeforeStaleResponse,
    'stale Wi-Fi response replaced the form baseline'
  );
  wifiFormBusy = false;
  resources.wifi.connectionStatus = originalConnectionStatus;
  updateWifiSaveAvailability();
  assert(
    !document.getElementById('wifiSaveButton').disabled,
    `Wi-Fi save became disabled after stale-response protection: ${
      document.getElementById('wifiSaveState').textContent
    }`
  );

  submit(document.getElementById('wifiForm'));
  await waitFor(
    () => !wifiFormBusy,
    `safe Wi-Fi transition did not reach terminal connected state: ${
      document.getElementById('wifiSaveState').textContent
    }`
  );
  assert(
    JSON.parse(wifiFormBaseline).mode === 'sta',
    `safe transition did not commit the submitted Wi-Fi baseline: ${wifiFormBaseline}; ${
      document.getElementById('wifiSaveState').textContent
    }`
  );

  const tokenBeforeLeave = state.token;
  const networksBeforeLeave = state.scanNetworks;
  const originalFetch = window.fetch;
  let resolveAbandonedScan;
  window.fetch = (path, options) => path === '/api/wifi/scan'
    ? new Promise(resolve => { resolveAbandonedScan = resolve; }) : originalFetch(path, options);
  const abandonedScan = scanWifi();
  DeviceConsole.app.router.navigate('hardware');
  await waitFor(() => state.activePage === 'hardware', 'navigation during scan did not finish');
  await abandonedScan;
  resolveAbandonedScan(new Response(JSON.stringify({ success: true, data: { networks: [{ ssid: 'late' }] } })));
  await delay(25);
  assert(state.token === tokenBeforeLeave, 'route cancellation cleared token');
  assert(state.scanNetworks === networksBeforeLeave, 'late abandoned scan changed shared state');
  window.fetch = originalFetch;

  result.textContent = 'PASS';
})().catch((error) => {
  document.getElementById('result').textContent = `FAIL: ${error.message}`;
});
