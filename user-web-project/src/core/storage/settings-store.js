(function (app) {
    // localStorage 只保存輕量 app preference。
    // 圖片、canvas 與 Dither Editor workspace 不做跨重新整理持久化。
    // 解析 localStorage JSON，並確認 schema key 相符。
    function parseSettings(raw, keys) {
        if (!raw) {
            return null;
        }
        try {
            var value = JSON.parse(raw);
            // schema 不符時回傳 null，讓呼叫端用 default state；避免舊資料造成啟動失敗。
            if (value.schemaVersion !== keys.schemaVersion) {
                return null;
            }
            return value;
        } catch (error) {
            return null;
        }
    }

    // 從 localStorage 讀取網站偏好；瀏覽器阻擋時回傳 null。
    function loadLocalStorage(keys) {
        try {
            return parseSettings(localStorage.getItem(keys.settings), keys);
        } catch (error) {
            return null;
        }
    }

    // 將網站偏好寫入 localStorage；失敗時安靜略過，避免阻斷主流程。
    function saveLocalStorage(value, keys) {
        try {
            localStorage.setItem(keys.settings, JSON.stringify(value));
        } catch (error) {}
    }

    app.core.settingsStore = {
        // 對外讀取目前設定。
        load: function load() {
            var keys = app.core.storageKeys;
            return loadLocalStorage(keys);
        },
        // 對外儲存目前設定。
        save: function save(value) {
            var keys = app.core.storageKeys;
            var nextValue = Object.assign({ schemaVersion: keys.schemaVersion }, value);
            saveLocalStorage(nextValue, keys);
        }
    };
})(window.DitherApp);
