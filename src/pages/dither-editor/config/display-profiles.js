(function (app) {
    // Display profiles 描述目標顯示器尺寸與輸出特性。
    // 目前主要作為新圖、初始 resize/crop 的尺寸來源，不直接耦合特定硬體 API。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.displayProfiles = [
        {
            id: 'epaper-color-800x480',
            labelKey: 'displayEpaperColor800x480',
            width: 800,
            height: 480,
            aspectRatio: 800 / 480,
            colorMode: 'color',
            refreshMode: 'direct',
            outputFormat: 'device-native'
        },
        {
            id: 'epaper-color-custom',
            labelKey: 'displayEpaperColorCustom',
            width: null,
            height: null,
            aspectRatio: null,
            colorMode: 'color',
            refreshMode: 'direct',
            outputFormat: 'device-native'
        }
    ];
})(window.DitherApp);
