(function (app) {
    // Web Setting 頁入口維持與其他頁相同的 lazy page 載入模式。
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany(['src/pages/web-setting/page.js']).then(function () {
            app.app.pageRegistry.register(app.pages.webSettingPage);
        })
    );
})(window.DitherApp);
