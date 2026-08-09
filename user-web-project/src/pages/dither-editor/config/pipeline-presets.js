(function (app) {
    // Pipeline preset 只描述預設組合；實際有哪些 feature 由 feature manifest/registry 決定。
    // 目前 default 不硬寫 effectsOrder，讓 enabled features 可以自動組出 pipeline。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.pipelinePresets = [
        {
            id: 'default',
            labelKey: 'pipelineDefault'
        }
    ];
})(window.DitherApp);
