(function (app) {
    var values = null;
    var listeners = [];
    var request = null;
    var generation = 0;
    var started = false;
    var names = ['sleep', 'epaper', 'storage', 'auth', 'user_files', 'mdns', 'battery', 'console'];

    function notify() {
        listeners.slice().forEach(function (listener) { listener(); });
    }
    function refresh() {
        if (request) { return request; }
        var epoch = generation;
        var pending = app.device.api.resources.features().then(function (data) {
            var next = data && data.features;
            if (!next || !names.every(function (name) { return typeof next[name] === 'boolean'; })) {
                throw new Error('Invalid feature response');
            }
            if (epoch === generation) {
                var changed = JSON.stringify(values) !== JSON.stringify(next);
                values = Object.assign({}, next);
                if (changed) { notify(); }
            }
            return values;
        }).catch(function () {
            // An unreachable device never implies that authentication is disabled.
            return values;
        }).finally(function () { if (request === pending) { request = null; } });
        request = pending;
        return pending;
    }
    app.device.features = {
        known: function () { return values !== null; },
        supports: function (name) { return values !== null && values[name] === true; },
        disabled: function (name) { return values !== null && values[name] === false; },
        refresh: refresh,
        subscribe: function (listener) {
            listeners.push(listener);
            return function () { var index = listeners.indexOf(listener); if (index >= 0) { listeners.splice(index, 1); } };
        },
        start: function () {
            if (started) { return; }
            started = true;
            app.device.live.subscribe(function (state) {
                if (state === 'online') { refresh(); }
                else if (state === 'offline') { generation += 1; request = null; values = null; notify(); }
            });
            window.setInterval(function () {
                if (!document.hidden && app.device.live.state() === 'online') { refresh(); }
            }, 30000);
            refresh();
        }
    };
})(window.DitherApp);
