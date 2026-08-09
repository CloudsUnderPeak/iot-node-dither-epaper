(function (app) {
    // 所有 localStorage key 集中在這裡，避免設定 key 散落各處。
    app.core.storageKeys = {
        settings: 'dither-app:settings:v1',
        deviceToken: 'dither-app:device-token:v1',
        schemaVersion: 1
    };
})(window.DitherApp);
