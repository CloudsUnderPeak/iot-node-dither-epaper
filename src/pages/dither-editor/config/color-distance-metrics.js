(function (app) {
    // Color distance metrics 控制 palette 最近色搜尋的比較方式。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.colorDistanceMetrics = [
        {
            id: 'euclidean-bt709',
            labelKey: 'colorDistanceEuclideanBt709'
        },
        {
            id: 'euclidean-rgb',
            labelKey: 'colorDistanceEuclideanRgb'
        },
        {
            id: 'manhattan-bt709',
            labelKey: 'colorDistanceManhattanBt709'
        },
        {
            id: 'manhattan-rgb',
            labelKey: 'colorDistanceManhattanRgb'
        },
        {
            id: 'ciede2000',
            labelKey: 'colorDistanceCiede2000'
        }
    ];
})(window.DitherApp);
