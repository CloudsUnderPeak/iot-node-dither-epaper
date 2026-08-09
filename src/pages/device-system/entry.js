(function (app) {
    // 每個頁面都有自己的 entry，負責載入 page.js 並註冊到全域 pageRegistry。
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany(['src/pages/device-system/page.js']).then(function () {
            app.app.pageRegistry.register(app.pages.deviceSystemPage);
        })
    );
})(window.DitherApp);
