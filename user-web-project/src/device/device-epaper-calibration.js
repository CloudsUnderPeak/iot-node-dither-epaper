(function (app) {
    var DEFAULT_COLORS = [
        { id: 'black', code: 0, r: 39, g: 39, b: 43 },
        { id: 'white', code: 1, r: 237, g: 237, b: 225 },
        { id: 'yellow', code: 2, r: 224, g: 212, b: 31 },
        { id: 'red', code: 3, r: 120, g: 32, b: 32 },
        { id: 'blue', code: 5, r: 31, g: 88, b: 169 },
        { id: 'green', code: 6, r: 58, g: 110, b: 72 }
    ];
    var listeners = [];
    var started = false;
    var epaperUnsubscribe = null;
    var liveUnsubscribe = null;
    var requestSerial = 0;
    var loadRequest = null;
    var state = {
        colors: copyColors(DEFAULT_COLORS),
        source: 'default',
        recoveryReason: 'none',
        revision: 0,
        loading: false,
        saving: false,
        synced: false,
        error: null
    };

    function copyColors(colors) {
        return colors.map(function (color) {
            return {
                id: color.id,
                code: color.code,
                r: color.r,
                g: color.g,
                b: color.b
            };
        });
    }

    function snapshot() {
        return {
            colors: copyColors(state.colors),
            source: state.source,
            recoveryReason: state.recoveryReason,
            revision: state.revision,
            loading: state.loading,
            saving: state.saving,
            synced: state.synced,
            error: state.error
        };
    }

    function notify() {
        var current = snapshot();
        listeners.slice().forEach(function (listener) {
            listener(current);
        });
    }

    function subscribe(listener) {
        listeners.push(listener);
        return function unsubscribe() {
            var index = listeners.indexOf(listener);
            if (index !== -1) {
                listeners.splice(index, 1);
            }
        };
    }

    function validationError(message) {
        var error = new Error(message || 'Invalid e-paper calibration');
        error.code = 'invalid_calibration';
        return error;
    }

    function channel(value) {
        return Number.isInteger(value) && value >= 0 && value <= 255 ? value : null;
    }

    function normalizeColors(colors) {
        if (!Array.isArray(colors) || colors.length !== DEFAULT_COLORS.length) {
            throw validationError('Calibration must contain exactly six colors.');
        }
        var byId = {};
        colors.forEach(function (color) {
            if (!color || typeof color.id !== 'string' || byId[color.id]) {
                throw validationError('Calibration color ids must be unique.');
            }
            byId[color.id] = color;
        });
        var seenRgb = {};
        return DEFAULT_COLORS.map(function (definition) {
            var source = byId[definition.id];
            var display = source && source.display ? source.display : source;
            if (!source || (source.code !== undefined && source.code !== definition.code)) {
                throw validationError('Calibration color identity does not match the panel profile.');
            }
            var r = channel(display && display.r);
            var g = channel(display && display.g);
            var b = channel(display && display.b);
            if (r === null || g === null || b === null) {
                throw validationError('Calibration channels must be integers from 0 to 255.');
            }
            var rgbKey = r + ',' + g + ',' + b;
            if (seenRgb[rgbKey]) {
                throw validationError('Calibration display colors must be unique.');
            }
            seenRgb[rgbKey] = true;
            return { id: definition.id, code: definition.code, r: r, g: g, b: b };
        });
    }

    function colorsEqual(left, right) {
        return left.length === right.length && left.every(function (color, index) {
            var other = right[index];
            return color.id === other.id && color.code === other.code
                && color.r === other.r && color.g === other.g && color.b === other.b;
        });
    }

    function applyResponse(data) {
        var colors = normalizeColors(data && data.colors);
        if (!colorsEqual(colors, state.colors)) {
            state.colors = colors;
            state.revision += 1;
        }
        state.source = data && data.source || 'default';
        state.recoveryReason = data && data.recovery_reason || 'none';
        state.synced = true;
        state.error = null;
    }

    function payloadFor(colors) {
        var payload = { colors: {} };
        normalizeColors(colors).forEach(function (color) {
            payload.colors[color.id] = { r: color.r, g: color.g, b: color.b };
        });
        return payload;
    }

    function load(force) {
        if (!app.device.epaper.isSupported() || app.device.live.state() !== 'online') {
            return Promise.resolve(snapshot());
        }
        if (!force && state.synced) {
            return Promise.resolve(snapshot());
        }
        if (loadRequest) {
            return loadRequest;
        }
        var serial = ++requestSerial;
        state.loading = true;
        state.error = null;
        notify();
        loadRequest = app.device.api.resources.epaperCalibration()
            .then(function (data) {
                if (serial === requestSerial) {
                    applyResponse(data);
                }
                return snapshot();
            })
            .catch(function (error) {
                if (serial === requestSerial) {
                    state.synced = false;
                    state.error = error;
                }
                throw error;
            })
            .finally(function () {
                if (serial === requestSerial) {
                    state.loading = false;
                    notify();
                }
                loadRequest = null;
            });
        return loadRequest;
    }

    function save(colors) {
        var payload;
        try {
            payload = payloadFor(colors);
        } catch (error) {
            return Promise.reject(error);
        }
        var serial = ++requestSerial;
        state.loading = false;
        state.saving = true;
        state.error = null;
        notify();
        return app.device.api.resources.updateEpaperCalibration(payload)
            .then(function (data) {
                if (serial === requestSerial) {
                    applyResponse(data);
                }
                return snapshot();
            })
            .catch(function (error) {
                if (serial === requestSerial) {
                    state.error = error;
                }
                throw error;
            })
            .finally(function () {
                if (serial === requestSerial) {
                    state.saving = false;
                    notify();
                }
            });
    }

    function reset() {
        var serial = ++requestSerial;
        state.loading = false;
        state.saving = true;
        state.error = null;
        notify();
        return app.device.api.resources.resetEpaperCalibration()
            .then(function (data) {
                if (serial === requestSerial) {
                    applyResponse(data);
                }
                return snapshot();
            })
            .catch(function (error) {
                if (serial === requestSerial) {
                    state.error = error;
                }
                throw error;
            })
            .finally(function () {
                if (serial === requestSerial) {
                    state.saving = false;
                    notify();
                }
            });
    }

    function syncWhenAvailable() {
        if (app.device.epaper.isSupported() && app.device.live.state() === 'online') {
            load(false).catch(function () {});
        }
    }

    function start() {
        if (started) {
            return;
        }
        started = true;
        epaperUnsubscribe = app.device.epaper.subscribe(syncWhenAvailable);
        liveUnsubscribe = app.device.live.subscribe(function (liveState) {
            if (liveState === 'offline' || liveState === 'standalone') {
                state.synced = false;
                notify();
                return;
            }
            syncWhenAvailable();
        });
        syncWhenAvailable();
    }

    app.device.epaperCalibration = {
        start: start,
        load: load,
        save: save,
        reset: reset,
        snapshot: snapshot,
        subscribe: subscribe,
        colors: function colors() { return copyColors(state.colors); },
        defaults: function defaults() { return copyColors(DEFAULT_COLORS); },
        revision: function revision() { return state.revision; },
        validate: normalizeColors
    };
})(window.DitherApp);
