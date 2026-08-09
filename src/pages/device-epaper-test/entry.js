(function (app) {
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany(['src/pages/device-epaper-test/page.js']).then(function () {
            app.app.pageRegistry.register(app.pages.deviceEpaperTestPage);
        })
    );
})(window.DitherApp);
