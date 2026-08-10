(function (app) {
    var PALETTE_ID = 'e6-color-epaper';
    // OUTPUT_COLORS 是 EPDIMG encoder 的協定色 A；依 EPD code 0、1、2、3、5、6 排列。
    var OUTPUT_COLORS = [
        { r: 0, g: 0, b: 0 },
        { r: 255, g: 255, b: 255 },
        { r: 255, g: 255, b: 0 },
        { r: 255, g: 0, b: 0 },
        { r: 0, g: 0, b: 255 },
        { r: 0, g: 255, b: 0 }
    ];

    // Defaults 只供 calibration service 尚未建立時 fallback；正式 canonical 由裝置 NVS API 提供。
    // 順序必須維持 EPD code 0、1、2、3、5、6：黑、白、黃、紅、藍、綠。
    var DEFAULT_DISPLAY_COLORS = [
        { r: 39, g: 39, b: 43 },       // code 0：黑色
        { r: 237, g: 237, b: 225 },    // code 1：白色
        { r: 224, g: 212, b: 31 },     // code 2：黃色
        { r: 120, g: 32, b: 32 },      // code 3：紅色
        { r: 31, g: 88, b: 169 },      // code 5：藍色
        { r: 58, g: 110, b: 72 }       // code 6：綠色
    ];
    var displayImageCache = new WeakMap();
    var outputImageCache = new WeakMap();

    function copyColors(colors) {
        return colors.map(function (color) { return Object.assign({}, color); });
    }

    function displayColors() {
        if (!app.device.epaperCalibration) {
            return copyColors(DEFAULT_DISPLAY_COLORS);
        }
        return app.device.epaperCalibration.colors().map(function (color) {
            return { r: color.r, g: color.g, b: color.b };
        });
    }

    function calibrationRevision() {
        return app.device.epaperCalibration ? app.device.epaperCalibration.revision() : 0;
    }

    function colorsEqual(left, right) {
        return Array.isArray(left) && left.length === right.length && left.every(function (color, index) {
            return color.r === right[index].r && color.g === right[index].g && color.b === right[index].b;
        });
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
        var colors = displayColors();
        return color
            ? colorIndex(colors, color.r, color.g, color.b)
            : -1;
    }

    function displayColor(color) {
        var index = outputColorIndex(color);
        var colors = displayColors();
        return Object.assign({}, index === -1 ? color : colors[index]);
    }

    function displayImageData(imageData) {
        if (!imageData || !imageData.data) {
            return imageData;
        }
        var revision = calibrationRevision();
        var cached = displayImageCache.get(imageData);
        if (cached && cached.revision === revision) {
            return cached.imageData;
        }
        var colors = displayColors();
        var data = new Uint8ClampedArray(imageData.data);
        for (var offset = 0; offset < data.length; offset += 4) {
            var index = colorIndex(OUTPUT_COLORS, data[offset], data[offset + 1], data[offset + 2]);
            if (index !== -1) {
                data[offset] = colors[index].r;
                data[offset + 1] = colors[index].g;
                data[offset + 2] = colors[index].b;
            }
        }
        var displayed = new ImageData(data, imageData.width, imageData.height);
        displayImageCache.set(imageData, { revision: revision, imageData: displayed });
        return displayed;
    }

    // Pipeline 以 A′ 運算；只有送入 EPDIMG 前才把每個 palette index 正規化為協定色 A。
    function outputImageData(imageData) {
        if (!imageData || !imageData.data) {
            return imageData;
        }
        var revision = calibrationRevision();
        var cached = outputImageCache.get(imageData);
        if (cached && cached.revision === revision) {
            return cached.imageData;
        }
        var colors = displayColors();
        var data = new Uint8ClampedArray(imageData.data);
        for (var offset = 0; offset < data.length; offset += 4) {
            var index = colorIndex(colors, data[offset], data[offset + 1], data[offset + 2]);
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
        outputImageCache.set(imageData, { revision: revision, imageData: output });
        return output;
    }

    function isEpaper(state) {
        return Boolean(state && state.target && state.target.mode === 'epaper');
    }

    function force(state) {
        if (!state || !state.settings || !app.device.epaper.isSupported()) {
            return false;
        }
        var previousTarget = state.target || {};
        var previousPalette = state.settings.palette && state.settings.palette.palette;
        var crop = state.settings.crop;
        var portrait = crop && crop.aspectRatioId === '3-5';
        if (crop && crop.aspectRatioId !== '5-3' && crop.aspectRatioId !== '3-5') {
            crop.aspectRatioId = '5-3';
            portrait = false;
        }
        var revision = calibrationRevision();
        var colors = displayColors();
        state.target = {
            mode: 'epaper',
            orientation: portrait ? 'portrait' : 'landscape',
            calibrationRevision: revision
        };
        if (state.settings.resize) {
            Object.assign(state.settings.resize, portrait
                ? { width: 480, height: 800, aspectRatio: 3 / 5 }
                : { width: 800, height: 480, aspectRatio: 5 / 3 });
        }
        if (state.settings.palette) {
            state.settings.palette.presetId = PALETTE_ID;
            state.settings.palette.palette = copyColors(colors);
        }
        return previousTarget.mode !== 'epaper'
            || previousTarget.orientation !== state.target.orientation
            || previousTarget.calibrationRevision !== revision
            || !colorsEqual(previousPalette, colors);
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
        colors: displayColors,
        outputColors: OUTPUT_COLORS,
        displayColors: displayColors,
        calibrationRevision: calibrationRevision,
        displayColor: displayColor,
        displayImageData: displayImageData,
        outputImageData: outputImageData,
        outputColorIndex: outputColorIndex,
        displayColorIndex: displayColorIndex
    };
})(window.DitherApp);
