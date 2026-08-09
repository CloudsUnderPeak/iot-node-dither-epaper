(function (app) {
    // 每個頁面都有自己的 entry，負責載入 page.js 與內部模組並註冊到全域 pageRegistry。
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany([
            'src/pages/device-network/wifi-form.js',
            'src/pages/device-network/wifi-controller.js',
            'src/pages/device-network/scan-dialog.js',
            'src/pages/device-network/page.js'
        ]).then(function () {
            app.app.pageRegistry.register(app.pages.deviceNetworkPage);
        })
    );
})(window.DitherApp);
