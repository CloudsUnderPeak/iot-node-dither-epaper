(async function () {
    var passed = [];
    try {
        var liveListeners = [];
        var transport = harness.controlledFetch();
        var token = '';
        var app = { core: { storageKeys: { deviceToken: 'features-token' } }, device: {},
            i18n: { t: function (key) { return key; } } };
        var fakeWindow = { DitherApp: app, setTimeout: function () { return 1; },
            clearTimeout: function () {}, clearInterval: function () {}, setInterval: function () { return 1; } };
        app.device.live = { state: function () { return 'online'; },
            subscribe: function (listener) { liveListeners.push(listener); },
            noteSuccess: function () {}, noteRequestFailure: function () {} };
        harness.execute('device/device-api.js', { window: fakeWindow, fetch: transport,
            AbortController: undefined, localStorage: { getItem: function () { return token; },
                setItem: function (key, value) { token = value; }, removeItem: function () { token = ''; } } });
        harness.execute('device/device-features.js', { window: fakeWindow, document: { hidden: false } });
        harness.execute('device/device-auth.js', { window: fakeWindow });
        var features = app.device.features;
        harness.assert(!features.known() && !app.device.auth.canManage(), 'unknown capabilities keep management locked');
        var pending = features.refresh();
        var duplicate = features.refresh();
        harness.assert(pending === duplicate && transport.requests.length === 1, 'feature discovery is single flight');
        var none = { sleep: false, epaper: false, storage: false, auth: false,
            user_files: false, mdns: false, battery: false, console: false };
        transport.requests[0].response.resolve({ ok: true, status: 200,
            json: function () { return Promise.resolve({ success: true, data: { features: none } }); } });
        await pending;
        harness.assert(features.known() && features.disabled('epaper') && !features.supports('battery'), 'explicit false disables features');
        harness.assert(await app.device.auth.ensureSession(), 'auth off allows management');
        harness.assert(!app.device.auth.hasToken() && app.device.auth.canManage(), 'management access never fabricates a token');
        var count = transport.requests.length;
        var error = await app.device.api.resources.epaperWhite().then(function () { return ''; }, function (failure) { return failure.code; });
        harness.assert(error === 'feature_unsupported' && transport.requests.length === count, 'unsupported panel action never reaches transport');
        error = await app.device.api.resources.sleepNow().then(function () { return ''; }, function (failure) { return failure.code; });
        harness.assert(error === 'feature_unsupported' && transport.requests.length === count, 'unsupported sleep action never reaches transport');
        pending = app.device.api.resources.changePassword('new-password');
        harness.assert(transport.requests[count].url === 'api/wifi/ap/password', 'auth off uses the AP credential route');
        transport.requests[count].response.resolve({ ok: true, status: 200,
            json: function () { return Promise.resolve({ success: true, data: {} }); } });
        await pending;
        passed.push('Feature discovery, no-token management and unsupported action guards');
        app.app = { state: { blockingOperation: null } };
        app.utils = {}; app.pages = {};
        app.device.epaper = { isSupported: function () { return true; } };
        app.device.sleep = { snapshot: function () { return { supported: true }; } };
        app.device.bindLiveGate = function () {
            return { banner: document.createElement('div'), unbind: function () {} };
        };
        harness.execute('utils/dom.js', { window: fakeWindow });
        harness.execute('app/app-menu.js', { window: fakeWindow });
        var menu = new app.app.AppMenu(document.createElement('button'), function () {});
        harness.assert(!menu.node.textContent.includes('menuDeviceSleep')
            && !menu.node.textContent.includes('menuEpaperTest')
            && !menu.node.textContent.includes('menuLogout'), 'disabled features hide actions despite stale service state');
        menu.node.remove();
        harness.execute('pages/device-info/page.js', { window: fakeWindow });
        var host = document.createElement('div');
        app.pages.deviceInfoPage.mount(host);
        var cards = host.querySelectorAll('.panel-section');
        harness.assert(cards[1].hidden && cards[3].hidden && !cards[2].hidden,
            'battery and userdata cards hide while firmware capacity stays visible');
        app.pages.deviceInfoPage.unmount();
        host.remove();
        passed.push('Feature-dependent menu and information cards');

        features.start();
        var stale = transport.requests[transport.requests.length - 1];
        liveListeners[0]('offline');
        harness.assert(!features.known() && !app.device.auth.canManage(), 'offline invalidates auth exemption');
        stale.response.resolve({ ok: true, status: 200,
            json: function () { return Promise.resolve({ success: true, data: { features: none } }); } });
        await Promise.resolve(); await Promise.resolve(); await Promise.resolve();
        harness.assert(!features.known(), 'late feature response cannot restore auth exemption');
        pending = features.refresh();
        var full = Object.assign({}, none, { auth: true, epaper: true, storage: true });
        transport.requests[transport.requests.length - 1].response.resolve({ ok: true, status: 200,
            json: function () { return Promise.resolve({ success: true, data: { features: full } }); } });
        await pending;
        harness.assert(features.supports('auth') && !app.device.auth.canManage(), 'auth on still requires a real token');
        passed.push('Offline generation guard and restoring authenticated firmware');
        document.getElementById('result').textContent = JSON.stringify({ passed: passed });
    } catch (error) {
        document.getElementById('result').textContent = JSON.stringify({ error: error.stack });
    }
})();
