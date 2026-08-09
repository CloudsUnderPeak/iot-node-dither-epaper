(function (app) {
    // Crop 背景色模組：background preset 定義與 Auto 取色演算法。
    // Auto 取色以裁切框範圍縮樣後，取「不透明邊界像素」的 trimmed average；
    // 邊界取不到樣本時退回影像四邊，並用穩定距離避免拖曳時顏色跳動。
    // 依賴 cropGeometry.frameForBounds 與 core.canvasUtils（離屏取樣）。
    var geometry = app.pages.ditherEditor.cropGeometry;
    var BACKGROUND_PRESET_AUTO = 'auto';
    var BACKGROUND_PRESET_BLACK = 'black';
    var BACKGROUND_PRESET_WHITE = 'white';
    var BACKGROUND_PRESET_CUSTOM = 'custom';
    var DEFAULT_BACKGROUND_PRESET = BACKGROUND_PRESET_AUTO;
    var DEFAULT_BACKGROUND_COLOR = '#ffffff';
    var AUTO_BACKGROUND_SAMPLE_LONG_EDGE = 240;
    var AUTO_BACKGROUND_ALPHA_THRESHOLD = 24;
    var AUTO_BACKGROUND_STABLE_DISTANCE = 12;
    var AUTO_BACKGROUND_TRIM_RATIO = 0.15;
    var autoBackgroundCache = {
        imageData: null,
        sourceCanvas: null,
        key: '',
        color: DEFAULT_BACKGROUND_COLOR
    };

    var BACKGROUND_PRESETS = [
        { id: BACKGROUND_PRESET_AUTO, labelKey: 'backgroundAuto', color: DEFAULT_BACKGROUND_COLOR },
        { id: BACKGROUND_PRESET_BLACK, labelKey: 'backgroundBlack', color: '#000000' },
        { id: BACKGROUND_PRESET_WHITE, labelKey: 'backgroundWhite', color: '#ffffff' },
        { id: BACKGROUND_PRESET_CUSTOM, labelKey: 'backgroundCustom', color: DEFAULT_BACKGROUND_COLOR }
    ];

    function backgroundPresetFor(id) {
        return BACKGROUND_PRESETS.find(function (preset) {
            return preset.id === id;
        }) || BACKGROUND_PRESETS.find(function (preset) {
            return preset.id === DEFAULT_BACKGROUND_PRESET;
        });
    }

    function normalizeHexColor(value) {
        var text = String(value || '').trim();
        if (/^#[0-9a-f]{6}$/i.test(text)) {
            return text.toLowerCase();
        }
        return DEFAULT_BACKGROUND_COLOR;
    }

    function colorToHex(r, g, b) {
        return '#' + [r, g, b].map(function (value) {
            return Math.max(0, Math.min(255, Math.round(value))).toString(16).padStart(2, '0');
        }).join('');
    }

    function hexToColor(hex) {
        var normalized = normalizeHexColor(hex).replace('#', '');
        return {
            r: parseInt(normalized.slice(0, 2), 16),
            g: parseInt(normalized.slice(2, 4), 16),
            b: parseInt(normalized.slice(4, 6), 16)
        };
    }

    function colorDistance(a, b) {
        var red = a.r - b.r;
        var green = a.g - b.g;
        var blue = a.b - b.b;
        return Math.sqrt(red * red + green * green + blue * blue);
    }

    function stabilizeAutoBackgroundColor(rawColor, settings) {
        var previousColor = normalizeHexColor(settings && settings.autoBackgroundColor);
        var from = hexToColor(previousColor);
        var to = hexToColor(rawColor);
        var distance = colorDistance(from, to);
        if (distance <= AUTO_BACKGROUND_STABLE_DISTANCE) {
            return previousColor;
        }
        return rawColor;
    }

    function trimmedAverage(values) {
        if (!values.length) {
            return 255;
        }
        values.sort(function (a, b) {
            return a - b;
        });
        var trim = Math.floor(values.length * AUTO_BACKGROUND_TRIM_RATIO);
        var start = Math.min(trim, values.length - 1);
        var end = Math.max(start + 1, values.length - trim);
        var sum = 0;
        for (var i = start; i < end; i += 1) {
            sum += values[i];
        }
        return sum / (end - start);
    }

    function samplesToColor(samples, fallback) {
        if (!samples.length) {
            return fallback;
        }
        var red = [];
        var green = [];
        var blue = [];
        samples.forEach(function (sample) {
            red.push(sample.r);
            green.push(sample.g);
            blue.push(sample.b);
        });
        return colorToHex(
            trimmedAverage(red),
            trimmedAverage(green),
            trimmedAverage(blue)
        );
    }

    function isAutoOpaque(data, offset) {
        return data[offset + 3] > AUTO_BACKGROUND_ALPHA_THRESHOLD;
    }

    function addAutoSample(samples, data, offset) {
        if (!isAutoOpaque(data, offset)) {
            return;
        }
        samples.push({
            r: data[offset],
            g: data[offset + 1],
            b: data[offset + 2]
        });
    }

    function autoBackgroundKey(imageData, settings, width, height) {
        return [
            imageData.width,
            imageData.height,
            width,
            height,
            settings.aspectRatioId,
            Number(settings.panX || 0).toFixed(2),
            Number(settings.panY || 0).toFixed(2),
            Number(settings.zoom || 1).toFixed(2),
            Number(settings.rotation || 0).toFixed(2),
            settings.flipX ? 1 : 0,
            settings.flipY ? 1 : 0
        ].join('|');
    }

    function sourceCanvasForAuto(imageData) {
        if (autoBackgroundCache.imageData === imageData && autoBackgroundCache.sourceCanvas) {
            return autoBackgroundCache.sourceCanvas;
        }
        var canvas = app.core.canvasUtils.createCanvas(imageData.width, imageData.height);
        canvas.getContext('2d').putImageData(imageData, 0, 0);
        autoBackgroundCache.imageData = imageData;
        autoBackgroundCache.sourceCanvas = canvas;
        autoBackgroundCache.key = '';
        return canvas;
    }

    function collectAutoBoundarySamples(data, width, height) {
        var samples = [];
        for (var y = 1; y < height - 1; y += 1) {
            for (var x = 1; x < width - 1; x += 1) {
                var offset = (y * width + x) * 4;
                if (!isAutoOpaque(data, offset)) {
                    continue;
                }
                var top = offset - width * 4;
                var right = offset + 4;
                var bottom = offset + width * 4;
                var left = offset - 4;
                if (
                    isAutoOpaque(data, top) &&
                    isAutoOpaque(data, right) &&
                    isAutoOpaque(data, bottom) &&
                    isAutoOpaque(data, left)
                ) {
                    continue;
                }
                addAutoSample(samples, data, offset);
            }
        }
        return samples;
    }

    function collectAutoFallbackSamples(data, width, height) {
        var samples = [];
        function addPixel(x, y) {
            var offset = (y * width + x) * 4;
            addAutoSample(samples, data, offset);
        }
        for (var x = 0; x < width; x += 1) {
            addPixel(x, 0);
            addPixel(x, height - 1);
        }
        for (var y = 1; y < height - 1; y += 1) {
            addPixel(0, y);
            addPixel(width - 1, y);
        }
        return samples;
    }

    function autoBackgroundColor(imageData, settings) {
        if (!imageData) {
            return normalizeHexColor(settings && (settings.autoBackgroundColor || settings.backgroundColor));
        }
        var frame = geometry.frameForBounds(imageData, settings.aspectRatioId);
        var width = Math.max(1, Math.round(frame.width));
        var height = Math.max(1, Math.round(frame.height));
        var key = autoBackgroundKey(imageData, settings, width, height);
        if (autoBackgroundCache.imageData === imageData && autoBackgroundCache.key === key) {
            return autoBackgroundCache.color;
        }

        var scale = Math.min(1, AUTO_BACKGROUND_SAMPLE_LONG_EDGE / Math.max(width, height));
        var sampleWidth = Math.max(1, Math.round(width * scale));
        var sampleHeight = Math.max(1, Math.round(height * scale));
        var sampleCanvas = app.core.canvasUtils.createCanvas(sampleWidth, sampleHeight);
        var sampleCtx = sampleCanvas.getContext('2d', { willReadFrequently: true });
        var sourceCanvas = sourceCanvasForAuto(imageData);
        sampleCtx.clearRect(0, 0, sampleWidth, sampleHeight);
        sampleCtx.save();
        sampleCtx.translate(
            sampleWidth / 2 + Number(settings.panX || 0) * scale,
            sampleHeight / 2 + Number(settings.panY || 0) * scale
        );
        sampleCtx.rotate(Number(settings.rotation || 0) * Math.PI / 180);
        sampleCtx.scale(
            (settings.flipX ? -1 : 1) * Number(settings.zoom || 1) * scale,
            (settings.flipY ? -1 : 1) * Number(settings.zoom || 1) * scale
        );
        sampleCtx.drawImage(sourceCanvas, -imageData.width / 2, -imageData.height / 2);
        sampleCtx.restore();

        var sampleData = sampleCtx.getImageData(0, 0, sampleWidth, sampleHeight).data;
        var fallback = normalizeHexColor(settings && (settings.autoBackgroundColor || settings.backgroundColor));
        var rawColor = samplesToColor(
            collectAutoBoundarySamples(sampleData, sampleWidth, sampleHeight),
            null
        ) || samplesToColor(
            collectAutoFallbackSamples(sampleData, sampleWidth, sampleHeight),
            fallback
        );
        var color = stabilizeAutoBackgroundColor(rawColor, settings);
        autoBackgroundCache.key = key;
        autoBackgroundCache.color = color;
        if (settings) {
            settings.autoBackgroundColor = color;
        }
        return color;
    }

    // 依 preset 決定 crop 背景色：Auto 走取色演算法、Custom 用使用者色、其餘用 preset 固定色。
    function cropBackgroundColor(settings, imageData) {
        var preset = backgroundPresetFor(settings && settings.backgroundPreset);
        if (preset.id === BACKGROUND_PRESET_AUTO) {
            return autoBackgroundColor(imageData, settings);
        }
        if (preset.id !== BACKGROUND_PRESET_CUSTOM) {
            return preset.color;
        }
        return normalizeHexColor(settings && settings.backgroundColor);
    }

    app.pages.ditherEditor.cropBackground = {
        PRESET_AUTO: BACKGROUND_PRESET_AUTO,
        PRESET_BLACK: BACKGROUND_PRESET_BLACK,
        PRESET_WHITE: BACKGROUND_PRESET_WHITE,
        PRESET_CUSTOM: BACKGROUND_PRESET_CUSTOM,
        DEFAULT_PRESET: DEFAULT_BACKGROUND_PRESET,
        DEFAULT_COLOR: DEFAULT_BACKGROUND_COLOR,
        presets: BACKGROUND_PRESETS.slice(),
        presetFor: backgroundPresetFor,
        normalizeHexColor: normalizeHexColor,
        backgroundColor: cropBackgroundColor
    };
})(window.DitherApp);
