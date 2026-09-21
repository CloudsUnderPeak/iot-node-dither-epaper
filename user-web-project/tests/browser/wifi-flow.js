(async function () {
    var passed = [];
    var root = document.createElement('div');
    document.body.appendChild(root);
    try {
        ['namespace.js', 'utils/dom.js', 'i18n/en.js', 'i18n/zh-TW.js', 'i18n/index.js',
            'ui/select-field.js', 'ui/password-field.js', 'pages/device-network/wifi-form.js']
            .forEach(function (path) {
                if (path === 'ui/password-field.js') {
                    window.DitherApp.ui.svgIcons = { create: function () { return document.createElement('span'); } };
                }
                harness.execute(path, { window: window });
            });
        var app = window.DitherApp;
        var form = app.pages.deviceNetworkWifiForm.create();
        root.appendChild(form.node);
        var baseline = { configured_mode: 'sta', fallback_to_ap: true, interfaces: {
            sta: { ssid: 'home', security: 'wpa', ip_config: { mode: 'dhcp' } },
            ap: { ssid: 'device', password_enabled: false, ip_config: { mode: 'default' } }
        } };
        form.setBaseline(baseline, true);
        harness.assert(!form.hasChanges() && form.valid(), 'clean WPA baseline is valid');
        var mode = root.querySelector('input[name="wifi-mode"][value="ap_sta"]');
        mode.checked = true;
        mode.dispatchEvent(new Event('change'));
        harness.assert(form.hasChanges() && form.valid(), 'mode change is dirty and valid with reused password');
        var payload = form.buildPayload();
        harness.assert(payload.mode === 'ap_sta' && payload.interfaces.sta.ssid === 'home' &&
            !Object.prototype.hasOwnProperty.call(payload.interfaces.sta, 'password') &&
            payload.interfaces.ap.ssid === 'device', 'complete replacement keeps both interfaces and WPA secret');
        mode = root.querySelector('input[name="wifi-mode"][value="ap"]');
        mode.checked = true;
        mode.dispatchEvent(new Event('change'));
        payload = form.buildPayload();
        harness.assert(payload.interfaces.sta.ssid === 'home', 'hidden STA fields come from baseline');
        passed.push('Wi-Fi mode, dirty/valid, hidden fields, reused WPA and replacement payload');

        mode = root.querySelector('input[name="wifi-mode"][value="ap_sta"]');
        mode.checked = true;
        mode.dispatchEvent(new Event('change'));
        var sta = root.querySelectorAll('.wifi-section')[0];
        var selects = sta.querySelectorAll('select');
        selects[0].value = 'open';
        selects[0].dispatchEvent(new Event('change'));
        payload = form.buildPayload();
        harness.assert(form.valid() && payload.interfaces.sta.security === 'open' &&
            !Object.prototype.hasOwnProperty.call(payload.interfaces.sta, 'password') &&
            sta.querySelector('input[type="password"]').disabled, 'Open disables password and omits it from payload');
        selects[1].value = 'static';
        selects[1].dispatchEvent(new Event('change'));
        harness.assert(!form.valid(), 'static IPv4 requires valid addresses');
        var addressInputs = sta.querySelectorAll('.device-static-rows input');
        ['192.168.1.50', '255.255.255.0', '192.168.1.1', '8.8.8.8', ''].forEach(function (value, index) {
            addressInputs[index].value = value;
            addressInputs[index].dispatchEvent(new Event('input'));
        });
        payload = form.buildPayload();
        harness.assert(form.valid() && payload.interfaces.sta.ip_config.mode === 'static' &&
            payload.interfaces.sta.ip_config.address === '192.168.1.50' &&
            JSON.stringify(payload.interfaces.sta.ip_config.dns) === '["8.8.8.8"]',
            'valid static IPv4 and DNS enter replacement payload');
        form.setBaseline(baseline, true);
        payload = form.buildPayload();
        passed.push('Wi-Fi Open security and static IPv4 validation');

        var timers = [], updates = [], statuses = [], notices = [], releases = 0, accepted = 0;
        var fakeWindow = { DitherApp: app, setTimeout: function (fn, delay) { timers.push({fn: fn, delay: delay}); } };
        app.device.live = { suppress: function () {}, release: function () { releases++; } };
        app.device.api = { resources: {
            wifiUpdate: function (value) { updates.push(value); return Promise.resolve({state: 'connecting'}); },
            wifiConnectStatus: function () { var item = harness.deferred(); statuses.push(item); return item.promise; }
        } };
        app.device.errorText = function (error) { return error.message; };
        app.device.auth = { invalidateSession: function () {} };
        harness.execute('pages/device-network/wifi-controller.js', { window: fakeWindow });
        var mounted = true;
        var context = { form: {
            buildPayload: function () { return payload; }, hasApChanges: function () { return false; },
            passwordEnabledChanged: function () { return false; }, setBusy: function () {},
            acceptSaved: function () { accepted++; }
        }, notice: { set: function (value) { notices.push(value); }, clear: function () {} },
        refresh: function () {}, isMounted: function () { return mounted; } };
        app.pages.deviceNetworkController.save(context);
        await Promise.resolve();
        harness.assert(accepted === 0 && timers.some(function (item) { return item.delay === 1000; }), '202 waits for status');
        timers.filter(function (item) { return item.delay === 1000; }).pop().fn();
        statuses.shift().resolve({state: 'connected', ip: '192.168.1.2'});
        await Promise.resolve(); await Promise.resolve();
        harness.assert(accepted === 1 && releases === 1, 'connected alone commits baseline');
        app.pages.deviceNetworkController.save(context);
        await Promise.resolve();
        timers.filter(function (item) { return item.delay === 1000; }).pop().fn();
        statuses.shift().resolve({state: 'failed', failure_code: 'connect_timeout'});
        await Promise.resolve(); await Promise.resolve();
        harness.assert(accepted === 1 && notices.some(function (value) { return value.indexOf('connect_timeout') !== -1; }), 'failed transition keeps draft');
        app.pages.deviceNetworkController.save(context);
        await Promise.resolve();
        mounted = false;
        timers.filter(function (item) { return item.delay === 1000; }).pop().fn();
        harness.assert(accepted === 1, 'leaving page ignores stale poll');
        passed.push('Wi-Fi 202 polling, connected baseline, failed draft and unmount');

        mounted = true;
        app.device.api.resources.wifiUpdate = function () { return Promise.resolve({state: 'connecting'}); };
        app.pages.deviceNetworkController.save(context);
        await Promise.resolve();
        timers.filter(function (item) { return item.delay === 1000; }).pop().fn();
        var timeoutStatus = statuses.shift();
        var originalNow = Date.now;
        try {
            var start = originalNow();
            Date.now = function () { return start + 26000; };
            timeoutStatus.reject(new Error('disconnected'));
            await Promise.resolve(); await Promise.resolve();
            harness.assert(accepted === 1 && notices.some(function (value) {
                return value === app.i18n.t('wifiTransitionRetryEnded');
            }), 'transport timeout preserves draft and releases transition');
        } finally {
            Date.now = originalNow;
        }
        var lateUpdate = harness.deferred();
        app.device.api.resources.wifiUpdate = function () { return lateUpdate.promise; };
        app.pages.deviceNetworkController.save(context);
        mounted = false;
        lateUpdate.resolve({state: 'connected'});
        await Promise.resolve(); await Promise.resolve();
        harness.assert(accepted === 1, 'late save response cannot commit an unmounted form');
        passed.push('Wi-Fi transport timeout and stale save response');
        document.getElementById('result').textContent = JSON.stringify({passed: passed});
    } finally {
        root.remove();
    }
})().catch(function (error) {
    document.getElementById('result').textContent = JSON.stringify({error: error.stack});
});
