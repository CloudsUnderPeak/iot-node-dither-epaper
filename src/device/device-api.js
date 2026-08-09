(function (app) {
    // iot-node-bedrock 裝置 REST client：統一 envelope 解析、Bearer token 與 timeout。
    // 頁面一律透過 resources 取用 endpoint，不直接呼叫 fetch。
    // 部署後與裝置 API 同源，因此 path 固定使用相對路徑。
    var DEFAULT_TIMEOUT_MS = 10000;
    var unauthorizedListeners = [];
    var token = readToken();

    function readToken() {
        try {
            return localStorage.getItem(app.core.storageKeys.deviceToken) || '';
        } catch (error) {
            return '';
        }
    }

    // token 由裝置 runtime 保存，重開機或他人登入即失效；本地只是快取。
    function setToken(nextToken) {
        token = nextToken || '';
        try {
            if (token) {
                localStorage.setItem(app.core.storageKeys.deviceToken, token);
            } else {
                localStorage.removeItem(app.core.storageKeys.deviceToken);
            }
        } catch (error) {}
    }

    // 把 envelope 錯誤與網路層失敗統一成帶 status/code/fields 的 Error。
    function apiError(message, status, data) {
        var error = new Error(message || 'Device API error');
        error.status = status;
        error.data = data || {};
        error.code = error.data.code || '';
        error.fields = Array.isArray(error.data.fields) ? error.data.fields : [];
        return error;
    }

    function notifyUnauthorized() {
        unauthorizedListeners.slice().forEach(function (listener) {
            listener();
        });
    }

    // 發送 request 並解析統一 envelope；HTTP 200 但 success !== true 也視為錯誤。
    function request(method, path, options) {
        options = options || {};
        var headers = { Accept: 'application/json' };
        var usedToken = options.auth !== false && !!token;
        if (usedToken) {
            headers.Authorization = 'Bearer ' + token;
        }
        var init = { method: method, headers: headers };
        if (options.json !== undefined) {
            headers['Content-Type'] = 'application/json';
            init.body = JSON.stringify(options.json);
        } else if (options.body !== undefined) {
            if (options.contentType) {
                headers['Content-Type'] = options.contentType;
            }
            init.body = options.body;
        }
        var timeoutId = null;
        if (typeof AbortController === 'function') {
            var controller = new AbortController();
            init.signal = controller.signal;
            timeoutId = window.setTimeout(function () {
                controller.abort();
            }, options.timeoutMs || DEFAULT_TIMEOUT_MS);
        }
        return fetch(path, init)
            .catch(function (cause) {
                // fetch 例外＝網路層失敗；讓 live monitor 立即補查連線狀態。
                if (app.device.live) {
                    app.device.live.noteRequestFailure();
                }
                throw apiError(cause && cause.message ? cause.message : 'Network request failed', 0, { code: 'transport_error' });
            })
            .then(function (response) {
                return response.json()
                    .catch(function () {
                        return null;
                    })
                    .then(function (payload) {
                        if (!response.ok || !payload || payload.success !== true) {
                            // 只有帶著 token 的 request 收到 401 才代表 session 失效；
                            // login 本身的 401 是帳密錯誤，不觸發全域登出。
                            if (response.status === 401 && usedToken) {
                                notifyUnauthorized();
                            }
                            throw apiError(
                                payload && payload.message ? payload.message : 'HTTP ' + response.status,
                                response.status,
                                payload && payload.data
                            );
                        }
                        // 任何成功回應都證明裝置在線。
                        if (app.device.live) {
                            app.device.live.noteSuccess();
                        }
                        return payload.data || {};
                    });
            })
            .finally(function () {
                if (timeoutId !== null) {
                    window.clearTimeout(timeoutId);
                }
            });
    }

    // 常見 error code 轉成使用者語言；API message 只作 fallback。
    app.device.errorText = function errorText(error) {
        var errorKeys = {
            epaper_busy: 'epaperErrorBusy',
            invalid_epaper_image: 'epaperErrorInvalidImage',
            epaper_image_not_found: 'epaperErrorImageNotFound',
            epaper_unavailable: 'epaperErrorUnavailable',
            storage_busy: 'epaperErrorStorageBusy',
            storage_unavailable: 'epaperErrorStorageUnavailable',
            insufficient_storage: 'epaperErrorInsufficientStorage',
            storage_error: 'epaperErrorStorage'
        };
        if (error && error.code === 'transport_error') {
            return app.i18n.t('deviceErrorUnreachable');
        }
        if (error && error.code === 'unauthorized') {
            return app.i18n.t('deviceSessionExpired');
        }
        if (error && errorKeys[error.code]) {
            return app.i18n.t(errorKeys[error.code]);
        }
        if (error && error.message) {
            return error.message;
        }
        return app.i18n.t('errorGeneric');
    };

    app.device.api = {
        request: request,
        hasToken: function hasToken() {
            return !!token;
        },
        setToken: setToken,
        // session 失效（401）的全域通知；device-auth 訂閱後負責清 token 與 UI 鎖定。
        onUnauthorized: function onUnauthorized(listener) {
            unauthorizedListeners.push(listener);
        },
        resources: {
            alive: function alive() {
                return request('GET', 'api/alive', { auth: false, timeoutMs: 2500 });
            },
            device: function device() {
                return request('GET', 'api/device', { auth: false });
            },
            storage: function storage() {
                return request('GET', 'api/storage', { auth: false });
            },
            epaperCapabilities: function epaperCapabilities() {
                return request('GET', 'api/epaper', { auth: false, timeoutMs: 4000 });
            },
            epaperStatus: function epaperStatus() {
                return request('GET', 'api/epaper/status', { auth: false, timeoutMs: 4000 });
            },
            epaperUpload: function epaperUpload(payload) {
                return request('POST', 'api/epaper/image', {
                    auth: false,
                    body: payload,
                    contentType: 'application/octet-stream',
                    timeoutMs: 30000
                });
            },
            epaperWhite: function epaperWhite() {
                return request('POST', 'api/epaper/image/white', { auth: false, timeoutMs: 10000 });
            },
            epaperPalette: function epaperPalette() {
                return request('POST', 'api/epaper/image/palette', { auth: false, timeoutMs: 10000 });
            },
            epaperRefresh: function epaperRefresh() {
                return request('POST', 'api/epaper/image/refresh', { auth: false, timeoutMs: 10000 });
            },
            wifi: function wifi() {
                return request('GET', 'api/wifi', { auth: false });
            },
            wifiScan: function wifiScan() {
                return request('GET', 'api/wifi/scan', { timeoutMs: 20000 });
            },
            wifiUpdate: function wifiUpdate(payload) {
                return request('PUT', 'api/wifi', { json: payload });
            },
            wifiConnectStatus: function wifiConnectStatus() {
                return request('GET', 'api/wifi/connect', { timeoutMs: 3000 });
            },
            authInfo: function authInfo() {
                return request('GET', 'api/auth', { auth: false });
            },
            login: function login(username, password) {
                return request('POST', 'api/auth/login', { auth: false, json: { username: username, password: password } });
            },
            session: function session() {
                return request('GET', 'api/auth/session');
            },
            logout: function logout() {
                return request('POST', 'api/auth/logout', { json: {} });
            },
            changePassword: function changePassword(password) {
                return request('PUT', 'api/auth/password', { json: { password: password } });
            },
            systemUpdate: function systemUpdate(payload) {
                return request('PUT', 'api/system', { json: payload });
            },
            // 完整重設；裝置在回應後立即重啟，呼叫端必須自行作廢本地 session。
            systemReset: function systemReset() {
                return request('POST', 'api/system/reset', { json: {} });
            }
        }
    };
})(window.DitherApp);
