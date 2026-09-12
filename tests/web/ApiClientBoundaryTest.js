(async function runApiClientBoundaryTests() {
  function assert(condition, message) {
    if (!condition) throw new Error(message);
  }

  const mockEnabled = window.DEVICE_CONSOLE_FLAGS.mockApi === true;
  if (mockEnabled) {
    assert(window.fetch !== window.__formalFetch, 'mock adapter did not intercept fetch');
    const login = await resources.auth.login({ username: 'admin', password: 'password' });
    setToken(login.token);
    const first = resources.wifi.scan();
    const second = await resources.wifi.scan().catch(error => error);
    assert(second.status === 409 && second.code === 'wifi_scan_busy', 'concurrent scan must be busy');
    const completed = await first;
    assert(Array.isArray(completed.networks), 'scan must retain networks envelope');
    const device = await resources.device.get();
    assert(device.chip_model === 'ESP32-C6', 'mock resource contract was not used');
    await resources.auth.logout();
    setToken('');
    assert(
      window.__formalRequests.length === 0,
      'mock API leaked a device request to formal transport'
    );
  } else {
    assert(window.fetch === window.__formalFetch, 'disabled mock replaced fetch');
    setToken('boundary-token');
    await resources.wifi.update({
      mode: 'ap',
      fallback_to_ap: true,
      interfaces: {
        sta: {
          ssid: '',
          security: 'wpa',
          ip_config: {
            mode: 'dhcp',
            address: '',
            gateway: '',
            netmask: '',
            dns: []
          }
        },
        ap: {
          ssid: 'boundary-ap',
          password_enabled: false,
          ip_config: {
            mode: 'default',
            address: '192.168.4.1',
            netmask: '255.255.255.0'
          }
        }
      }
    });
    await resources.device.get();

    const protectedRequest = window.__formalRequests[0];
    const publicRequest = window.__formalRequests[1];
    assert(
      protectedRequest.path === '/api/wifi'
        && protectedRequest.options.method === 'PUT',
      'resource layer changed the Wi-Fi route or method'
    );
    assert(
      protectedRequest.options.headers.Authorization === 'Bearer boundary-token',
      'API client omitted the protected bearer token'
    );
    assert(
      JSON.parse(protectedRequest.options.body).mode === 'ap',
      'API client did not serialize the request body'
    );
    assert(
      publicRequest.path === '/api/device'
        && !publicRequest.options.headers.Authorization,
      'public resource unexpectedly sent authorization'
    );
  }

  document.getElementById('result').textContent = 'PASS';
})().catch((error) => {
  document.getElementById('result').textContent = `FAIL: ${error.message}`;
});
