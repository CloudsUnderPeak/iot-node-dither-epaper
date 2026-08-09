(function (app) {
    // 裝置連線監看：定期打 GET /api/alive，作為全站「裝置是否在線」的單一事實來源。
    // 連續 2 次失敗才判離線，避免單次 timeout 誤判；任何 API 成功都視為 alive。
    var POLL_INTERVAL_MS = 5000;
    var OFFLINE_AFTER_FAILURES = 2;

    // checking：啟動後首次確認前。standalone：從未成功連線（開發環境）。
    var state = 'checking';
    var everOnline = false;
    var failures = 0;
    var suppressReasons = {};
    var listeners = [];
    var timerId = null;
    var checking = false;
    var started = false;
    var lastOnlineAt = 0;

    function notify() {
        listeners.slice().forEach(function (listener) {
            listener(state);
        });
    }

    function setState(next) {
        if (state === next) {
            return;
        }
        state = next;
        notify();
    }

    function suppressed() {
        return Object.keys(suppressReasons).length > 0;
    }

    function noteSuccess() {
        failures = 0;
        everOnline = true;
        lastOnlineAt = Date.now();
        setState('online');
    }

    function noteCheckFailure() {
        failures += 1;
        // Wi-Fi 套用等已知會短暫斷線的流程中不判離線，由該流程自行呈現進度。
        if (suppressed()) {
            return;
        }
        if (!everOnline) {
            setState('standalone');
            return;
        }
        if (failures >= OFFLINE_AFTER_FAILURES) {
            setState('offline');
        }
    }

    function check() {
        if (checking) {
            return;
        }
        checking = true;
        // 成功路徑由 device-api 的成功 hook 呼叫 noteSuccess。
        app.device.api.resources.alive().then(
            function () {
                checking = false;
            },
            function () {
                checking = false;
                noteCheckFailure();
            }
        );
    }

    function startTimer() {
        if (timerId === null) {
            timerId = window.setInterval(check, POLL_INTERVAL_MS);
        }
    }

    function stopTimer() {
        if (timerId !== null) {
            window.clearInterval(timerId);
            timerId = null;
        }
    }

    app.device.live = {
        state: function currentState() {
            return state;
        },
        lastOnlineAt: function onlineAt() {
            return lastOnlineAt;
        },
        // 回傳 unsubscribe，讓頁面 unmount 時解除訂閱。
        subscribe: function subscribe(listener) {
            listeners.push(listener);
            return function unsubscribe() {
                var index = listeners.indexOf(listener);
                if (index !== -1) {
                    listeners.splice(index, 1);
                }
            };
        },
        start: function start() {
            if (started) {
                return;
            }
            started = true;
            // 分頁隱藏時暫停輪詢；恢復可見立即補查一次。
            document.addEventListener('visibilitychange', function () {
                if (document.hidden) {
                    stopTimer();
                } else {
                    check();
                    startTimer();
                }
            });
            check();
            startTimer();
        },
        noteSuccess: noteSuccess,
        // 一般 API 網路層失敗時立即補查，縮短斷線發現時間；
        // alive 檢查自己的失敗（checking 中）不重入。
        noteRequestFailure: function noteRequestFailure() {
            if (started && !checking && state !== 'offline' && state !== 'standalone') {
                check();
            }
        },
        // 已知會短暫斷線的流程（例如 Wi-Fi safe transition）暫停離線判定。
        suppress: function suppress(reason) {
            suppressReasons[reason] = true;
        },
        release: function release(reason) {
            delete suppressReasons[reason];
            if (started && !suppressed()) {
                failures = 0;
                check();
            }
        }
    };
})(window.DitherApp);
