(function (app) {
    // Clustered-dot halftone 使用中心向外成長的 ordered matrix 產生規律網點。
    // 它是風格化網點，不是 dot diffusion。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    function buildClusteredDotMatrix(size) {
        var center = (size - 1) / 2;
        var cells = [];
        var matrix = new Array(size);
        for (var y = 0; y < size; y += 1) {
            matrix[y] = new Array(size);
            for (var x = 0; x < size; x += 1) {
                var dx = x - center;
                var dy = y - center;
                cells.push({
                    x: x,
                    y: y,
                    distance: dx * dx + dy * dy,
                    angle: Math.atan2(dy, dx)
                });
            }
        }
        cells.sort(function (a, b) {
            if (a.distance !== b.distance) {
                return a.distance - b.distance;
            }
            return a.angle - b.angle;
        });
        for (var i = 0; i < cells.length; i += 1) {
            matrix[cells[i].y][cells[i].x] = i;
        }
        return matrix;
    }

    var CLUSTERED_DOT_MATRIX = buildClusteredDotMatrix(8);
    var CLUSTERED_DOT_THRESHOLDS = (function buildThresholds() {
        var matrixSize = CLUSTERED_DOT_MATRIX.length;
        var levels = matrixSize * matrixSize;
        var thresholds = new Float32Array(levels);
        for (var y = 0; y < matrixSize; y += 1) {
            for (var x = 0; x < matrixSize; x += 1) {
                thresholds[y * matrixSize + x] = (
                    CLUSTERED_DOT_MATRIX[y][x] + 0.5
                ) / levels;
            }
        }
        return thresholds;
    })();
    var CLUSTERED_DOT_WRAP_MASK = (CLUSTERED_DOT_MATRIX.length & (CLUSTERED_DOT_MATRIX.length - 1)) === 0
        ? CLUSTERED_DOT_MATRIX.length - 1
        : null;

    function wrapIndex(value, size) {
        return CLUSTERED_DOT_WRAP_MASK === null ? value % size : value & CLUSTERED_DOT_WRAP_MASK;
    }

    function normalizeDotDensity(value) {
        var dotDensity = Number(value);
        if (!Number.isFinite(dotDensity)) {
            return 1;
        }
        return Math.max(0, Math.min(1.5, dotDensity));
    }

    function dotDensityScale(dotDensity) {
        return Math.pow(2, (dotDensity - 1) * 2);
    }

    function thresholdIndex(position, densityScale, matrixSize) {
        return wrapIndex(Math.floor(position * densityScale), matrixSize);
    }

    function applyCpu(imageData, options, thresholdScale, densityScale) {
        var width = imageData.width;
        var height = imageData.height;
        var source = imageData.data;
        var output = new Uint8ClampedArray(source.length);
        var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
        var matrixSize = CLUSTERED_DOT_MATRIX.length;
        if (!paletteMapper.length) {
            return imageData;
        }

        for (var y = 0; y < height; y += 1) {
            var rowOffset = y * width * 4;
            var thresholdRow = thresholdIndex(y, densityScale, matrixSize) * matrixSize;
            for (var x = 0; x < width; x += 1) {
                var index = rowOffset + x * 4;
                var threshold = CLUSTERED_DOT_THRESHOLDS[
                    thresholdRow + thresholdIndex(x, densityScale, matrixSize)
                ];
                var nearest = paletteMapper.mapThresholdColor(
                    source[index],
                    source[index + 1],
                    source[index + 2],
                    threshold,
                    thresholdScale
                );
                output[index] = nearest.r;
                output[index + 1] = nearest.g;
                output[index + 2] = nearest.b;
                output[index + 3] = 255;
            }
        }

        return new ImageData(output, width, height);
    }

    app.pages.ditherEditor.patternDither = {
        // 執行 clustered-dot halftone，輸出仍會映射到 palette。
        apply: function apply(imageData, options, algorithm) {
            var thresholdScale = algorithm && algorithm.thresholdScale || 86;
            var dotDensity = normalizeDotDensity(options.dotDensity);
            var densityScale = dotDensityScale(dotDensity);
            var gpuProcessor = app.pages.ditherEditor.thresholdDitherProcessor;
            if (gpuProcessor) {
                return gpuProcessor.apply(imageData, options, {
                    cacheKey: 'pattern:clustered-dot:' + thresholdScale + ':' + dotDensity,
                    matrixSize: CLUSTERED_DOT_MATRIX.length,
                    levels: CLUSTERED_DOT_MATRIX.length * CLUSTERED_DOT_MATRIX.length,
                    thresholds: CLUSTERED_DOT_THRESHOLDS,
                    thresholdScale: thresholdScale,
                    thresholdStrength: 1,
                    thresholdCellScale: densityScale
                }, function fallback() {
                    return applyCpu(imageData, options, thresholdScale, densityScale);
                });
            }
            return applyCpu(imageData, options, thresholdScale, densityScale);
        }
    };

    app.pages.ditherEditor.ditherAlgorithmRegistry.registerProcessor({
        id: 'pattern',
        apply: function apply(imageData, options, algorithm) {
            return app.pages.ditherEditor.patternDither.apply(imageData, options, algorithm);
        }
    });
})(window.DitherApp);
