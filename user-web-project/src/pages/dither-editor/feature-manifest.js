(function (app) {
    // Feature manifest 是 Dither Editor 的功能開關與載入清單。
    // 要停用某個功能，原則上從這裡把 enabled 改為 false，而不是到 page/controller 到處刪 id。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.featureManifest = [
        {
            id: 'input',
            enabled: true,
            path: 'src/pages/dither-editor/features/input-feature.js',
            loadOrder: 10
        },
        {
            id: 'crop',
            enabled: true,
            // crop 由三支 script 組成：純幾何 → 背景取色 → feature 本體（依序載入）。
            paths: [
                'src/pages/dither-editor/features/crop-geometry.js',
                'src/pages/dither-editor/features/crop-auto-background.js',
                'src/pages/dither-editor/features/crop-feature.js'
            ],
            loadOrder: 20
        },
        {
            id: 'resize',
            enabled: true,
            path: 'src/pages/dither-editor/features/resize-feature.js',
            loadOrder: 30
        },
        {
            id: 'adjust',
            enabled: true,
            path: 'src/pages/dither-editor/features/adjust-feature.js',
            loadOrder: 40
        },
        {
            id: 'palette',
            enabled: true,
            path: 'src/pages/dither-editor/features/palette-feature.js',
            loadOrder: 50
        },
        {
            id: 'dither',
            enabled: true,
            path: 'src/pages/dither-editor/features/dither-feature.js',
            loadOrder: 60
        },
        {
            id: 'export',
            enabled: true,
            path: 'src/pages/dither-editor/features/export-feature.js',
            loadOrder: 70
        }
    ];
})(window.DitherApp);
