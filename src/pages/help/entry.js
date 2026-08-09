(function (app) {
    // Help 頁入口依序載入專屬雙語內容、文件樹與視覺元件。
    app.app.registerPageEntry(
        app.app.scriptLoader.loadMany([
            'src/pages/help/i18n/en.js',
            'src/pages/help/i18n/zh-TW.js',
            'src/pages/help/document-manifest.js',
            'src/pages/help/content-model.js',
            'src/pages/help/validation.js',
            'src/pages/help/visuals.js',
            'src/pages/help/page.js'
        ]).then(function () {
            var validation = app.pages.help.validation.validate();
            validation.errors.concat(validation.warnings).forEach(function (message) {
                window.console.warn('[Help] ' + message);
            });
            app.app.pageRegistry.register(app.pages.helpPage);
        })
    );
})(window.DitherApp);
