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
        var width = imageData.width;
        var height = imageData.height;
        var data = new Float32Array(imageData.data);
        var output = new Uint8ClampedArray(imageData.data.length);
        var factor7 = (7 / 16) * strength;
        var factor3 = (3 / 16) * strength;
        var factor5 = (5 / 16) * strength;
        var factor1 = (1 / 16) * strength;
        var rowStride = width * 4;

        for (var y = 0; y < height; y += 1) {
            var reverse = serpentine && y % 2 === 1;
            var rowOffset = y * rowStride;

            if (!reverse) {
                for (var x = 0; x < width; x += 1) {
                    var index = rowOffset + x * 4;
                    var r = data[index];
                    var g = data[index + 1];
                    var b = data[index + 2];
                    var mapped = mappedColor(paletteMapper, r, g, b);
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

                    if (x + 1 < width) {
                        diffuse(data, index + 4, er, eg, eb, factor7);
                    }
                    if (y + 1 < height) {
                        var nextRow = index + rowStride;
                        if (x > 0) {
                            diffuse(data, nextRow - 4, er, eg, eb, factor3);
                        }
                        diffuse(data, nextRow, er, eg, eb, factor5);
                        if (x + 1 < width) {
                            diffuse(data, nextRow + 4, er, eg, eb, factor1);
                        }
                    }
                }
            } else {
                for (var rx = width - 1; rx >= 0; rx -= 1) {
                    var reverseIndex = rowOffset + rx * 4;
                    var rr = data[reverseIndex];
                    var rg = data[reverseIndex + 1];
                    var rb = data[reverseIndex + 2];
                    var reverseMapped = mappedColor(paletteMapper, rr, rg, rb);
                    var rnr = reverseMapped.r;
                    var rng = reverseMapped.g;
                    var rnb = reverseMapped.b;
                    var rer = rr - rnr;
                    var reg = rg - rng;
                    var reb = rb - rnb;

                    output[reverseIndex] = rnr;
                    output[reverseIndex + 1] = rng;
                    output[reverseIndex + 2] = rnb;
                    output[reverseIndex + 3] = 255;

                    if (rx > 0) {
                        diffuse(data, reverseIndex - 4, rer, reg, reb, factor7);
                    }
                    if (y + 1 < height) {
                        var reverseNextRow = reverseIndex + rowStride;
                        if (rx + 1 < width) {
                            diffuse(data, reverseNextRow + 4, rer, reg, reb, factor3);
                        }
                        diffuse(data, reverseNextRow, rer, reg, reb, factor5);
                        if (rx > 0) {
                            diffuse(data, reverseNextRow - 4, rer, reg, reb, factor1);
                        }
                    }
                }
            }
        }

        return new ImageData(output, width, height);
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
        var width = imageData.width;
        var height = imageData.height;
        var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
        if (!paletteMapper.length) {
            return imageData;
        }

        var source = imageData.data;
        var meanMap = computeMeanMap(source, width, height, radius);
        var data = new Float32Array(source);
        var output = new Uint8ClampedArray(source.length);
        var strength = normalizeErrorStrength(options.errorStrength);
        var factor7 = (7 / 16) * strength;
        var factor3 = (3 / 16) * strength;
        var factor5 = (5 / 16) * strength;
        var factor1 = (1 / 16) * strength;
        var rowStride = width * 4;
        var adaptiveBiasScale = 0.35;

        for (var y = 0; y < height; y += 1) {
            var reverse = options.serpentine && y % 2 === 1;
            var rowOffset = y * rowStride;
            var start = reverse ? width - 1 : 0;
            var end = reverse ? -1 : width;
            var step = reverse ? -1 : 1;

            for (var x = start; x !== end; x += step) {
                var index = rowOffset + x * 4;
                var r = data[index];
                var g = data[index + 1];
                var b = data[index + 2];
                var localMean = meanMap[y * width + x];
                var bias = (128 - localMean) * adaptiveBiasScale;
                var mapped = mappedColor(
                    paletteMapper,
                    clampChannel(r + bias),
                    clampChannel(g + bias),
                    clampChannel(b + bias)
                );
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

                if (!reverse) {
                    if (x + 1 < width) {
                        diffuse(data, index + 4, er, eg, eb, factor7);
                    }
                    if (y + 1 < height) {
                        var nextRow = index + rowStride;
                        if (x > 0) {
                            diffuse(data, nextRow - 4, er, eg, eb, factor3);
                        }
                        diffuse(data, nextRow, er, eg, eb, factor5);
                        if (x + 1 < width) {
                            diffuse(data, nextRow + 4, er, eg, eb, factor1);
                        }
                    }
                } else {
                    if (x > 0) {
                        diffuse(data, index - 4, er, eg, eb, factor7);
                    }
                    if (y + 1 < height) {
                        var reverseNextRow = index + rowStride;
                        if (x + 1 < width) {
                            diffuse(data, reverseNextRow + 4, er, eg, eb, factor3);
                        }
                        diffuse(data, reverseNextRow, er, eg, eb, factor5);
                        if (x > 0) {
                            diffuse(data, reverseNextRow - 4, er, eg, eb, factor1);
                        }
                    }
                }
            }
        }

        return new ImageData(output, width, height);
    }

    function applyMatrix(imageData, options, matrix, paletteMapper, strength) {
        var width = imageData.width;
        var height = imageData.height;
        var data = new Float32Array(imageData.data);
        var output = new Uint8ClampedArray(imageData.data.length);
        var matrixOffsets = matrixOffsetCache(matrix, width, strength);
        var matrixLength = matrixOffsets.length;
        var offsetX = matrixOffsets.offsetX;
        var offsetY = matrixOffsets.offsetY;
        var forwardOffset = matrixOffsets.forwardOffset;
        var reverseOffset = matrixOffsets.reverseOffset;
        var factors = matrixOffsets.factors;

        for (var y = 0; y < height; y += 1) {
            var reverse = options.serpentine && y % 2 === 1;
            var start = reverse ? width - 1 : 0;
            var end = reverse ? -1 : width;
            var step = reverse ? -1 : 1;

            for (var x = start; x !== end; x += step) {
                var index = (y * width + x) * 4;
                var r = data[index];
                var g = data[index + 1];
                var b = data[index + 2];
                var mapped = mappedColor(paletteMapper, r, g, b);
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

                for (var entryIndex = 0; entryIndex < matrixLength; entryIndex += 1) {
                    var nx = x + (reverse ? -offsetX[entryIndex] : offsetX[entryIndex]);
                    var ny = y + offsetY[entryIndex];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                        continue;
                    }
                    diffuse(
                        data,
                        index + (reverse ? reverseOffset[entryIndex] : forwardOffset[entryIndex]),
                        er,
                        eg,
                        eb,
                        factors[entryIndex]
                    );
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
