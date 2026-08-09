(function (app) {
    // Palette presets 是可套用到 palette feature 的靜態色票。
    // Original/custom 不放在這裡，因為它們分別由圖片萃取與使用者編輯動態產生。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.palettePresets = [
        {
            id: 'monochrome',
            labelKey: 'paletteMonochrome',
            colors: [
                { r: 0, g: 0, b: 0 },
                { r: 255, g: 255, b: 255 }
            ]
        },
        {
            id: 'game-boy',
            labelKey: 'paletteGameBoy',
            colors: [
                { r: 15, g: 56, b: 15 },
                { r: 48, g: 98, b: 48 },
                { r: 139, g: 172, b: 15 },
                { r: 155, g: 188, b: 15 }
            ]
        },
        {
            id: 'warm-ink',
            labelKey: 'paletteWarmInk',
            colors: [
                { r: 31, g: 27, b: 24 },
                { r: 107, g: 73, b: 57 },
                { r: 204, g: 172, b: 124 },
                { r: 244, g: 239, b: 220 }
            ]
        },
        {
            id: 'e6-color-epaper',
            labelKey: 'paletteE6ColorEpaper',
            colors: [
                { r: 0, g: 0, b: 0 },
                { r: 255, g: 255, b: 255 },
                { r: 255, g: 0, b: 0 },
                { r: 255, g: 255, b: 0 },
                { r: 0, g: 0, b: 255 },
                { r: 0, g: 255, b: 0 }
            ]
        }
    ];
})(window.DitherApp);
