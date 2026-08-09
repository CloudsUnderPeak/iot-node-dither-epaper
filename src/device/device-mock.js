(function (app) {
    // 開發／demo 用假裝置 API adapter，由 index.html 的 PREVIEW 區塊載入。
    // 正式 build（make build）會剝除該區塊並排除本檔，裝置上不存在 mock；
    // 因此仿 builtin-web preview/config.js：本檔有被載入即預設啟用，?mock=0 可關閉。
    //
    // 測試腳本：登入帳密 admin / password；假裝置初始為 AP + STA（STA 已連線）。
    // safe transition（含驗證失敗）需先存成 AP mode 再切回含 STA 的 mode，
    // 掃描結果選「Mock-Fail」即模擬驗證失敗（其餘 SSID 皆成功）。
    var queryValue = /[?&]mock=([^&]*)/.exec(window.location.search);
    if (queryValue && ['0', 'false', 'off'].indexOf(queryValue[1].toLowerCase()) !== -1) {
        return;
    }

    var TRANSITION_CONNECT_MS = 4000;
    var LATENCY_MIN_MS = 120;
    var LATENCY_MAX_MS = 320;

    var state = {
        password: 'password',
        token: '',
        hostname: 'esp32-device',
        heapUsedPercent: 24,
        wifi: {
            mode: 'ap_sta',
            fallbackToAp: true,
            sta: {
                ssid: 'HomeWiFi-5G',
                security: 'wpa',
                password: 'mock-password',
                state: 'connected',
                ip: '192.168.1.50',
                ipConfig: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
            },
            ap: {
                enabled: true,
                state: 'active',
                ssid: 'esp32-device-A1B2',
                passwordEnabled: false,
                ip: '192.168.4.1',
                ipConfig: { mode: 'default', address: '192.168.4.1', netmask: '255.255.255.0' }
            }
        },
        // {startedAt, payload, fail} — PUT /api/wifi 的 202 safe transition。
        transition: null,
        epaper: { operation: null, stored: false, lastSource: null }
    };

    var SCAN_NETWORKS = [
        { ssid: 'HomeWiFi-5G', rssi: -52, channel: 6, encryption_type: 4, encryption: 'wpa2', hidden: false },
        { ssid: 'Mock-Fail', rssi: -58, channel: 1, encryption_type: 4, encryption: 'wpa2', hidden: false },
        { ssid: 'Office-2F', rssi: -61, channel: 11, encryption_type: 4, encryption: 'wpa2', hidden: false },
        { ssid: 'Cafe-Guest', rssi: -70, channel: 3, encryption_type: 0, encryption: 'open', hidden: false },
        { ssid: 'Far-Away', rssi: -82, channel: 9, encryption_type: 4, encryption: 'wpa2', hidden: false }
    ];

    function jsonResponse(status, body) {
        return new Response(JSON.stringify(body), {
            status: status,
            headers: { 'Content-Type': 'application/json' }
        });
    }

    function ok(data, message) {
        return jsonResponse(200, { success: true, data: data || {}, message: message || 'ok' });
    }

    function fail(status, code, message, extra) {
        var data = Object.assign({ code: code }, extra || {});
        return jsonResponse(status, { success: false, data: data, message: message });
    }

    function bearerToken(init) {
        var headers = (init && init.headers) || {};
        var value = headers.Authorization || headers.authorization || '';
        return value.indexOf('Bearer ') === 0 ? value.slice(7) : '';
    }

    function requireAuth(init) {
        return !!state.token && bearerToken(init) === state.token;
    }

    function parseJson(init) {
        try {
            return init && init.body ? JSON.parse(init.body) : null;
        } catch (error) {
            return null;
        }
    }

    function randomToken() {
        return 'mock-' + Math.random().toString(36).slice(2, 12);
    }

    // 依 transition 進度推進 Wi-Fi 狀態；terminal 結果由查詢時套用。
    function settleTransition() {
        var transition = state.transition;
        if (!transition) {
            return null;
        }
        if (Date.now() - transition.startedAt < TRANSITION_CONNECT_MS) {
            return { state: 'connecting', failure_code: 'none', ip: '', ap_shutdown_in_seconds: 0 };
        }
        state.transition = null;
        if (transition.fail) {
            return { state: 'failed', failure_code: 'station_disconnected', ip: '', ap_shutdown_in_seconds: 0 };
        }
        applyWifiPayload(transition.payload);
        state.wifi.sta.state = 'connected';
        state.wifi.sta.ip = '192.168.1.50';
        return {
            state: 'connected',
            failure_code: 'none',
            ip: '192.168.1.50',
            ap_shutdown_in_seconds: transition.payload.mode === 'sta' ? 5 : 0
        };
    }

    function applyWifiPayload(payload) {
        var sta = payload.interfaces.sta || {};
        var staIp = sta.ip_config || {};
        var apData = payload.interfaces.ap || {};
        var apIp = apData.ip_config || {};
        state.wifi.mode = payload.mode;
        state.wifi.fallbackToAp = !!payload.fallback_to_ap;
        state.wifi.sta.ssid = sta.ssid || '';
        state.wifi.sta.security = sta.security === 'open' ? 'open' : 'wpa';
        if (sta.password !== undefined && sta.password !== '********') {
            state.wifi.sta.password = sta.password;
        }
        state.wifi.sta.ipConfig = {
            mode: staIp.mode === 'static' ? 'static' : 'dhcp',
            address: staIp.address || '',
            gateway: staIp.gateway || '',
            netmask: staIp.netmask || '',
            dns: staIp.dns || []
        };
        state.wifi.ap.ssid = apData.ssid || state.wifi.ap.ssid;
        state.wifi.ap.passwordEnabled = !!apData.password_enabled;
        state.wifi.ap.ipConfig = apIp.mode === 'static'
            ? { mode: 'static', address: apIp.address, netmask: apIp.netmask }
            : { mode: 'default', address: '192.168.4.1', netmask: '255.255.255.0' };
        state.wifi.ap.ip = state.wifi.ap.ipConfig.address;
        var hasSta = payload.mode !== 'ap';
        state.wifi.sta.state = hasSta ? 'connected' : 'disabled';
        state.wifi.sta.ip = hasSta ? '192.168.1.50' : '0.0.0.0';
        state.wifi.ap.enabled = payload.mode !== 'sta';
        state.wifi.ap.state = payload.mode !== 'sta' ? 'active' : 'inactive';
    }

    function wifiSnapshot() {
        var wifi = state.wifi;
        var runtimeMode = state.transition ? 'ap_sta' : wifi.mode;
        return {
            mode: runtimeMode,
            configured_mode: wifi.mode,
            fallback_to_ap: wifi.fallbackToAp,
            interfaces: {
                sta: {
                    enabled: wifi.mode !== 'ap',
                    ssid: wifi.sta.ssid,
                    security: wifi.sta.security,
                    state: state.transition ? 'connecting' : wifi.sta.state,
                    ip: wifi.sta.ip,
                    ip_config: {
                        mode: wifi.sta.ipConfig.mode,
                        address: wifi.sta.ipConfig.address,
                        gateway: wifi.sta.ipConfig.gateway,
                        netmask: wifi.sta.ipConfig.netmask,
                        dns: wifi.sta.ipConfig.dns
                    }
                },
                ap: {
                    enabled: wifi.ap.enabled,
                    state: wifi.ap.state,
                    ssid: wifi.ap.ssid,
                    password_enabled: wifi.ap.passwordEnabled,
                    ip: wifi.ap.ip,
                    ip_config: {
                        mode: wifi.ap.ipConfig.mode,
                        address: wifi.ap.ipConfig.address,
                        netmask: wifi.ap.ipConfig.netmask
                    }
                }
            }
        };
    }

    function storageSnapshot() {
        return {
            flash: {
                total_bytes: 4194304,
                fixed_regions: { bootloader_reserved_bytes: 32768, partition_table_bytes: 4096 },
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
                capabilities: { file_upload: true, file_list: true, file_download: true, file_delete: true },
                capacity: { total_bytes: 1933312, used_bytes: 327680, available_bytes: 1605632 },
                limits: {
                    max_upload_bytes: 1605632,
                    reserved_bytes: 65536,
                    allocation_unit_bytes: 4096,
                    max_filename_bytes: 64
                }
            }
        };
    }

    var EPAPER_DRAW_MS = 5200;
    var EPAPER_COOLDOWN_MS = 8000;

    function epaperCapabilities() {
        return {
            panel: { model: 'waveshare-7in3e', width: 800, height: 480, colors: 6, color_codes: [0, 1, 2, 3, 5, 6] },
            image: { name: 'epaper-current.epd', format: 'epdimg', header_bytes: 40, frame_bytes: 192000, upload_bytes: 192040 },
            refresh: { cpu_mhz: 80, cooldown_seconds: 180, automatic_on_boot: false },
            capabilities: { upload: true, metadata: true, download: true, refresh: true, white: true, palette: true }
        };
    }

    function epaperStatus() {
        var operation = state.epaper.operation;
        var base = {
            state: 'idle', phase: null, busy: false, can_upload: true, can_draw: true,
            can_download: state.epaper.stored, retry_after_seconds: 0, cpu_mhz: 160,
            panel_state: 'sleeping', shutdown_method: 'power_off_then_deep_sleep', recovery_required: null,
            stored_image: { available: state.epaper.stored, valid: state.epaper.stored },
            last_operation: state.epaper.lastSource
                ? { source: state.epaper.lastSource, result: 'success', error_code: 'none' }
                : { source: 'none', result: 'none', error_code: 'none' },
            last_reset_reason: 'software', brownout_detected: false, brownout_during_draw: false
        };
        if (!operation) {
            return base;
        }
        var elapsed = Date.now() - operation.startedAt;
        if (elapsed >= EPAPER_DRAW_MS + EPAPER_COOLDOWN_MS) {
            state.epaper.operation = null;
            return base;
        }
        base.busy = true;
        base.can_upload = false;
        base.can_draw = false;
        if (elapsed >= EPAPER_DRAW_MS) {
            base.state = 'cooldown';
            base.retry_after_seconds = Math.ceil((EPAPER_DRAW_MS + EPAPER_COOLDOWN_MS - elapsed) / 1000);
            return base;
        }
        var phases = [
            { until: 350, state: 'queued', phase: null },
            { until: 700, state: 'drawing', phase: 'prewake' },
            { until: 1200, state: 'drawing', phase: 'initializing' },
            { until: 2100, state: 'drawing', phase: 'transferring' },
            { until: 4300, state: 'drawing', phase: 'refreshing' },
            { until: 4650, state: 'drawing', phase: 'powering_off' },
            { until: 4950, state: 'drawing', phase: 'sleeping' },
            { until: EPAPER_DRAW_MS, state: 'drawing', phase: 'quiescing' }
        ];
        var current = phases.find(function (phase) { return elapsed < phase.until; }) || phases[phases.length - 1];
        base.state = current.state;
        base.phase = current.phase;
        base.panel_state = current.state === 'queued' ? 'inactive' : 'active';
        return base;
    }

    function beginEpaper(source) {
        var current = epaperStatus();
        if (!current.can_draw) {
            return fail(409, 'epaper_busy', 'e-paper busy', { retry_after_seconds: current.retry_after_seconds });
        }
        state.epaper.lastSource = source;
        state.epaper.operation = { startedAt: Date.now(), source: source };
        return jsonResponse(202, { success: true, data: { state: 'queued' }, message: 'e-paper draw queued' });
    }

    function handle(method, path, init) {
        if (path === 'api/alive' && method === 'GET') {
            return ok({});
        }
        if (path === 'api/epaper' && method === 'GET') {
            return ok(epaperCapabilities());
        }
        if (path === 'api/epaper/status' && method === 'GET') {
            return ok(epaperStatus());
        }
        if (path === 'api/epaper/image' && method === 'POST') {
            var byteLength = init && init.body && init.body.byteLength;
            if (byteLength !== 192040) {
                return fail(422, 'invalid_epaper_image', 'invalid EPDIMG length');
            }
            state.epaper.stored = true;
            return beginEpaper('uploaded');
        }
        if (path === 'api/epaper/image/white' && method === 'POST') {
            return beginEpaper('white');
        }
        if (path === 'api/epaper/image/palette' && method === 'POST') {
            return beginEpaper('palette');
        }
        if (path === 'api/epaper/image/refresh' && method === 'POST') {
            return state.epaper.stored ? beginEpaper('stored') : fail(404, 'epaper_image_not_found', 'stored image not found');
        }
        if (path === 'api/device' && method === 'GET') {
            // heap 固定值：假資料不做隨機浮動，避免背景輪詢時數字自己跳動。
            return ok({
                chip_model: 'ESP32-C6',
                chip_revision: 0,
                cpu_cores: 1,
                flash_mb: 4,
                heap_used_percent: state.heapUsedPercent,
                mac_address: 'AA:BB:CC:DD:EE:FF',
                hostname: state.hostname,
                config_state: 'persisted',
                config_recovery_reason: 'none'
            });
        }
        if (path === 'api/storage' && method === 'GET') {
            return ok(storageSnapshot());
        }
        if (path === 'api/wifi' && method === 'GET') {
            return ok(wifiSnapshot());
        }
        if (path === 'api/auth' && method === 'GET') {
            return ok({ username: 'admin' });
        }
        if (path === 'api/auth/login' && method === 'POST') {
            var login = parseJson(init);
            if (!login || !login.username || login.password === undefined) {
                return fail(400, 'missing_field', 'missing credentials');
            }
            if (login.username !== 'admin' || login.password !== state.password) {
                return jsonResponse(401, {
                    success: false,
                    data: { code: 'unauthorized', authenticated: false },
                    message: 'invalid credentials'
                });
            }
            state.token = randomToken();
            return ok({ authenticated: true, token_type: 'Bearer', token: state.token }, 'authenticated');
        }
        // 以下皆需有效 Bearer token。
        if (!requireAuth(init)) {
            return fail(401, 'unauthorized', 'unauthorized', { authenticated: false });
        }
        if (path === 'api/auth/session' && method === 'GET') {
            return ok({ authenticated: true }, 'authenticated');
        }
        if (path === 'api/auth/logout' && method === 'POST') {
            state.token = '';
            return ok({}, 'logged out');
        }
        if (path === 'api/auth/password' && method === 'PUT') {
            var passwordBody = parseJson(init);
            if (!passwordBody || typeof passwordBody.password !== 'string'
                || passwordBody.password.length < 8 || passwordBody.password.length > 63) {
                return fail(400, 'invalid_field', 'invalid password', { fields: ['password'] });
            }
            state.password = passwordBody.password;
            state.token = '';
            return ok({ session: 'invalidated' }, 'admin password updated');
        }
        if (path === 'api/system' && method === 'PUT') {
            var systemBody = parseJson(init);
            var hostname = systemBody && systemBody.hostname;
            if (!hostname || !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,29}[A-Za-z0-9])?$/.test(hostname)) {
                return fail(400, 'invalid_field', 'invalid hostname', { fields: ['hostname'] });
            }
            state.hostname = hostname;
            return ok({ hostname: hostname }, 'system updated');
        }
        if (path === 'api/wifi/scan' && method === 'GET') {
            return ok({ networks: SCAN_NETWORKS });
        }
        if (path === 'api/wifi/connect' && method === 'GET') {
            return ok(settleTransition() || { state: 'idle', failure_code: 'none', ip: '', ap_shutdown_in_seconds: 0 });
        }
        if (path === 'api/wifi' && method === 'PUT') {
            var payload = parseJson(init);
            if (!payload || !payload.mode || !payload.interfaces) {
                return fail(400, 'missing_field', 'missing wifi fields');
            }
            if (state.transition) {
                return fail(409, 'wifi_connect_busy', 'wifi busy');
            }
            // 持久化 mode 為 AP 且目標含 STA → safe transition（202）；SSID 含 fail 模擬驗證失敗。
            if (state.wifi.mode === 'ap' && payload.mode !== 'ap') {
                state.transition = {
                    startedAt: Date.now(),
                    payload: payload,
                    fail: /fail/i.test(payload.interfaces.sta.ssid || '')
                };
                return jsonResponse(202, {
                    success: true,
                    data: { state: 'connecting' },
                    message: 'wifi transition started'
                });
            }
            applyWifiPayload(payload);
            return ok({}, 'wifi updated');
        }
        if (path === 'api/wifi/reconnect' && method === 'POST') {
            return ok({}, 'wifi reconnecting');
        }
        if (path === 'api/system/reset' && method === 'POST') {
            // 真裝置會清設定並重啟；mock 同步回到出廠預設，讓重設後的畫面是真的。
            state.password = 'password';
            state.token = '';
            state.hostname = 'esp32-device';
            state.transition = null;
            state.wifi.mode = 'ap';
            state.wifi.fallbackToAp = true;
            state.wifi.sta = {
                ssid: '',
                security: 'wpa',
                password: '',
                state: 'disabled',
                ip: '0.0.0.0',
                ipConfig: { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
            };
            state.wifi.ap = {
                enabled: true,
                state: 'active',
                ssid: 'esp32-device-A1B2',
                passwordEnabled: false,
                ip: '192.168.4.1',
                ipConfig: { mode: 'default', address: '192.168.4.1', netmask: '255.255.255.0' }
            };
            return ok({}, 'reset scheduled; restarting');
        }
        return fail(404, 'not_found', 'not found');
    }

    // 只攔相對路徑 api/... 的 fetch；其餘（圖片、demo assets）照常放行。
    var realFetch = window.fetch;
    window.fetch = function (input, init) {
        var url = typeof input === 'string' ? input : (input && input.url) || '';
        // 避免 regex 內出現 `//`：tools/build 的簡易 JS minifier 會把它誤判為註解。
        var path = url.replace(/^\.?[/]/, '').split('?')[0];
        if (path.indexOf('api/') !== 0) {
            return realFetch.apply(window, arguments);
        }
        var method = ((init && init.method) || 'GET').toUpperCase();
        return new Promise(function (resolve) {
            // 模擬些許網路延遲，讓 busy 狀態可被觀察。
            var latency = LATENCY_MIN_MS + Math.random() * (LATENCY_MAX_MS - LATENCY_MIN_MS);
            window.setTimeout(function () {
                resolve(handle(method, path, init));
            }, latency);
        });
    };

    // header 常駐 PREVIEW 標示，避免把假資料誤認成真實裝置狀態。
    // 等 DOMContentLoaded 讓 app-state 先套用語言偏好，badge 文字才用對語言。
    function showBadge() {
        var actions = document.querySelector('.app-header-actions');
        if (!actions) {
            return;
        }
        actions.insertBefore(
            app.utils.dom.el('span', {
                className: 'device-mock-badge',
                text: app.i18n.t('mockBadge'),
                attrs: { title: app.i18n.t('mockBadgeTitle') }
            }),
            actions.firstChild
        );
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', showBadge, { once: true });
    } else {
        showBadge();
    }
})(window.DitherApp);
