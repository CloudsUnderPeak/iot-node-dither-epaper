(async function () {
    var passed = [];
    var root = document.createElement('div');
    document.body.appendChild(root);
    try {
        ['namespace.js', 'utils/dom.js', 'i18n/en.js', 'i18n/zh-TW.js',
            'i18n/index.js', 'ui/notice.js'].forEach(function (path) {
            harness.execute(path, { window: window });
        });
        var app = window.DitherApp;
        var liveState = 'online', liveListener, authListener;
        var timerCalls = [], statusCalls = [], deviceCalls = [], keepCalls = [];
        app.device.live = {
            state: function () { return liveState; },
            subscribe: function (listener) { liveListener = listener; },
            noteSuccess: function () {}, noteRequestFailure: function () {}
        };
        app.device.auth = {
            canManage: function () { return true; },
            subscribe: function (listener) { authListener = listener; },
            ensureSession: function () { return Promise.resolve(true); },
            createLockedCard: function () { return document.createElement('div'); }
        };
        app.device.api = { sessionEpoch: function () { return 1; }, resources: {
            device: function () { var pending = harness.deferred(); deviceCalls.push(pending); return pending.promise; },
            sleepStatus: function () { var pending = harness.deferred(); statusCalls.push(pending); return pending.promise; },
            sleepKeepAwake: function () { var pending = harness.deferred(); keepCalls.push(pending); return pending.promise; },
            sleepUpdate: function (payload) { return Promise.resolve(Object.assign({}, sample, { enabled: payload.enabled })); },
            sleepNow: function () { return Promise.resolve({ sleep_request: { state: 'pending', error_code: null } }); },
            systemTime: function () { return Promise.resolve({ epoch: 1790067600, synced: true, source: 'client' }); }
        } };
        var featureValues = null;
        app.device.features = {
            known: function () { return featureValues !== null; },
            supports: function (name) { return !!(featureValues && featureValues[name]); },
            subscribe: function () {},
            refresh: function () {
                var pending = harness.deferred(); deviceCalls.push(pending);
                return pending.promise.then(function (data) { featureValues = data.features; });
            }
        };
        app.device.errorText = function (error) { return error.message; };
        var originalInterval = window.setInterval;
        var originalClear = window.clearInterval;
        window.setInterval = function (fn, ms) { timerCalls.push({ fn: fn, ms: ms }); return timerCalls.length; };
        window.clearInterval = function () {};
        var sample = {
            enabled: true, wake_network_sync_enabled: true, mode: 'normal', state: 'armed', storage_state: 'ok',
            idle: { armed: true, timeout_seconds: 600, remaining_seconds: 300 },
            schedule: { period_minutes: 1440, anchor_epoch: 1790071200, clock_basis: 'absolute',
                next_wake_epoch: 1790071200, next_wake_in_seconds: 3600 },
            time: { epoch: 1790067600, synced: true, source: 'client' },
            sleep_request: { state: 'none', error_code: null }, blockers: []
        };
        try {
            harness.execute('device/device-sleep.js', { window: window });
            app.device.sleep.start();
            app.device.sleep.start();
            harness.assert(deviceCalls.length === 1 && timerCalls.filter(function (x) { return x.ms === 15000; }).length === 1,
                'one capability request and one polling timer');
            deviceCalls.shift().resolve({ features: { sleep: true }, time: sample.time });
            await Promise.resolve(); await Promise.resolve();
            harness.assert(statusCalls.length === 1, 'supported capability starts sleep status');
            statusCalls.shift().resolve(sample);
            await Promise.resolve(); await Promise.resolve();
            harness.assert(app.device.sleep.snapshot().remainingSeconds <= 300, 'countdown initialized');
            var previous = app.device.sleep.snapshot().remainingSeconds;
            var oldNow = Date.now;
            Date.now = function () { return oldNow() + 3600000; };
            harness.assert(Math.abs(app.device.sleep.snapshot().remainingSeconds - previous) <= 1,
                'countdown uses monotonic time after browser clock change');
            Date.now = oldNow;
            passed.push('Sleep capability, single timer and monotonic countdown');

            var firstKeep = app.device.sleep.keepAwake();
            var secondRejected = false;
            await app.device.sleep.keepAwake().catch(function () { secondRejected = true; });
            harness.assert(secondRejected && keepCalls.length === 1, 'keep-awake is single flight');
            keepCalls.shift().resolve({ idle: { armed: true, timeout_seconds: 600, remaining_seconds: 1800 } });
            await firstKeep;
            harness.assert(app.device.sleep.snapshot().remainingSeconds >= 599, 'keep-awake updates countdown');
            passed.push('Keep-awake single flight and authoritative idle response');

            var stale = app.device.sleep.refresh();
            harness.assert(statusCalls.length === 1, 'status refresh uses one request');
            liveState = 'offline'; liveListener('offline');
            liveState = 'online'; liveListener('online');
            deviceCalls.shift().resolve({ features: { sleep: true }, time: sample.time });
            await Promise.resolve(); await Promise.resolve();
            harness.assert(statusCalls.length === 2,
                'reconnect starts a fresh status request while stale request is pending');
            statusCalls.shift().resolve(Object.assign({}, sample, { enabled: false }));
            await stale;
            harness.assert(app.device.sleep.snapshot().status === null,
                'late offline response cannot restore stale status');
            statusCalls.shift().resolve(sample);
            await Promise.resolve(); await Promise.resolve();
            harness.assert(app.device.sleep.snapshot().status === sample,
                'fresh reconnect response becomes authoritative');
            liveState = 'offline'; liveListener('offline');
            liveState = 'online'; liveListener('online');
            deviceCalls.shift().resolve({ features: { sleep: false }, time: sample.time });
            await Promise.resolve(); await Promise.resolve();
            harness.assert(!app.device.sleep.snapshot().supported, 'explicit false hides sleep feature');
            passed.push('Offline invalidates stale responses, reconnect refreshes, and false hides sleep');

            harness.assert(typeof authListener === 'function', 'session lifecycle listener registered');
            ['ui/svg-icons.js', 'ui/select-field.js', 'pages/device-sleep/page.js'].forEach(function (path) {
                harness.execute(path, { window: window });
            });
            var updateCalls = [], pageListener;
            var pageStatus = Object.assign({}, sample, { schedule: Object.assign({}, sample.schedule, {
                next_wake_epoch: new Date(2026, 8, 27, 9, 30).getTime() / 1000
            }) });
            app.device.sleep = {
                snapshot: function () { return { supported: true, status: pageStatus }; },
                subscribe: function (listener) { pageListener = listener; return function () { pageListener = null; }; },
                refresh: function () { return Promise.resolve(pageStatus); },
                update: function (payload) {
                    updateCalls.push(payload);
                    pageStatus = Object.assign({}, pageStatus, { enabled: payload.enabled });
                    if (typeof payload.wake_network_sync_enabled === 'boolean') {
                        pageStatus.wake_network_sync_enabled = payload.wake_network_sync_enabled;
                    }
                    return Promise.resolve(pageStatus);
                }
            };
            app.device.bindLiveGate = function () {
                return { banner: document.createElement('div'), unbind: function () {} };
            };
            var fixedTime = new Date(2026, 8, 27, 8, 15, 30).getTime();
            Date.now = function () { return fixedTime; };
            try {
                app.pages.deviceSleepPage.mount(root);
                var checkbox = root.querySelector('input[type="checkbox"]');
                var syncCheckbox = root.querySelector('#sleep-network-sync');
                harness.assert(syncCheckbox.checked && !syncCheckbox.disabled, 'network sync initialized from device');
                syncCheckbox.checked = false;
                syncCheckbox.dispatchEvent(new Event('change', { bubbles: true }));
                var period = root.querySelector('select');
                var hour = root.querySelector('select[name="sleep-hour"]');
                var minute = root.querySelector('select[name="sleep-minute"]');
                var save = root.querySelector('button');
                function edit(value, hours) {
                    var parts = value.split(':');
                    hour.value = String(Number(parts[0]));
                    minute.value = String(Number(parts[1]));
                    period.value = String((hours || 24) * 60);
                    minute.dispatchEvent(new Event('change', { bubbles: true }));
                }
                async function saveDraft() {
                    save.click();
                    await Promise.resolve(); await Promise.resolve(); await Promise.resolve();
                    return updateCalls[updateCalls.length - 1];
                }
                harness.assert(hour.value === '9' && minute.value === '30' && period.value === '1440', 'time draft uses existing schedule');
                harness.assert(root.querySelectorAll('select').length === 3 && root.querySelectorAll('button').length === 1
                    && !root.querySelector('input[type="text"], input[type="number"], input[type="date"], input[type="datetime-local"], input[type="time"]'),
                    'only period, hour/minute dropdowns and save are exposed; no sleep-now action or date picker');
                harness.assert(hour.options.length === 24 && minute.options.length === 60
                    && hour.options[0].text === '00' && hour.options[23].text === '23'
                    && minute.options[0].text === '00' && minute.options[59].text === '59',
                    'time dropdowns expose only valid hours and minutes');
                harness.assert(root.querySelector('.device-content').firstElementChild.matches('.device-sleep-info')
                    && root.querySelector('.device-sleep-info .device-field-value').textContent === app.i18n.t('sleepEnabled')
                    && root.querySelector('.device-sleep-blockers .device-field-value').textContent === app.i18n.t('sleepNoBlockers')
                    && !root.querySelector('.device-sleep-card .device-badge, .device-sleep-card .device-field-value'),
                    'information card precedes settings and owns status and blockers');
                var payload = await saveDraft();
                harness.assert(payload.wake_network_sync_enabled === false, 'unchecking is saved with schedule');
                harness.assert(payload.first_wake_delay_minutes === 75 && payload.client_time === Math.floor(fixedTime / 1000)
                    && payload.period_minutes === 1440, 'HH:mm converts to existing minute-based API payload');
                passed.push('Power schedule uses hour/minute dropdowns and a separate information card');

                edit('20:16', 12);
                harness.assert(save.disabled, 'a time outside the 12-hour interval blocks saving');
                edit('20:15', 12);
                harness.assert(!save.disabled, '12-hour boundary is accepted');
                payload = await saveDraft();
                harness.assert(payload.first_wake_delay_minutes === 720 && payload.period_minutes === 720,
                    '12-hour boundary sends the maximum valid delay');
                edit('08:15');
                payload = await saveDraft();
                harness.assert(payload.first_wake_delay_minutes === 1440, 'past time uses tomorrow');
                edit('09:30', 48);
                payload = await saveDraft();
                harness.assert(payload.period_minutes === 2880, '48-hour repeat interval is retained');
                fixedTime = new Date(2026, 8, 27, 23, 55, 30).getTime();
                edit('00:10', 12);
                payload = await saveDraft();
                harness.assert(payload.first_wake_delay_minutes === 15, 'midnight rollover uses the next local day');
                passed.push('Power schedule validates interval limits and midnight rollover');

                edit('01:30');
                pageListener({ supported: true, status: Object.assign({}, pageStatus, {
                    enabled: false, blockers: ['usb_host_connected']
                }) });
                harness.assert(hour.value === '1' && minute.value === '30', 'status polling does not overwrite the time draft');
                harness.assert(!syncCheckbox.checked, 'polling does not overwrite network sync draft');
                harness.assert(root.querySelector('.device-sleep-info .device-field-value').textContent === app.i18n.t('sleepDisabled')
                    && root.querySelector('.device-sleep-blockers .device-field-value').textContent === 'usb_host_connected',
                    'polling updates disabled status and blockers in the information card');
                checkbox.checked = false;
                checkbox.dispatchEvent(new Event('change', { bubbles: true }));
                harness.assert(syncCheckbox.disabled && period.disabled && hour.disabled && minute.disabled && !save.disabled, 'disabling locks time and period fields');
                payload = await saveDraft();
                harness.assert(JSON.stringify(payload) === '{"enabled":false}', 'disable sends only enabled=false');
                passed.push('Power schedule preserves drafts and disables with the existing API contract');
                app.pages.deviceSleepPage.unmount();
                root.innerHTML = '';
                fixedTime = new Date(2026, 8, 27, 8, 15, 30).getTime();
                pageStatus = Object.assign({}, sample, {
                    wake_network_sync_enabled: false,
                    schedule: { period_minutes: 1, next_wake_epoch: Math.floor(fixedTime / 1000) + 60 }
                });
                app.pages.deviceSleepPage.mount(root);
                period = root.querySelector('select');
                hour = root.querySelector('select[name="sleep-hour"]');
                minute = root.querySelector('select[name="sleep-minute"]');
                save = root.querySelector('button');
                harness.assert(period.value === '1' && period.selectedOptions[0].textContent.includes('1'),
                    'custom minute schedule remains visible');
                payload = await saveDraft();
                harness.assert(payload.period_minutes === 1 && payload.first_wake_delay_minutes === 1,
                    'saving a custom minute schedule does not replace it with 24 hours');
                passed.push('Custom minute schedules are displayed and preserved');
                harness.assert(!root.querySelector('#sleep-network-sync').checked,
                    'saved network preference restored when page is remounted');
                var realUpdate = app.device.sleep.update;
                app.device.sleep.update = function () { return Promise.reject(new Error('save failed')); };
                syncCheckbox = root.querySelector('#sleep-network-sync');
                syncCheckbox.checked = true;
                await saveDraft();
                await Promise.resolve();
                harness.assert(syncCheckbox.checked && !save.disabled, 'failed save retains draft and releases busy');
                app.device.sleep.update = realUpdate;
                app.pages.deviceSleepPage.unmount();
                root.innerHTML = '';
                delete pageStatus.wake_network_sync_enabled;
                app.pages.deviceSleepPage.mount(root);
                syncCheckbox = root.querySelector('#sleep-network-sync');
                harness.assert(syncCheckbox.disabled && syncCheckbox.indeterminate
                    && root.querySelector('#sleep-network-sync-hint').getAttribute('data-tooltip') === app.i18n.t('sleepNetworkSyncUnsupported'),
                    'legacy firmware displays unsupported control without claiming it is off');
                save = root.querySelector('button');
                payload = await saveDraft();
                harness.assert(!Object.prototype.hasOwnProperty.call(payload, 'wake_network_sync_enabled'),
                    'legacy firmware saves omit the unsupported field');
                passed.push('Wake network preference, failed drafts and legacy firmware handling');
            } finally {
                app.pages.deviceSleepPage.unmount();
                Date.now = oldNow;
            }
            var realFetch = window.fetch;
            try {
                harness.execute('device/device-mock.js', { window: window });
                var loginResponse = await window.fetch('api/auth/login', {
                    method: 'POST', body: JSON.stringify({ username: 'admin', password: 'password' })
                });
                var login = await loginResponse.json();
                var headers = { Authorization: 'Bearer ' + login.data.token };
                async function mockUpdate(body) {
                    var response = await window.fetch('api/sleep', {
                        method: 'PUT', headers: headers, body: JSON.stringify(body)
                    });
                    return response.json();
                }
                var stored = await mockUpdate({ enabled: false, wake_network_sync_enabled: false });
                harness.assert(stored.success && stored.data.wake_network_sync_enabled === false,
                    'preview saves false without requiring clock fields');
                stored = await mockUpdate({ enabled: false });
                harness.assert(stored.data.wake_network_sync_enabled === false, 'preview omission preserves preference');
                for (var invalidValue of [null, 0, 'false']) {
                    var rejected = await mockUpdate({ enabled: false, wake_network_sync_enabled: invalidValue });
                    harness.assert(!rejected.success && rejected.data.code === 'invalid_field',
                        'preview rejects non-boolean preferences');
                }
                await window.fetch('api/system/reset', { method: 'POST', headers: headers, body: '{}' });
                var resetResponse = await window.fetch('api/sleep');
                stored = await resetResponse.json();
                harness.assert(!stored.data.enabled && stored.data.wake_network_sync_enabled === true
                    && stored.data.idle.timeout_seconds === 600, 'preview reset restores defaults');
                passed.push('Mock network preference validation, preservation and reset');
            } finally { window.fetch = realFetch; }
            document.getElementById('result').textContent = JSON.stringify({ passed: passed });
        } finally {
            window.setInterval = originalInterval;
            window.clearInterval = originalClear;
        }
    } finally { root.remove(); }
})().catch(function (error) {
    document.getElementById('result').textContent = JSON.stringify({ error: error.stack });
});
