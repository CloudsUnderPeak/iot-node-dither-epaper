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
            enabled: true, mode: 'normal', state: 'armed', storage_state: 'ok',
            idle: { armed: true, timeout_seconds: 1800, remaining_seconds: 300 },
            schedule: { period_hours: 24, anchor_epoch: 1790071200, clock_basis: 'absolute',
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
            keepCalls.shift().resolve({ idle: { armed: true, timeout_seconds: 1800, remaining_seconds: 1800 } });
            await firstKeep;
            harness.assert(app.device.sleep.snapshot().remainingSeconds >= 1799, 'keep-awake updates countdown');
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
            document.getElementById('result').textContent = JSON.stringify({ passed: passed });
        } finally {
            window.setInterval = originalInterval;
            window.clearInterval = originalClear;
        }
    } finally { root.remove(); }
})().catch(function (error) {
    document.getElementById('result').textContent = JSON.stringify({ error: error.stack });
});
