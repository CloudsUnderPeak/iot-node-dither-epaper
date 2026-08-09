(function (app) {
    // Dither Editor page entry 統一管理本頁需要的 script 載入順序。
    // index.html 只載入這個入口，避免把頁面內部檔案全部攤平在 HTML。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    var bootstrapScripts = [
        'src/pages/dither-editor/config/app-mode.js',
        'src/pages/dither-editor/config/palette-presets.js',
        'src/pages/dither-editor/dither/dither-algorithm-registry.js',
        'src/pages/dither-editor/config/dither-algorithms.js',
        'src/pages/dither-editor/config/color-distance-metrics.js',
        'src/pages/dither-editor/config/palette-mapping-modes.js',
        'src/pages/dither-editor/config/pipeline-presets.js',
        'src/pages/dither-editor/config/display-profiles.js',
        'src/pages/dither-editor/config/target-policy.js',
        'src/core/encoders/epdimg-encoder.js',
        'src/vendor/rgbquant.js',
        'src/pages/dither-editor/dither/rgbquant-adapter.js',
        'src/pages/dither-editor/dither/dither-matrices.js',
        'src/pages/dither-editor/dither/palette-mapping.js',
        'src/pages/dither-editor/gpu/gl-helpers.js',
        'src/pages/dither-editor/gpu/threshold-dither-processor.js',
        'src/pages/dither-editor/dither/error-diffusion.js',
        'src/pages/dither-editor/dither/ordered-dither.js',
        'src/pages/dither-editor/dither/pattern-dither.js',
        'src/pages/dither-editor/dither/dot-diffusion.js',
        'src/pages/dither-editor/gpu/adjust-processor.js',
        'src/pages/dither-editor/operations/operation-registry.js',
        'src/pages/dither-editor/constants.js',
        'src/pages/dither-editor/feature-manifest.js',
        'src/pages/dither-editor/feature-registry.js',
        'src/pages/dither-editor/panel-utils.js'
    ];

    var pageScripts = [
        'src/pages/dither-editor/worker/dither-worker-client.js',
        'src/pages/dither-editor/operations/pipeline-runner.js',
        'src/pages/dither-editor/editor-mode-state-machine.js',
        'src/pages/dither-editor/state.js',
        'src/pages/dither-editor/viewport/viewport-renderer.js',
        'src/pages/dither-editor/viewport/overlay-renderer.js',
        'src/pages/dither-editor/viewport/pointer-mapper.js',
        'src/pages/dither-editor/viewport/pixel-preview-drag.js',
        'src/pages/dither-editor/preview-toolbar.js',
        'src/pages/dither-editor/controller.js',
        'src/pages/dither-editor/page.js'
    ];

    function loadFeatureAndPageScripts() {
        // 同批插入 feature 與 page scripts，減少 GitHub Pages 上的序列化網路波次；
        // scriptLoader 會用 async=false 保持 classic script 依插入順序執行。
        return app.app.scriptLoader.loadMany(
            app.pages.ditherEditor.featureRegistry.featureScripts().concat(pageScripts)
        );
    }

    app.app.registerPageEntry(
        app.app.scriptLoader
            .loadMany(bootstrapScripts)
            .then(loadFeatureAndPageScripts)
            .then(function () {
                app.pages.ditherEditor.ditherAlgorithmRegistry.assertRegistered();
                app.pages.ditherEditor.featureRegistry.assertRegistered();
            })
            .then(function () {
                app.app.pageRegistry.register(app.pages.ditherEditorPage);
            })
    );
})(window.DitherApp);
