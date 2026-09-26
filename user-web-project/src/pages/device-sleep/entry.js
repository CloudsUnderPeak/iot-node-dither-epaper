(function (app) {
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany(['src/pages/device-sleep/page.js']).then(function () {
            app.app.pageRegistry.register(app.pages.deviceSleepPage);
        })
    );
})(window.DitherApp);
