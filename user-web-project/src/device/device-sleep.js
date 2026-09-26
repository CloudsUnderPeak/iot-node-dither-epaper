(function (app) {
    var POLL_MS = 15000;
    var CLOCK_SYNC_MS = 600000;
    var listeners = [];
    var supported = false;
    var known = false;
    var status = null;
    var error = null;
    var receivedAt = 0;
    var request = null;
    var timer = null;
    var keepBusy = false;
    var generation = 0;
    var lastClockAttempt = 0;
    var started = false;

    function monotonic() {
        return window.performance && window.performance.now ? window.performance.now() : Date.now();
    }

    function online() {
        return app.device.live.state() === 'online' && !document.hidden;
    }

    function countdown() {
        if (!status || !online()) { return null; }
        var idle = status.idle;
        if (!status.enabled || status.mode !== 'normal' || !idle || idle.armed !== true
            || typeof idle.remaining_seconds !== 'number') { return null; }
        return Math.max(0, Math.ceil(idle.remaining_seconds - (monotonic() - receivedAt) / 1000));
    }

    function snapshot() {
        return {
            known: known,
            supported: supported,
            status: status,
            error: error,
            remainingSeconds: countdown(),
            keepBusy: keepBusy
        };
    }

    function notify() {
        var current = snapshot();
        listeners.slice().forEach(function (listener) { listener(current); });
    }

    function subscribe(listener) {
        listeners.push(listener);
        return function () {
            var index = listeners.indexOf(listener);
            if (index !== -1) { listeners.splice(index, 1); }
        };
    }

    function accept(next) {
        status = next;
        receivedAt = monotonic();
        error = null;
        notify();
        maybeSyncClock(next);
        return next;
    }

    function refresh() {
        if (!known || !supported || !online()) { return Promise.resolve(status); }
        if (request) { return request; }
        var epoch = generation;
        var pending = app.device.api.resources.sleepStatus().then(function (next) {
            if (epoch === generation) { accept(next); }
            return next;
        }, function (failure) {
            if (epoch === generation) { error = failure; notify(); }
            throw failure;
        }).finally(function () {
            if (request === pending) { request = null; }
        });
        request = pending;
        return pending;
    }

    function maybeSyncClock(device) {
        if (!app.device.auth.hasToken() || !online()) { return; }
        var now = Date.now();
        if (now - lastClockAttempt < CLOCK_SYNC_MS) { return; }
        var clock = device && device.time;
        var browserSeconds = Math.floor(now / 1000);
        if (clock && clock.synced === true && typeof clock.epoch === 'number'
            && Math.abs(browserSeconds - clock.epoch) <= 5) { return; }
        lastClockAttempt = now;
        var session = app.device.api.sessionEpoch();
        app.device.api.resources.systemTime(browserSeconds).then(function () {
            if (session === app.device.api.sessionEpoch()) { refresh().catch(function () {}); }
        }, function () {});
    }

    function discover() {
        if (!online()) { return Promise.resolve(false); }
        var epoch = ++generation;
        return app.device.api.resources.device().then(function (device) {
            if (epoch !== generation) { return false; }
            known = true;
            supported = Boolean(device && device.features && device.features.sleep_scheduler === true);
            status = null;
            error = null;
            notify();
            maybeSyncClock(device);
            if (supported) { refresh().catch(function () {}); }
            return supported;
        }, function (failure) {
            if (epoch === generation) { error = failure; notify(); }
            return false;
        });
    }

    function keepAwake() {
        if (keepBusy || !supported || !online()) { return Promise.reject(new Error('Sleep service unavailable')); }
        keepBusy = true;
        notify();
        var epoch = generation;
        return app.device.api.resources.sleepKeepAwake().then(function (result) {
            if (epoch === generation && status) {
                status = Object.assign({}, status, { idle: result.idle, mode: 'normal' });
                receivedAt = monotonic();
                error = null;
                notify();
            }
            return result;
        }).catch(function (failure) {
            if (epoch === generation) { error = failure; notify(); }
            throw failure;
        }).finally(function () { keepBusy = false; notify(); });
    }

    function update(payload) {
        return app.device.api.resources.sleepUpdate(payload).then(function (result) {
            lastClockAttempt = Date.now();
            accept(result);
            return result;
        });
    }

    function now() {
        return app.device.api.resources.sleepNow().then(function (result) {
            if (status) {
                status = Object.assign({}, status, {
                    sleep_request: result.sleep_request || { state: 'pending', error_code: null }
                });
                notify();
            }
            return result;
        });
    }

    function stopTimer() {
        if (timer !== null) { window.clearInterval(timer); timer = null; }
    }

    function startTimer() {
        if (timer === null) {
            timer = window.setInterval(function () {
                refresh().catch(function () {});
                notify();
            }, POLL_MS);
        }
    }

    function start() {
        if (started) { return; }
        started = true;
        app.device.live.subscribe(function (state) {
            generation += 1;
            request = null;
            if (state === 'online' && !document.hidden) {
                startTimer();
                discover();
            } else {
                stopTimer();
                status = null;
                receivedAt = 0;
                notify();
            }
        });
        app.device.auth.subscribe(function () {
            generation += 1;
            request = null;
            if (online()) { discover(); }
            else notify();
        });
        document.addEventListener('visibilitychange', function () {
            generation += 1;
            request = null;
            if (document.hidden) {
                stopTimer();
                notify();
            } else if (app.device.live.state() === 'online') {
                startTimer();
                discover();
            }
        });
        if (online()) { startTimer(); discover(); }
    }

    app.device.sleep = {
        start: start,
        subscribe: subscribe,
        snapshot: snapshot,
        refresh: refresh,
        discover: discover,
        keepAwake: keepAwake,
        update: update,
        now: now
    };
})(window.DitherApp);
