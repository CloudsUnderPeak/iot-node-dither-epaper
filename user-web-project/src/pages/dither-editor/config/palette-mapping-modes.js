(function (app) {
    // Palette Mapping 控制 dither processor 如何把輸入顏色映射到固定 palette。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.paletteMappingModes = [
        {
            id: 'nearest-color',
            labelKey: 'paletteMappingNearestColor'
        },
        {
            id: 'pair-mix',
            labelKey: 'paletteMappingPairMix'
        },
        {
            id: 'tri-mix',
            labelKey: 'paletteMappingTriMix'
        }
    ];
})(window.DitherApp);
