(function (app) {
    var PALETTE_ID = 'e6-color-epaper';
    // OUTPUT_COLORS 是 EPDIMG encoder 的協定色 A；不得為了肉眼校色修改。
    var OUTPUT_COLORS = [
        { r: 0, g: 0, b: 0 },
        { r: 255, g: 255, b: 255 },
        { r: 255, g: 0, b: 0 },
        { r: 255, g: 255, b: 0 },
        { r: 0, g: 0, b: 255 },
        { r: 0, g: 255, b: 0 }
    ];

    // DISPLAY_COLORS 是實體面板肉眼呈現的參考色 A′，同時供網頁預覽、色距與抖動使用。
    // 輸出前會依相同 index 精確轉回 OUTPUT_COLORS；校色時只修改這六組 RGB。
    // 順序必須維持：黑、白、紅、黃、藍、綠。
    var DISPLAY_COLORS = [
        { r: 24, g: 24, b: 22 },
        { r: 242, g: 239, b: 222 },
        { r: 194, g: 49, b: 43 },
        { r: 230, g: 194, b: 55 },
        { r: 45, g: 79, b: 137 },
        { r: 54, g: 125, b: 71 }
    ];
    var displayImageCache = new WeakMap();
    var outputImageCache = new WeakMap();

    function copyColors(colors) {
        return colors.map(function (color) { return Object.assign({}, color); });
    }

    function colorIndex(colors, r, g, b) {
        for (var index = 0; index < colors.length; index += 1) {
            var color = colors[index];
            if (r === color.r && g === color.g && b === color.b) {
                return index;
            }
        }
        return -1;
    }

    function outputColorIndex(color) {
        return color
            ? colorIndex(OUTPUT_COLORS, color.r, color.g, color.b)
            : -1;
    }

    function displayColorIndex(color) {
        return color
            ? colorIndex(DISPLAY_COLORS, color.r, color.g, color.b)
            : -1;
    }

    function displayColor(color) {
        var index = outputColorIndex(color);
        return Object.assign({}, index === -1 ? color : DISPLAY_COLORS[index]);
    }

    function displayImageData(imageData) {
        if (!imageData || !imageData.data) {
            return imageData;
        }
        var cached = displayImageCache.get(imageData);
        if (cached) {
            return cached;
        }
        var data = new Uint8ClampedArray(imageData.data);
        for (var offset = 0; offset < data.length; offset += 4) {
            var index = colorIndex(OUTPUT_COLORS, data[offset], data[offset + 1], data[offset + 2]);
            if (index !== -1) {
                data[offset] = DISPLAY_COLORS[index].r;
                data[offset + 1] = DISPLAY_COLORS[index].g;
                data[offset + 2] = DISPLAY_COLORS[index].b;
            }
        }
        var displayed = new ImageData(data, imageData.width, imageData.height);
        displayImageCache.set(imageData, displayed);
        return displayed;
    }

    // Pipeline 以 A′ 運算；只有送入 EPDIMG 前才把每個 palette index 正規化為協定色 A。
    function outputImageData(imageData) {
        if (!imageData || !imageData.data) {
            return imageData;
        }
        var cached = outputImageCache.get(imageData);
        if (cached) {
            return cached;
        }
        var data = new Uint8ClampedArray(imageData.data);
        for (var offset = 0; offset < data.length; offset += 4) {
            var index = colorIndex(DISPLAY_COLORS, data[offset], data[offset + 1], data[offset + 2]);
            if (index === -1) {
                index = colorIndex(OUTPUT_COLORS, data[offset], data[offset + 1], data[offset + 2]);
            }
            if (index === -1) {
                throw new Error('E-paper pipeline output contains a color outside the calibrated six-color palette.');
            }
            data[offset] = OUTPUT_COLORS[index].r;
            data[offset + 1] = OUTPUT_COLORS[index].g;
            data[offset + 2] = OUTPUT_COLORS[index].b;
        }
        var output = new ImageData(data, imageData.width, imageData.height);
        outputImageCache.set(imageData, output);
        return output;
    }

    function isEpaper(state) {
        return Boolean(state && state.target && state.target.mode === 'epaper');
    }

    function force(state) {
        if (!state || !state.settings || !app.device.epaper.isSupported()) {
            return false;
        }
        var crop = state.settings.crop;
        var portrait = crop && crop.aspectRatioId === '3-5';
        if (crop && crop.aspectRatioId !== '5-3' && crop.aspectRatioId !== '3-5') {
            crop.aspectRatioId = '5-3';
            portrait = false;
        }
        state.target = { mode: 'epaper', orientation: portrait ? 'portrait' : 'landscape' };
        if (state.settings.resize) {
            Object.assign(state.settings.resize, portrait
                ? { width: 480, height: 800, aspectRatio: 3 / 5 }
                : { width: 800, height: 480, aspectRatio: 5 / 3 });
        }
        if (state.settings.palette) {
            state.settings.palette.presetId = PALETTE_ID;
            state.settings.palette.palette = copyColors(DISPLAY_COLORS);
        }
        return true;
    }

    function sync(state) {
        if (app.device.epaper.isSupported()) {
            return force(state);
        }
        if (!state.target) {
            state.target = { mode: 'standalone' };
        }
        return false;
    }

    function settingAllowed(state, group, key, value) {
        if (!isEpaper(state)) {
            return true;
        }
        if (group === 'resize' || group === 'palette') {
            return false;
        }
        return group !== 'crop' || key !== 'aspectRatioId' || value === '5-3' || value === '3-5';
    }

    app.pages.ditherEditor.targetPolicy = {
        isEpaper: isEpaper,
        sync: sync,
        normalizeBeforePipeline: force,
        settingAllowed: settingAllowed,
        paletteId: PALETTE_ID,
        colors: DISPLAY_COLORS,
        outputColors: OUTPUT_COLORS,
        displayColors: DISPLAY_COLORS,
        displayColor: displayColor,
        displayImageData: displayImageData,
        outputImageData: outputImageData,
        outputColorIndex: outputColorIndex,
        displayColorIndex: displayColorIndex
    };
})(window.DitherApp);
