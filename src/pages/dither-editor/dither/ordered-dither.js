(function (app) {
    // Ordered dithering 使用固定 Bayer matrix 產生規律網點，速度穩定且結果可預期。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    function matrixSize(matrix) {
        return matrix.size || matrix.length;
    }

    function matrixLevels(matrix, size) {
        return matrix.levels || size * size;
    }

    function matrixCell(matrix, x, y, size) {
        if (typeof matrix.cell === 'function') {
            return matrix.cell(x, y);
        }
        return matrix[y % size][x % size];
    }

    function thresholdMap(matrix, size, levels) {
        if (matrix.thresholdMap) {
            return matrix.thresholdMap;
        }
        var map = new Float32Array(size * size);
        for (var y = 0; y < size; y += 1) {
            for (var x = 0; x < size; x += 1) {
                map[y * size + x] = (matrixCell(matrix, x, y, size) + 0.5) / levels;
            }
        }
        matrix.thresholdMap = map;
        return map;
    }

    function wrapIndex(value, size, wrapMask) {
        return wrapMask === null ? value % size : value & wrapMask;
    }

    function applyCpu(imageData, options, thresholds, size, wrapMask, thresholdScale) {
        var width = imageData.width;
        var height = imageData.height;
        var source = imageData.data;
        var output = new Uint8ClampedArray(source.length);
        var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
        if (!paletteMapper.length) {
            return imageData;
        }

        for (var y = 0; y < height; y += 1) {
            var rowOffset = y * width * 4;
            var thresholdRow = wrapIndex(y, size, wrapMask) * size;
            for (var x = 0; x < width; x += 1) {
                var index = rowOffset + x * 4;
                var threshold = thresholds[thresholdRow + wrapIndex(x, size, wrapMask)];
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

    app.pages.ditherEditor.orderedDither = {
        // 執行 ordered dithering，options.matrixId 決定 threshold matrix。
        apply: function apply(imageData, options, algorithm) {
            var matrices = app.pages.ditherEditor.ditherMatrices;
            var matrix = matrices[options.matrixId] || matrices.bayer4;
            var size = matrixSize(matrix);
            var levels = matrixLevels(matrix, size);
            var thresholds = thresholdMap(matrix, size, levels);
            var wrapMask = (size & (size - 1)) === 0 ? size - 1 : null;
            var thresholdScale = Number.isFinite(options.thresholdScale)
                ? options.thresholdScale
                : algorithm && algorithm.thresholdScale || 70;
            var thresholdStrength = Number.isFinite(options.thresholdStrength) ? options.thresholdStrength : 1;
            var gpuProcessor = app.pages.ditherEditor.thresholdDitherProcessor;
            if (gpuProcessor) {
                return gpuProcessor.apply(imageData, options, {
                    cacheKey: 'ordered:' + (options.matrixId || 'bayer4') + ':' + thresholdScale + ':' + thresholdStrength,
                    matrixSize: size,
                    levels: levels,
                    thresholds: thresholds,
                    thresholdScale: thresholdScale,
                    thresholdStrength: thresholdStrength
                }, function fallback() {
                    return applyCpu(imageData, options, thresholds, size, wrapMask, thresholdScale);
                });
            }
            return applyCpu(imageData, options, thresholds, size, wrapMask, thresholdScale);
        }
    };

    app.pages.ditherEditor.ditherAlgorithmRegistry.registerProcessor({
        id: 'ordered',
        apply: function apply(imageData, options, algorithm) {
            return app.pages.ditherEditor.orderedDither.apply(imageData, options, algorithm);
        }
    });
})(window.DitherApp);
