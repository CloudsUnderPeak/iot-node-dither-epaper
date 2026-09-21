(function (app) {
    // Error diffusion dithering 會把每個像素量化後的誤差擴散到鄰近未處理像素。
    // 這類演算法較慢，但能產生比單純 threshold 更自然的階調。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    // clamp 保留小數：工作緩衝是 Float32Array，過早 round 會改變誤差累積。
    var clampChannel = app.core.colorUtils.clampChannel;

    function normalizeErrorStrength(value) {
        var percent = Number(value);
        if (!Number.isFinite(percent)) {
            return 1;
        }
        return Math.max(0, Math.min(150, percent)) / 100;
    }

    function mappedColor(paletteMapper, r, g, b) {
        return paletteMapper.mapColor(r, g, b);
    }

    function applyFloydSteinberg(imageData, paletteMapper, serpentine, strength) {
        return applyMatrix(imageData, { serpentine: serpentine },
            app.pages.ditherEditor.ditherMatrices.floydSteinberg, paletteMapper, strength);
    }

    function diffuse(data, index, er, eg, eb, factor) {
        data[index] = clampChannel(data[index] + er * factor);
        data[index + 1] = clampChannel(data[index + 1] + eg * factor);
        data[index + 2] = clampChannel(data[index + 2] + eb * factor);
    }

    function luminanceAt(data, index) {
        return 0.2126 * data[index] + 0.7152 * data[index + 1] + 0.0722 * data[index + 2];
    }

    function computeMeanMap(data, width, height, radius) {
        var stride = width + 1;
        var integral = new Float32Array(stride * (height + 1));

        for (var y = 0; y < height; y += 1) {
            var rowSum = 0;
            for (var x = 0; x < width; x += 1) {
                var pixel = y * width + x;
                rowSum += luminanceAt(data, pixel * 4);
                integral[(y + 1) * stride + (x + 1)] = integral[y * stride + (x + 1)] + rowSum;
            }
        }

        var output = new Float32Array(width * height);
        for (var row = 0; row < height; row += 1) {
            var top = Math.max(0, row - radius);
            var bottom = Math.min(height - 1, row + radius);
            for (var col = 0; col < width; col += 1) {
                var left = Math.max(0, col - radius);
                var right = Math.min(width - 1, col + radius);
                var a = integral[top * stride + left];
                var b = integral[top * stride + (right + 1)];
                var c = integral[(bottom + 1) * stride + left];
                var d = integral[(bottom + 1) * stride + (right + 1)];
                var area = (right - left + 1) * (bottom - top + 1);
                output[row * width + col] = (d - b - c + a) / area;
            }
        }
        return output;
    }

    function matrixOffsetCache(matrix, width, strength) {
        var matrixLength = matrix.length;
        var rowStride = width * 4;
        var cache = matrix._ditherOffsetCache;
        if (cache && cache.width === width && cache.strength === strength) {
            return cache;
        }

        var offsetX = new Int16Array(matrixLength);
        var offsetY = new Int16Array(matrixLength);
        var forwardOffset = new Int32Array(matrixLength);
        var reverseOffset = new Int32Array(matrixLength);
        var factors = new Array(matrixLength);
        for (var i = 0; i < matrixLength; i += 1) {
            offsetX[i] = matrix[i].x;
            offsetY[i] = matrix[i].y;
            forwardOffset[i] = matrix[i].y * rowStride + matrix[i].x * 4;
            reverseOffset[i] = matrix[i].y * rowStride - matrix[i].x * 4;
            factors[i] = matrix[i].factor * strength;
        }
        cache = {
            width: width,
            strength: strength,
            length: matrixLength,
            offsetX: offsetX,
            offsetY: offsetY,
            forwardOffset: forwardOffset,
            reverseOffset: reverseOffset,
            factors: factors
        };
        matrix._ditherOffsetCache = cache;
        return cache;
    }

    function applyAdaptiveFloydSteinberg(imageData, options, radius) {
        var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
        if (!paletteMapper.length) {
            return imageData;
        }
        var meanMap = computeMeanMap(imageData.data, imageData.width, imageData.height, radius);
        return applyMatrix(imageData, options, app.pages.ditherEditor.ditherMatrices.floydSteinberg,
            paletteMapper, normalizeErrorStrength(options.errorStrength), meanMap);
    }

    function applyMatrix(imageData, options, matrix, paletteMapper, strength, meanMap) {
        var width = imageData.width;
        var height = imageData.height;
        var source = imageData.data;
        var output = new Uint8ClampedArray(source.length);
        var matrixOffsets = matrixOffsetCache(matrix, width, strength);
        var offsetX = matrixOffsets.offsetX;
        var offsetY = matrixOffsets.offsetY;
        var factors = matrixOffsets.factors;
        var rowStride = width * 4;
        var rowCount = 1;
        for (var offset = 0; offset < matrixOffsets.length; offset += 1) {
            rowCount = Math.max(rowCount, offsetY[offset] + 1);
        }
        var rows = new Array(rowCount);
        var loaded = new Int32Array(rowCount);
        loaded.fill(-1);
        function rowFor(y) {
            var slot = y % rowCount;
            if (loaded[slot] !== y) {
                if (!rows[slot]) { rows[slot] = new Float32Array(rowStride); }
                rows[slot].set(source.subarray(y * rowStride, (y + 1) * rowStride));
                loaded[slot] = y;
            }
            return rows[slot];
        }

        for (var y = 0; y < height; y += 1) {
            var reverse = options.serpentine && y % 2 === 1;
            var start = reverse ? width - 1 : 0;
            var end = reverse ? -1 : width;
            var step = reverse ? -1 : 1;
            var row = rowFor(y);

            for (var x = start; x !== end; x += step) {
                var localIndex = x * 4;
                var index = y * rowStride + localIndex;
                var r = row[localIndex];
                var g = row[localIndex + 1];
                var b = row[localIndex + 2];
                var mapped;
                if (meanMap) {
                    var bias = (128 - meanMap[y * width + x]) * 0.35;
                    mapped = mappedColor(paletteMapper,
                        clampChannel(r + bias), clampChannel(g + bias), clampChannel(b + bias));
                } else {
                    mapped = mappedColor(paletteMapper, r, g, b);
                }
                var nr = mapped.r;
                var ng = mapped.g;
                var nb = mapped.b;
                var er = r - nr;
                var eg = g - ng;
                var eb = b - nb;

                output[index] = nr;
                output[index + 1] = ng;
                output[index + 2] = nb;
                output[index + 3] = 255;

                for (var entryIndex = 0; entryIndex < matrixOffsets.length; entryIndex += 1) {
                    var nx = x + (reverse ? -offsetX[entryIndex] : offsetX[entryIndex]);
                    var ny = y + offsetY[entryIndex];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) { continue; }
                    diffuse(rowFor(ny), nx * 4, er, eg, eb, factors[entryIndex]);
                }
            }
        }
        return new ImageData(output, width, height);
    }

    app.pages.ditherEditor.errorDiffusion = {
        // 執行 error diffusion，options.matrixId 決定誤差擴散權重。
        apply: function apply(imageData, options) {
            var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
            if (!paletteMapper.length) {
                return imageData;
            }
            var matrices = app.pages.ditherEditor.ditherMatrices;
            var matrix = matrices[options.matrixId] || matrices.floydSteinberg;
            var errorStrength = normalizeErrorStrength(options.errorStrength);
            if (matrix === matrices.floydSteinberg) {
                return applyFloydSteinberg(
                    imageData,
                    paletteMapper,
                    Boolean(options.serpentine),
                    errorStrength
                );
            }

            return applyMatrix(imageData, options, matrix, paletteMapper, errorStrength);
        }
    };

    app.pages.ditherEditor.ditherAlgorithmRegistry.registerProcessor({
        id: 'error-diffusion',
        apply: function apply(imageData, options) {
            return app.pages.ditherEditor.errorDiffusion.apply(imageData, options);
        }
    });

    app.pages.ditherEditor.ditherAlgorithmRegistry.registerProcessor({
        id: 'adaptive-error-diffusion',
        apply: function apply(imageData, options, algorithm) {
            return applyAdaptiveFloydSteinberg(imageData, options, algorithm.adaptiveRadius || 1);
        }
    });
})(window.DitherApp);
