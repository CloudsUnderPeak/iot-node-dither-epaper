(async function () {
    var passed = [];
    try {
        var listeners = {}, calls = [], location = {hash: ''};
        var fakeWindow = { location: location,
            addEventListener: function (name, fn) { listeners[name] = fn; },
            history: {
                pushState: function (state, unused, hash) { calls.push(['push', state.route]); location.hash = hash; },
                replaceState: function (state, unused, hash) { calls.push(['replace', state.route]); location.hash = hash; }
            }
        };
        var pages = {}, events = [];
        ['dither-editor', 'help', 'device-network'].forEach(function (id) {
            pages[id] = { id: id, title: id, mount: function () { events.push('mount:' + id); },
                unmount: function () { events.push('unmount:' + id); },
                onRouteChange: function (route) { events.push('route:' + route); } };
        });
        var app = { app: { state: { blockingOperation: null }, pageRegistry: {get: function (id) { return pages[id]; }} },
            i18n: {t: function (key) { return key; }}, utils: {dom: {clear: function () { events.push('clear'); }}} };
        fakeWindow.DitherApp = app;
        harness.execute('app/page-router.js', {window: fakeWindow});
        var context = {setTitle: function () {}};
        var router = new app.app.PageRouter({}, context);
        router.start('dither-editor');
        harness.assert(router.route() === 'dither-editor' && calls[0][0] === 'replace', 'startup replaces default route');
        router.navigate('help/guide');
        harness.assert(router.route() === 'help/guide' && calls[1][0] === 'push', 'navigate pushes full route');
        router.navigate('help/details');
        harness.assert(events.indexOf('route:help/details') !== -1 && calls[2][0] === 'push', 'same-page child route uses onRouteChange');
        location.hash = '#/dither-editor';
        listeners.popstate({state: {route: 'dither-editor'}});
        harness.assert(router.route() === 'dither-editor' && events.indexOf('unmount:help') !== -1,
            'back/forward popstate unmounts prior page');
        app.app.state.blockingOperation = 'epaper';
        harness.assert(router.navigate('device-network') === false && router.route() === 'dither-editor',
            'blocking operation rejects navigation');
        location.hash = '#/help/guide';
        listeners.popstate({state: {route: 'help/guide'}});
        harness.assert(router.route() === 'dither-editor' && calls[calls.length - 1][0] === 'replace',
            'blocked history transition restores current route');
        app.app.state.blockingOperation = null;
        location.hash = '#/does-not-exist';
        listeners.popstate({state: {route: 'does-not-exist'}});
        harness.assert(router.route() === 'dither-editor', 'unknown history route returns default');
        passed.push('History start, push, replace, child route, popstate, blocking and unmount');

        var host = document.createElement('div');
        document.body.appendChild(host);
        var wifiReply = harness.deferred(), hasToken = false, authListener = null, unsubscribed = false;
        var guardedPages = {};
        var guardedApp = {
            app: {state: {blockingOperation: null}, pageRegistry: {get: function (id) { return guardedPages[id]; }}},
            pages: {}, utils: {}, i18n: {t: function (key) { return key; }},
            ui: {createNotice: function () { return {node: document.createElement('div')}; },
                svgIcons: {create: function () { return document.createElement('span'); }}},
            device: {
                bindLiveGate: function () { return {banner: document.createElement('div'), unbind: function () {}}; },
                api: {resources: {wifi: function () { return wifiReply.promise; },
                    device: function () { return Promise.resolve({hostname: 'test'}); }}},
                auth: {hasToken: function () { return hasToken; },
                    createLockedCard: function () { var node = document.createElement('div'); node.className = 'locked'; return node; },
                    ensureSession: function () { return Promise.resolve(true); },
                    subscribe: function (listener) { authListener = listener; return function () { unsubscribed = true; }; }}
            }
        };
        guardedApp.pages.deviceNetworkWifiForm = {create: function () {
            var node = document.createElement('div'); node.className = 'wifi-edit';
            return {node: node, setBaseline: function () {}};
        }};
        var guardedWindow = {DitherApp: guardedApp, location: {hash: ''},
            addEventListener: function () {}, setInterval: function () { return 1; }, clearInterval: function () {},
            history: {pushState: function () {}, replaceState: function () {}}};
        harness.execute('utils/dom.js', {window: guardedWindow});
        harness.execute('pages/device-network/page.js', {window: guardedWindow});
        harness.execute('app/page-router.js', {window: guardedWindow});
        guardedPages['device-network'] = guardedApp.pages.deviceNetworkPage;
        guardedPages.help = {id: 'help', title: 'Help', mount: function () {}, unmount: function () {}};
        var guardedRouter = new guardedApp.app.PageRouter(host, {setTitle: function () {}});
        guardedRouter.start('device-network');
        harness.assert(Boolean(host.querySelector('.locked')) && !host.querySelector('.wifi-edit'),
            'history entry to device settings respects login gate');
        hasToken = true;
        authListener();
        harness.assert(Boolean(host.querySelector('.wifi-edit')), 'authenticated page exposes settings');
        hasToken = false;
        authListener();
        harness.assert(Boolean(host.querySelector('.locked')) && !host.querySelector('.wifi-edit'),
            'session rotation restores login gate');
        guardedRouter.navigate('help');
        wifiReply.resolve({mode: 'sta', interfaces: {sta: {state: 'connected'}, ap: {state: 'active'}}});
        await Promise.resolve(); await Promise.resolve();
        harness.assert(unsubscribed && guardedRouter.route() === 'help' && !host.querySelector('.network-status-value'),
            'unmounted network page ignores stale Wi-Fi response and unsubscribes');
        host.remove();
        passed.push('History device route login gate, session rotation and stale response cleanup');
        document.getElementById('result').textContent = JSON.stringify({passed: passed});
    } catch (error) {
        document.getElementById('result').textContent = JSON.stringify({passed: passed, error: error.stack});
    }
})();
