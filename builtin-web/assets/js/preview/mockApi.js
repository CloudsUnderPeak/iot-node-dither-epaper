(function enableMockApi() {
  if (!window.DEVICE_CONSOLE_FLAGS || window.DEVICE_CONSOLE_FLAGS.mockApi !== true) return;

  const DEFAULT_ADMIN_PASSWORD = 'password';
  const DEFAULT_AP_ADDRESS = '192.168.4.1';
  const DEFAULT_AP_NETMASK = '255.255.255.0';
  const PREVIEW_TOKEN_PREFIX = 'preview-session-';
  const RESET_NOTICE_KEY = 'preview.reset.completed';
  const SCAN_COOLDOWN_MS = 10000;
  const previewView = new URLSearchParams(window.location.search).get('view');
  const realFetch = window.fetch.bind(window);
  let previewUi = null;
  let previewUiState = 'default';

  const initialWifiConfig = {
    mode: 'ap_sta',
    fallback_to_ap: true,
    interfaces: {
      sta: {
        ssid: 'Studio-WiFi',
        security: 'wpa',
        ip_config: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
      },
      ap: {
        ssid: 'gallery-panel-c6-A7C2',
        password_enabled: false,
        ip_config: { mode: 'default', address: DEFAULT_AP_ADDRESS, netmask: DEFAULT_AP_NETMASK }
      }
    }
  };

  const factoryWifiConfig = {
    mode: 'ap',
    fallback_to_ap: true,
    interfaces: {
      sta: {
        ssid: '',
        security: 'wpa',
        ip_config: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
      },
      ap: {
        ssid: 'esp32-device-A7C2',
        password_enabled: false,
        ip_config: { mode: 'default', address: DEFAULT_AP_ADDRESS, netmask: DEFAULT_AP_NETMASK }
      }
    }
  };

  const preview = {
    device: {
      hostname: 'gallery-panel-c6',
      chip_model: 'ESP32-C6',
      chip_revision: 1,
      cpu_cores: 1,
      flash_mb: 4,
      heap_used_percent: 37,
      mac_address: '84:F7:03:2B:A7:C2'
    },
    storage: {
      flash: {
        total_bytes: 4194304,
        fixed_regions: {
          bootloader_reserved_bytes: 32768,
          partition_table_bytes: 4096
        },
        partitions: [
          { id: 'nvs', type: 'data', subtype: 'nvs', offset_bytes: 36864, size_bytes: 20480 },
          { id: 'otadata', type: 'data', subtype: 'ota', offset_bytes: 57344, size_bytes: 8192 },
          { id: 'app0', type: 'app', subtype: 'ota_0', offset_bytes: 65536, size_bytes: 2031616 },
          { id: 'userdata', type: 'data', subtype: 'spiffs', offset_bytes: 2097152, size_bytes: 1998848 },
          { id: 'user_nvs', type: 'data', subtype: 'nvs', offset_bytes: 4096000, size_bytes: 32768 },
          { id: 'coredump', type: 'data', subtype: 'coredump', offset_bytes: 4128768, size_bytes: 65536 }
        ]
      },
      app: {
        partition_id: 'app0',
        frontend_bundled: true,
        capacity: {
          total_bytes: 2031616,
          firmware_image_bytes: 1257440,
          frontend_payload_bytes: 39189,
          available_bytes: 774176
        }
      },
      user: {
        partition_id: 'userdata',
        filesystem: 'littlefs',
        mounted: true,
        capabilities: { file_upload: false },
        capacity: {
          total_bytes: 1933312,
          used_bytes: 327680,
          available_bytes: 1605632
        },
        limits: {
          max_upload_bytes: 1605632,
          reserved_bytes: 65536,
          allocation_unit_bytes: 4096
        }
      }
    },
    wifiConfig: clone(initialWifiConfig),
    staPassword: 'WiFi pass!',
    adminPassword: DEFAULT_ADMIN_PASSWORD,
    activeToken: '',
    nextTokenId: 1,
    runtime: {
      mode: 'ap_sta',
      sta: { enabled: true, state: 'connected', ip: '192.168.50.86' },
      ap: { enabled: true, state: 'active', ip: DEFAULT_AP_ADDRESS }
    },
    applyGeneration: 0,
    connectionGeneration: 0,
    wifiTransitionActive: false,
    wifiConnection: {
      state: 'idle', failure_code: 'none', ip: '0.0.0.0', ap_shutdown_in_seconds: 0
    },
    resetGeneration: 0,
    scanBusy: false,
    lastScanCompletedAt: 0,
    networks: [
      { ssid: 'Studio-WiFi', rssi: -42, channel: 6, encryption_type: 4, encryption: 'wpa2', hidden: false },
      { ssid: 'Pixel-Lab', rssi: -51, channel: 1, encryption_type: 5, encryption: 'wpa3', hidden: false },
      { ssid: 'Office-IoT', rssi: -59, channel: 11, encryption_type: 4, encryption: 'wpa2', hidden: false },
      { ssid: 'Guest', rssi: -53, channel: 1, encryption_type: 0, encryption: 'open', hidden: false },
      { ssid: 'Weak-Neighbor', rssi: -78, channel: 3, encryption_type: 4, encryption: 'wpa2', hidden: false },
      { ssid: 'Distant-Cafe', rssi: -88, channel: 9, encryption_type: 4, encryption: 'wpa2', hidden: false },
      { ssid: 'Hidden-Lab', rssi: -46, channel: 7, encryption_type: 4, encryption: 'wpa2', hidden: true },
      { ssid: '', rssi: -38, channel: 10, encryption_type: 4, encryption: 'wpa2', hidden: true },
      ...Array.from({ length: 18 }, (_, index) => ({
        ssid: `Lab-Device-${String(index + 1).padStart(2, '0')}`,
        rssi: -54 - index,
        channel: index % 11 + 1,
        encryption_type: 4,
        encryption: 'wpa2',
        hidden: false
      }))
    ]
  };
  async function dispatch(request) {
    const route = `${request.method} ${request.path}`;
    switch (route) {
      case 'GET /api/alive':
        return success({}, 'ok');
      case 'GET /api/device':
        return success(preview.device);
      case 'GET /api/storage':
        return success(preview.storage);
      case 'GET /api/wifi':
        return success(wifiResponse());
      case 'GET /api/auth':
        return success({ username: 'admin' });
      case 'POST /api/auth/login':
        return login(request);
      case 'POST /api/auth/verify':
        return verifyCredentials(request);
      case 'GET /api/auth/session':
        return isAuthorized(request) ? success({ authenticated: true }, 'authenticated') : unauthorized();
      case 'POST /api/auth/logout':
        if (!isAuthorized(request)) return unauthorized();
        preview.activeToken = '';
        return success({}, 'logged out');
      case 'PUT /api/auth/password':
        return isAuthorized(request) ? updateAdminPassword(request) : unauthorized();
      case 'GET /api/wifi/scan':
        return isAuthorized(request) ? scanNetworks() : unauthorized();
      case 'POST /api/wifi/connect':
        return isAuthorized(request) ? connectWifi(request) : unauthorized();
      case 'GET /api/wifi/connect':
        return isAuthorized(request) ? wifiConnectionStatus() : unauthorized();
      case 'PUT /api/wifi':
        return isAuthorized(request) ? updateWifi(request) : unauthorized();
      case 'POST /api/wifi/reconnect':
        if (!isAuthorized(request)) return unauthorized();
        if (wifiConnectionBlocksRadio()) {
          return problem(409, 'wifi_connect_busy', 'wifi connection is using the radio');
        }
        scheduleWifiApply();
        return success({}, 'wifi reconnecting');
      case 'PUT /api/system':
        return isAuthorized(request) ? updateSystem(request) : unauthorized();
      case 'POST /api/system/reset':
        if (!isAuthorized(request)) return unauthorized();
        scheduleReset('all');
        return success({}, 'reset scheduled; restarting');
      case 'POST /api/system/reset/settings':
        if (!isAuthorized(request)) return unauthorized();
        scheduleReset('settings');
        return success({}, 'reset scheduled; restarting');
      case 'POST /api/system/reset/data':
        if (!isAuthorized(request)) return unauthorized();
        scheduleReset('data');
        return success({}, 'reset scheduled; restarting');
      default:
        return problem(404, 'not_found', 'not found');
    }
  }
  function login(request) {
    const decoded = credentialsFromRequest(request);
    if (decoded.error) return decoded.error;
    if (decoded.value.username !== 'admin' || decoded.value.password !== preview.adminPassword) {
      return failure(401, { code: 'unauthorized', authenticated: false }, 'invalid credentials');
    }
    preview.activeToken = `${PREVIEW_TOKEN_PREFIX}${preview.nextTokenId++}`;
    return success({ authenticated: true, token_type: 'Bearer', token: preview.activeToken }, 'authenticated');
  }

  function verifyCredentials(request) {
    const decoded = credentialsFromRequest(request);
    if (decoded.error) return decoded.error;
    const authenticated = decoded.value.username === 'admin' && decoded.value.password === preview.adminPassword;
    return success({ authenticated }, authenticated ? 'credentials valid' : 'credentials invalid');
  }

  function credentialsFromRequest(request) {
    const parsed = parseObjectBody(request);
    if (parsed.error) return parsed;
    const shapeError = validateObject(parsed.value, {
      username: { type: 'string', required: true },
      password: { type: 'string', required: true }
    });
    return shapeError ? { error: issueResponse(shapeError) } : { value: parsed.value };
  }

  function updateAdminPassword(request) {
    const parsed = parseObjectBody(request);
    if (parsed.error) return parsed.error;
    const shapeError = validateObject(parsed.value, { password: { type: 'string', required: true } });
    if (shapeError) return issueResponse(shapeError);
    const passwordError = validatePassword(parsed.value.password, 'password', true);
    if (passwordError) return issueResponse(passwordError);

    preview.adminPassword = parsed.value.password;
    preview.activeToken = '';
    if (preview.wifiConfig.interfaces.ap.password_enabled) scheduleDeviceRestart();
    updatePreviewUi('changed');
    return success({ session: 'invalidated' }, 'admin password updated');
  }
  async function scanNetworks() {
    if (wifiConnectionBlocksRadio()) return problem(409, 'wifi_connect_busy', 'wifi connection is using the radio');
    if (preview.runtime.sta.state === 'connecting') {
      return failure(409, { code: 'wifi_scan_busy', retry_after_seconds: 1 }, 'wifi connection is using the radio');
    }
    const now = Date.now();
    if (preview.scanBusy) return rateLimited(10);
    const elapsed = now - preview.lastScanCompletedAt;
    if (preview.lastScanCompletedAt && elapsed < SCAN_COOLDOWN_MS) {
      return rateLimited(Math.ceil((SCAN_COOLDOWN_MS - elapsed) / 1000));
    }

    preview.scanBusy = true;
    await delay(420);
    preview.scanBusy = false;
    preview.lastScanCompletedAt = Date.now();
    const networks = preview.networks
      .filter((network) => Number(network.rssi) > -75)
      .sort((left, right) => Number(right.rssi) - Number(left.rssi))
      .slice(0, 20);
    return success({ networks });
  }

  function updateWifi(request) {
    const parsed = parseObjectBody(request);
    if (parsed.error) return parsed.error;
    const validated = validateWifiPayload(parsed.value);
    if (validated.error) return issueResponse(validated.error);
    if (wifiConnectionBlocksRadio()) {
      return problem(409, 'wifi_connect_busy', 'wifi connection is using the radio');
    }

    if (preview.wifiConfig.mode === 'ap'
      && (validated.value.config.mode === 'sta' || validated.value.config.mode === 'ap_sta')) {
      if (!preview.runtime.ap.enabled || preview.runtime.ap.state !== 'active') {
        return problem(409, 'wifi_connect_requires_ap', 'wifi connection requires an active management AP');
      }
      return startSafeWifiTransition(
        validated.value.config,
        validated.value.staPassword,
        'wifi transition started'
      );
    }

    const apProtectionChanged = preview.wifiConfig.interfaces.ap.password_enabled
      !== validated.value.config.interfaces.ap.password_enabled;
    resetWifiConnection();
    preview.wifiConfig = validated.value.config;
    preview.staPassword = validated.value.staPassword;
    if (apProtectionChanged) scheduleDeviceRestart();
    else scheduleWifiApply();
    return success({}, 'wifi updated');
  }

  function connectWifi(request) {
    if (wifiConnectionBlocksRadio()) return problem(409, 'wifi_connect_busy', 'wifi connection already in progress');
    if (!preview.runtime.ap.enabled || preview.runtime.ap.state !== 'active') {
      return problem(409, 'wifi_connect_requires_ap', 'wifi connection requires an active management AP');
    }
    const parsed = parseObjectBody(request);
    if (parsed.error) return parsed.error;
    const shapeError = validateObject(parsed.value, {
      ssid: { type: 'string', required: true },
      password: { type: 'string', required: false }
    });
    if (shapeError) return issueResponse(shapeError);
    const ssid = parsed.value.ssid;
    const passwordProvided = Object.prototype.hasOwnProperty.call(parsed.value, 'password');
    const savedSta = preview.wifiConfig.interfaces.sta;
    const password = passwordProvided
      ? parsed.value.password
      : ssid === savedSta.ssid && savedSta.security === 'wpa' ? preview.staPassword : '';
    if (!ssid || ssid.length > 32) return problem(400, 'invalid_field', 'invalid SSID', ['ssid']);
    if (password && (password.length < 8 || password.length > 63)) {
      return problem(400, 'invalid_field', 'invalid password', ['password']);
    }

    const candidate = clone(preview.wifiConfig);
    candidate.mode = 'ap_sta';
    candidate.fallback_to_ap = true;
    candidate.interfaces.sta = {
        ssid,
        security: password ? 'wpa' : 'open',
        ip_config: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
    };
    return startSafeWifiTransition(candidate, password, 'wifi connection started');
  }

  function startSafeWifiTransition(candidate, staPassword, message) {
    const previousAp = clone(preview.runtime.ap);
    const generation = ++preview.connectionGeneration;
    preview.wifiTransitionActive = true;
    preview.wifiConnection = {
      state: 'connecting', failure_code: 'none', ip: '0.0.0.0', ap_shutdown_in_seconds: 0
    };
    preview.runtime = {
      mode: 'ap_sta',
      sta: { enabled: true, state: 'connecting', ip: '0.0.0.0' },
      ap: previousAp
    };
    window.setTimeout(() => {
      if (generation !== preview.connectionGeneration) return;
      const staticIp = candidate.interfaces.sta.ip_config.mode === 'static';
      const staIp = staticIp ? candidate.interfaces.sta.ip_config.address : '192.168.50.86';
      preview.wifiConfig = clone(candidate);
      preview.staPassword = staPassword;

      if (candidate.mode === 'sta') {
        preview.wifiConnection = {
          state: 'connected', failure_code: 'none', ip: staIp, ap_shutdown_in_seconds: 5
        };
        preview.runtime = {
          mode: 'ap_sta',
          sta: { enabled: true, state: 'connected', ip: staIp },
          ap: previousAp
        };
        window.setTimeout(() => {
          if (generation !== preview.connectionGeneration) return;
          preview.wifiTransitionActive = false;
          preview.wifiConnection.ap_shutdown_in_seconds = 0;
          preview.runtime = {
            mode: 'sta',
            sta: { enabled: true, state: 'connected', ip: staIp },
            ap: { enabled: false, state: 'disabled', ip: '0.0.0.0' }
          };
        }, 5000);
        return;
      }

      preview.wifiTransitionActive = false;
      preview.wifiConnection = {
        state: 'connected', failure_code: 'none', ip: staIp, ap_shutdown_in_seconds: 0
      };
      preview.runtime = {
        mode: 'ap_sta',
        sta: { enabled: true, state: 'connected', ip: staIp },
        ap: {
          enabled: true,
          state: 'active',
          ip: candidate.interfaces.ap.ip_config.address
        }
      };
    }, 900);
    return response(202, { success: true, data: { state: 'connecting' }, message });
  }

  function wifiConnectionStatus() {
    return success(clone(preview.wifiConnection));
  }

  function wifiConnectionBlocksRadio() {
    return preview.wifiTransitionActive;
  }

  function resetWifiConnection() {
    preview.connectionGeneration += 1;
    preview.wifiTransitionActive = false;
    preview.wifiConnection = {
      state: 'idle', failure_code: 'none', ip: '0.0.0.0', ap_shutdown_in_seconds: 0
    };
  }
  function updateSystem(request) {
    const parsed = parseObjectBody(request);
    if (parsed.error) return parsed.error;
    const shapeError = validateObject(parsed.value, { hostname: { type: 'string', required: true } });
    if (shapeError) return issueResponse(shapeError);
    const hostname = parsed.value.hostname;
    if (!hostname || hostname.length > 31 || !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,29}[A-Za-z0-9])?$/.test(hostname)) {
      return problem(400, 'invalid_field', 'invalid hostname', ['hostname']);
    }
    preview.device.hostname = hostname;
    scheduleWifiApply();
    return success({ hostname }, 'system updated');
  }
  function validateWifiPayload(body) {
    const stringError = validateStringTree(body);
    if (stringError) return { error: stringError };

    let error = validateObject(body, {
      mode: { type: 'string', required: true },
      fallback_to_ap: { type: 'boolean', required: true },
      interfaces: { type: 'object', required: true }
    });
    if (error) return { error };
    if (!['sta', 'ap', 'ap_sta'].includes(body.mode)) {
      return { error: issue('invalid_field', 'mode', 'mode must be sta, ap, or ap_sta') };
    }

    error = validateObject(body.interfaces, {
      sta: { type: 'object', required: true },
      ap: { type: 'object', required: true }
    }, 'interfaces');
    if (error) return { error };

    const sta = body.interfaces.sta;
    const ap = body.interfaces.ap;
    error = validateObject(sta, {
      ssid: { type: 'string', required: true },
      security: { type: 'string', required: true },
      password: { type: 'string', required: false },
      ip_config: { type: 'object', required: true }
    }, 'interfaces.sta');
    if (error) return { error };
    error = validateObject(ap, {
      ssid: { type: 'string', required: true },
      password_enabled: { type: 'boolean', required: true },
      ip_config: { type: 'object', required: true }
    }, 'interfaces.ap');
    if (error) return { error };

    const staRequired = body.mode === 'sta' || body.mode === 'ap_sta';
    error = validateSsid(sta.ssid, staRequired, 'interfaces.sta.ssid', 'STA');
    if (error) return { error };
    const apRequired = body.mode === 'ap' || body.mode === 'ap_sta' || (body.mode === 'sta' && body.fallback_to_ap);
    error = validateSsid(ap.ssid, apRequired, 'interfaces.ap.ssid', 'AP');
    if (error) return { error };
    if (!['wpa', 'open'].includes(sta.security)) {
      return { error: issue('invalid_field', 'interfaces.sta.security', 'invalid STA security') };
    }

    let staPassword = preview.staPassword;
    if (Object.prototype.hasOwnProperty.call(sta, 'password') && sta.password !== '********') {
      error = validatePassword(sta.password, 'interfaces.sta.password', false);
      if (error) return { error };
      staPassword = sta.password;
    }
    if (sta.security === 'open') staPassword = '';

    const staIpResult = validateStaIpConfig(sta.ip_config);
    if (staIpResult.error) return staIpResult;
    const apIpResult = validateApIpConfig(ap.ip_config);
    if (apIpResult.error) return apIpResult;

    const apCanRunWithSta = body.mode === 'ap_sta' || (body.mode === 'sta' && body.fallback_to_ap);
    if (apCanRunWithSta && staIpResult.value.mode === 'static' && subnetsOverlap(staIpResult.value, apIpResult.value)) {
      return { error: issue('subnet_overlap', 'interfaces', 'STA and AP subnets must not overlap') };
    }

    return {
      value: {
        staPassword,
        config: {
          mode: body.mode,
          fallback_to_ap: body.fallback_to_ap,
          interfaces: {
            sta: { ssid: sta.ssid, security: sta.security, ip_config: staIpResult.value },
            ap: { ssid: ap.ssid, password_enabled: ap.password_enabled, ip_config: apIpResult.value }
          }
        }
      }
    };
  }

  function validateStaIpConfig(ipConfig) {
    const error = validateObject(ipConfig, {
      mode: { type: 'string', required: true },
      address: { type: 'string', required: true },
      gateway: { type: 'string', required: true },
      netmask: { type: 'string', required: true },
      dns: { type: 'array', required: true }
    }, 'interfaces.sta.ip_config');
    if (error) return { error };
    if (!['dhcp', 'static'].includes(ipConfig.mode)) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config.mode', 'invalid STA IP mode') };
    }
    if (ipConfig.dns.length > 2 || ipConfig.dns.some((value) => typeof value !== 'string')) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config.dns', 'invalid STA DNS') };
    }
    if (ipConfig.mode === 'dhcp') {
      if (ipConfig.address || ipConfig.gateway || ipConfig.netmask || ipConfig.dns.length) {
        return { error: issue('invalid_field', 'interfaces.sta.ip_config', 'STA DHCP fields must be empty') };
      }
      return { value: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] } };
    }

    const address = parseIpv4(ipConfig.address);
    const gateway = parseIpv4(ipConfig.gateway);
    const netmask = parseIpv4(ipConfig.netmask);
    if (!address) return { error: issue('invalid_field', 'interfaces.sta.ip_config.address', 'invalid STA static IP address') };
    if (!gateway) return { error: issue('invalid_field', 'interfaces.sta.ip_config.gateway', 'invalid STA static gateway') };
    if (!netmask || !isUsableNetmask(netmask)) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config.netmask', 'invalid STA static netmask') };
    }
    if (!isUsableHost(address, netmask)) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config.address', 'invalid STA static IP address') };
    }
    const gatewayOutsideSubnet = (
      subnetStart(address, netmask) !== subnetStart(gateway, netmask)
    );
    const gatewayMatchesAddress = ipv4Number(address) === ipv4Number(gateway);
    if (
      !isUsableHost(gateway, netmask) ||
      gatewayOutsideSubnet ||
      gatewayMatchesAddress
    ) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config', 'STA gateway must be a different host in the STA subnet') };
    }
    if (ipConfig.dns.some((value) => !parseIpv4(value) || ipv4Number(parseIpv4(value)) === 0)) {
      return { error: issue('invalid_field', 'interfaces.sta.ip_config.dns', 'invalid STA DNS') };
    }
    return { value: clone(ipConfig) };
  }

  function validateApIpConfig(ipConfig) {
    const error = validateObject(ipConfig, {
      mode: { type: 'string', required: true },
      address: { type: 'string', required: false },
      netmask: { type: 'string', required: false }
    }, 'interfaces.ap.ip_config');
    if (error) return { error };
    if (!['default', 'static'].includes(ipConfig.mode)) {
      return { error: issue('invalid_field', 'interfaces.ap.ip_config.mode', 'invalid AP IP mode') };
    }
    if (ipConfig.mode === 'default') {
      if (Object.prototype.hasOwnProperty.call(ipConfig, 'address') || Object.prototype.hasOwnProperty.call(ipConfig, 'netmask')) {
        return { error: issue('invalid_field', 'interfaces.ap.ip_config', 'AP default mode does not accept address or netmask') };
      }
      return { value: { mode: 'default', address: DEFAULT_AP_ADDRESS, netmask: DEFAULT_AP_NETMASK } };
    }
    if (!Object.prototype.hasOwnProperty.call(ipConfig, 'address') || !Object.prototype.hasOwnProperty.call(ipConfig, 'netmask')) {
      return { error: issue('missing_field', 'interfaces.ap.ip_config', 'AP static address and netmask are required') };
    }
    const address = parseIpv4(ipConfig.address);
    const netmask = parseIpv4(ipConfig.netmask);
    if (!address || !netmask || !isUsableNetmask(netmask) || !isUsableHost(address, netmask)) {
      return { error: issue('invalid_field', 'interfaces.ap.ip_config', 'invalid AP static IPv4') };
    }
    const prefix = netmaskPrefix(netmask);
    if (prefix < 24 || prefix > 28) {
      return { error: issue('invalid_field', 'interfaces.ap.ip_config.netmask', 'AP netmask must be between /24 and /28') };
    }
    return { value: clone(ipConfig) };
  }
  function scheduleWifiApply() {
    const generation = ++preview.applyGeneration;
    const applyWhenAvailable = () => {
      if (generation !== preview.applyGeneration) return;
      if (preview.wifiTransitionActive) {
        window.setTimeout(applyWhenAvailable, 100);
        return;
      }
      const config = preview.wifiConfig;
      const hasSta = config.mode === 'sta' || config.mode === 'ap_sta';
      const hasAp = config.mode === 'ap' || config.mode === 'ap_sta';
      preview.runtime = {
        mode: config.mode,
        sta: { enabled: hasSta, state: hasSta ? 'connecting' : 'disabled', ip: '0.0.0.0' },
        ap: { enabled: hasAp, state: hasAp ? 'starting' : 'disabled', ip: hasAp ? config.interfaces.ap.ip_config.address : '0.0.0.0' }
      };
      window.setTimeout(() => settleWifiApply(generation), 650);
    };
    window.setTimeout(applyWhenAvailable, 100);
  }

  function settleWifiApply(generation) {
    if (generation !== preview.applyGeneration) return;
    const config = preview.wifiConfig;
    const hasSta = config.mode === 'sta' || config.mode === 'ap_sta';
    const hasAp = config.mode === 'ap' || config.mode === 'ap_sta';
    const staUsesStaticIp = config.interfaces.sta.ip_config.mode === 'static';
    const connectedStaIp = staUsesStaticIp
      ? config.interfaces.sta.ip_config.address
      : '192.168.50.86';
    preview.runtime = {
      mode: config.mode,
      sta: {
        enabled: hasSta,
        state: hasSta ? 'connected' : 'disabled',
        ip: hasSta ? connectedStaIp : '0.0.0.0'
      },
      ap: {
        enabled: hasAp,
        state: hasAp ? 'active' : 'disabled',
        ip: hasAp ? config.interfaces.ap.ip_config.address : '0.0.0.0'
      }
    };
  }

  function scheduleDeviceRestart() {
    const generation = ++preview.resetGeneration;
    window.setTimeout(() => {
      if (generation !== preview.resetGeneration) return;
      preview.applyGeneration += 1;
      resetWifiConnection();
      preview.activeToken = '';
      scheduleWifiApply();
      updatePreviewUi('changed');
    }, 300);
  }

  function scheduleReset(scope) {
    const generation = ++preview.resetGeneration;
    window.setTimeout(() => {
      if (generation !== preview.resetGeneration) return;
      preview.applyGeneration += 1;
      preview.connectionGeneration += 1;
      preview.wifiTransitionActive = false;
      preview.activeToken = '';
      if (scope === 'settings' || scope === 'all') {
        preview.device.hostname = 'esp32-device';
        preview.wifiConfig = clone(factoryWifiConfig);
        preview.staPassword = '';
        preview.adminPassword = DEFAULT_ADMIN_PASSWORD;
        preview.runtime = {
          mode: 'ap',
          sta: { enabled: false, state: 'disabled', ip: '0.0.0.0' },
          ap: { enabled: true, state: 'active', ip: DEFAULT_AP_ADDRESS }
        };
      }
      if (scope === 'data' || scope === 'all') {
        preview.storage.user.capacity.used_bytes = 0;
        preview.storage.user.capacity.available_bytes = preview.storage.user.capacity.total_bytes;
        preview.storage.user.limits.max_upload_bytes = preview.storage.user.capacity.total_bytes;
      }
      preview.lastScanCompletedAt = 0;
      preview.wifiConnection = {
        state: 'idle', failure_code: 'none', ip: '0.0.0.0', ap_shutdown_in_seconds: 0
      };
      updatePreviewUi('default');
    }, 300);
  }

  function wifiResponse() {
    const config = preview.wifiConfig;
    return {
      mode: preview.runtime.mode,
      configured_mode: config.mode,
      fallback_to_ap: config.fallback_to_ap,
      interfaces: {
        sta: {
          enabled: preview.runtime.sta.enabled,
          ssid: config.interfaces.sta.ssid,
          security: config.interfaces.sta.security,
          state: preview.runtime.sta.state,
          ip: preview.runtime.sta.ip,
          ip_config: clone(config.interfaces.sta.ip_config)
        },
        ap: {
          enabled: preview.runtime.ap.enabled,
          state: preview.runtime.ap.state,
          ssid: config.interfaces.ap.ssid,
          password_enabled: config.interfaces.ap.password_enabled,
          ip: preview.runtime.ap.ip,
          ip_config: clone(config.interfaces.ap.ip_config)
        }
      }
    };
  }
  function parseObjectBody(request) {
    if (request.body === undefined || request.body === '') {
      return { error: problem(400, 'invalid_json', 'JSON body is required') };
    }
    let value;
    try {
      value = JSON.parse(request.body);
    } catch (_) {
      return { error: problem(400, 'invalid_json', 'invalid JSON') };
    }
    if (!isObject(value)) return { error: problem(400, 'invalid_json', 'JSON body must be an object') };
    return { value };
  }

  function validateObject(value, fields, path = '') {
    if (!isObject(value)) return issue('invalid_field', path, 'field must be an object');
    for (const [name, rule] of Object.entries(fields)) {
      const field = path ? `${path}.${name}` : name;
      if (!Object.prototype.hasOwnProperty.call(value, name)) {
        if (rule.required) return issue('missing_field', field, `${name} is required`);
        continue;
      }
      if (!isType(value[name], rule.type)) return issue('invalid_field', field, `field must be a ${rule.type}`);
    }
    for (const name of Object.keys(value)) {
      if (!Object.prototype.hasOwnProperty.call(fields, name)) {
        return issue('unsupported_field', path ? `${path}.${name}` : name, 'unsupported field');
      }
    }
    return null;
  }

  function validateStringTree(value, depth = 0) {
    if (depth > 8) return issue('invalid_field', '', 'JSON body is too nested');
    if (typeof value === 'string') {
      if (value.length > 128) return issue('invalid_field', '', 'string too long');
      if (!isPrintableAscii(value)) return issue('invalid_field', '', 'string has unsupported characters');
      return null;
    }
    if (Array.isArray(value)) {
      for (const item of value) {
        const error = validateStringTree(item, depth + 1);
        if (error) return error;
      }
    } else if (isObject(value)) {
      for (const item of Object.values(value)) {
        const error = validateStringTree(item, depth + 1);
        if (error) return error;
      }
    }
    return null;
  }

  function validateSsid(value, required, field, label) {
    if (!value) return required ? issue('invalid_field', field, `${label} SSID is required for selected Wi-Fi mode`) : null;
    if (value.length > 32 || !isPrintableAscii(value)) return issue('invalid_field', field, `invalid ${label} SSID`);
    return null;
  }

  function validatePassword(value, field, required) {
    if (!value) return required ? issue('invalid_field', field, 'password is required') : null;
    if (value.length < 8 || value.length > 63 || !isPrintableAscii(value)) {
      return issue('invalid_field', field, 'password must be 8-63 printable ASCII characters');
    }
    return null;
  }

  function parseIpv4(value) {
    if (typeof value !== 'string') return null;
    const parts = value.split('.');
    if (parts.length !== 4 || parts.some((part) => !/^\d{1,3}$/.test(part) || Number(part) > 255)) return null;
    return parts.map(Number);
  }

  function ipv4Number(parts) {
    return (((parts[0] * 256 + parts[1]) * 256 + parts[2]) * 256 + parts[3]) >>> 0;
  }

  function isUsableNetmask(parts) {
    const mask = ipv4Number(parts);
    const host = (~mask) >>> 0;
    return mask !== 0 && mask !== 0xffffffff && host >= 3 && (host & ((host + 1) >>> 0)) === 0;
  }

  function netmaskPrefix(parts) {
    let mask = ipv4Number(parts);
    let prefix = 0;
    while ((mask & 0x80000000) !== 0) {
      prefix += 1;
      mask = (mask << 1) >>> 0;
    }
    return prefix;
  }

  function subnetStart(address, netmask) {
    return (ipv4Number(address) & ipv4Number(netmask)) >>> 0;
  }

  function isUsableHost(address, netmask) {
    const ip = ipv4Number(address);
    const mask = ipv4Number(netmask);
    const network = (ip & mask) >>> 0;
    const broadcast = (network | (~mask)) >>> 0;
    return ip !== 0 && ip !== 0xffffffff && ip !== network && ip !== broadcast;
  }

  function subnetsOverlap(staIp, apIp) {
    const staAddress = parseIpv4(staIp.address);
    const staNetmask = parseIpv4(staIp.netmask);
    const apAddress = parseIpv4(apIp.address);
    const apNetmask = parseIpv4(apIp.netmask);
    const staStart = subnetStart(staAddress, staNetmask);
    const apStart = subnetStart(apAddress, apNetmask);
    const staEnd = (staStart | (~ipv4Number(staNetmask))) >>> 0;
    const apEnd = (apStart | (~ipv4Number(apNetmask))) >>> 0;
    return staStart <= apEnd && apStart <= staEnd;
  }
  function initializeSession() {
    const autoSession = ['settings', 'admin', 'system', 'scan'].includes(previewView);
    if (autoSession) {
      preview.activeToken = `${PREVIEW_TOKEN_PREFIX}auto`;
      localStorage.setItem('auth.token', preview.activeToken);
      return;
    }
    if (previewView === 'login') {
      localStorage.removeItem('auth.token');
      return;
    }
    const storedToken = localStorage.getItem('auth.token') || '';
    if (storedToken.startsWith(PREVIEW_TOKEN_PREFIX)) preview.activeToken = storedToken;
  }
  async function normalizeRequest(input, options) {
    const rawUrl = typeof input === 'string' ? input : input.url;
    const path = rawUrl.startsWith('/') ? rawUrl.split(/[?#]/, 1)[0] : new URL(rawUrl, window.location.href).pathname;
    const headers = new Headers(typeof input === 'string' ? undefined : input.headers);
    new Headers(options.headers || {}).forEach((value, name) => headers.set(name, value));
    let body = options.body;
    if (body === undefined && typeof input !== 'string' && input.body) body = await input.clone().text();
    return {
      path,
      method: String(options.method || (typeof input === 'string' ? 'GET' : input.method) || 'GET').toUpperCase(),
      headers,
      body
    };
  }

  function isAuthorized(request) {
    const authorization = request.headers.get('Authorization') || '';
    return Boolean(preview.activeToken) && authorization === `Bearer ${preview.activeToken}`;
  }

  function unauthorized() {
    return failure(401, { code: 'unauthorized', authenticated: false }, 'unauthorized');
  }

  function rateLimited(seconds) {
    return failure(429, { code: 'rate_limited', retry_after_seconds: Math.max(1, seconds) }, 'wifi scan rate limited');
  }

  function issue(code, field, message) {
    return { code, field, message };
  }

  function issueResponse(error) {
    return problem(400, error.code, error.message, error.field ? [error.field] : []);
  }

  function problem(status, code, message, fields = []) {
    const data = { code };
    if (fields.length) data.fields = fields;
    return failure(status, data, message);
  }

  function success(data, message = 'ok') {
    return response(200, { success: true, data: clone(data), message });
  }

  function failure(status, data, message) {
    return response(status, { success: false, data: clone(data), message });
  }

  function response(status, payload) {
    return new Response(JSON.stringify(payload), {
      status,
      headers: { 'Content-Type': 'application/json' }
    });
  }

  function isObject(value) {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
  }

  function isType(value, type) {
    if (type === 'object') return isObject(value);
    if (type === 'array') return Array.isArray(value);
    return typeof value === type;
  }

  function isPrintableAscii(value) {
    return /^[\x20-\x7E]*$/.test(value);
  }

  function delay(milliseconds) {
    return new Promise((resolve) => window.setTimeout(resolve, milliseconds));
  }

  function clone(value) {
    return JSON.parse(JSON.stringify(value));
  }
  function openPreviewView() {
    if (previewView === 'details' || previewView === 'hardware') document.querySelector('[data-page="hardware"]').click();
    if (previewView === 'menu') {
      document.getElementById('menuButton').click();
      document.getElementById('languageMenuItem').dispatchEvent(new MouseEvent('mouseenter'));
    }
    if (previewView === 'login') document.querySelector('[data-page="settings"]').click();
    if (['settings', 'admin', 'system', 'scan'].includes(previewView)) {
      document.querySelector('[data-page="settings"]').click();
      if (previewView === 'admin' || previewView === 'system') {
        window.setTimeout(() => document.querySelector(`[data-settings-page="${previewView}"]`).click(), 250);
      }
      if (previewView === 'scan') window.setTimeout(() => document.getElementById('chooseNetworkButton').click(), 250);
    }
  }

  function previewCopy() {
    const language = localStorage.getItem('ui.lang') === 'zh-Hant' ? 'zh-Hant' : 'en';
    const copy = {
      en: {
        badge: 'PREVIEW',
        title: 'Preview mode',
        openDetails: 'Open preview details',
        default: 'Mock device data',
        changed: 'The admin password was changed for this session.',
        reset: 'Mock device data · Demo reset',
        username: 'Username',
        password: 'Password',
        resetButton: 'Reset demo',
        closeButton: 'Close',
        resetting: 'Resetting…',
        resetNotice: 'Demo reset. Default preview data restored.'
      },
      'zh-Hant': {
        badge: '假資料',
        title: '預覽模式',
        openDetails: '開啟預覽資訊',
        default: '模擬裝置資料',
        changed: '此工作階段的管理員密碼已變更。',
        reset: '模擬裝置資料 · Demo 已重設',
        username: '帳號',
        password: '密碼',
        resetButton: '重設 Demo',
        closeButton: '關閉',
        resetting: '重設中…',
        resetNotice: 'Demo 已重設，已恢復預設假資料。'
      }
    };
    return copy[language];
  }

  function previewCredential(label, value) {
    const row = document.createElement('div');
    row.className = 'preview-credential';
    const name = document.createElement('span');
    name.textContent = label;
    const code = document.createElement('code');
    code.textContent = value;
    row.append(name, code);
    return row;
  }

  function showPreviewUi() {
    const topActions = document.querySelector('.top-actions');
    const refreshButton = document.getElementById('refreshButton');
    if (!topActions || !refreshButton) return;

    const badge = document.createElement('button');
    badge.className = 'preview-badge';
    badge.type = 'button';
    badge.setAttribute('aria-haspopup', 'dialog');
    badge.setAttribute('aria-controls', 'previewDialog');
    topActions.insertBefore(badge, refreshButton);

    const dialog = document.createElement('section');
    dialog.id = 'previewDialog';
    dialog.className = 'dialog confirm-dialog preview-dialog';
    dialog.hidden = true;
    dialog.dataset.initialFocus = 'closePreviewDialogButton';
    dialog.setAttribute('role', 'dialog');
    dialog.setAttribute('aria-modal', 'true');
    dialog.setAttribute('aria-labelledby', 'previewDialogTitle');
    const label = document.createElement('span');
    label.className = 'preview-dialog-label';
    const copyBlock = document.createElement('div');
    copyBlock.className = 'preview-dialog-copy';
    const title = document.createElement('strong');
    title.id = 'previewDialogTitle';
    const description = document.createElement('span');
    copyBlock.append(title, description);
    const credentials = document.createElement('div');
    credentials.className = 'preview-credentials';
    const resetButton = document.createElement('button');
    resetButton.className = 'preview-reset-button';
    resetButton.type = 'button';
    resetButton.addEventListener('click', resetPreview);
    const closeButton = document.createElement('button');
    closeButton.id = 'closePreviewDialogButton';
    closeButton.className = 'button';
    closeButton.type = 'button';
    closeButton.addEventListener('click', DeviceConsole.ui.dialog.close);
    const footer = document.createElement('footer');
    footer.append(resetButton, closeButton);
    dialog.append(label, copyBlock, credentials, footer);
    DeviceConsole.ui.dialog.add(dialog);
    badge.addEventListener('click', () => DeviceConsole.ui.dialog.open('previewDialog'));

    previewUi = { badge, dialog, label, title, description, credentials, resetButton, closeButton };
    const resetCompleted = sessionStorage.getItem(RESET_NOTICE_KEY) === 'true';
    sessionStorage.removeItem(RESET_NOTICE_KEY);
    updatePreviewUi(resetCompleted ? 'reset' : 'default');
    if (resetCompleted) {
      window.setTimeout(() => setTransientNotice(previewCopy().resetNotice), 500);
    }
  }

  function updatePreviewUi(state) {
    if (!previewUi) return;
    previewUiState = state || previewUiState;
    const messages = previewCopy();
    const description = messages[previewUiState] || messages.default;
    previewUi.badge.textContent = messages.badge;
    previewUi.badge.title = messages.openDetails;
    previewUi.badge.setAttribute('aria-label', messages.openDetails);
    previewUi.label.textContent = messages.badge;
    previewUi.title.textContent = messages.title;
    previewUi.description.textContent = description;
    previewUi.credentials.hidden = previewUiState === 'changed';
    previewUi.credentials.replaceChildren(
      previewCredential(messages.username, 'admin'),
      previewCredential(messages.password, DEFAULT_ADMIN_PASSWORD)
    );
    previewUi.resetButton.textContent = messages.resetButton;
    previewUi.closeButton.textContent = messages.closeButton;
  }

  function resetPreview() {
    const resetButton = previewUi && previewUi.resetButton;
    if (resetButton) {
      resetButton.disabled = true;
      resetButton.textContent = previewCopy().resetting;
    }
    sessionStorage.setItem(RESET_NOTICE_KEY, 'true');
    localStorage.removeItem('auth.token');
    const url = new URL(window.location.href);
    url.searchParams.delete('view');
    url.hash = '#/network';
    window.setTimeout(() => {
      if (url.toString() === window.location.href) window.location.reload();
      else window.location.replace(url.toString());
    }, 50);
  }
  initializeSession();

  window.fetch = async function previewFetch(input, options = {}) {
    const request = await normalizeRequest(input, options);
    if (!request.path.startsWith('/api/')) return realFetch(input, options);
    await delay(request.method === 'GET' ? 90 : 160);
    return dispatch(request);
  };

  window.addEventListener('device-console-language-change', () => updatePreviewUi());
  window.addEventListener('load', () => {
    showPreviewUi();
    window.setTimeout(openPreviewView, 300);
  });
})();
