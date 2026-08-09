(function (app) {
    // Class-matrix dot diffusion：先依 class matrix 排處理順序，再把 RGB 誤差擴散給尚未處理的鄰居。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    var CLASS_MATRIX = [
        [34, 48, 40, 32, 29, 15, 23, 31],
        [42, 58, 56, 53, 21, 5, 7, 10],
        [50, 62, 61, 45, 13, 1, 2, 18],
        [38, 46, 54, 37, 25, 17, 9, 26],
        [28, 14, 22, 30, 35, 49, 41, 33],
        [20, 4, 6, 11, 43, 59, 57, 52],
        [12, 0, 3, 19, 51, 63, 60, 44],
        [24, 16, 8, 27, 39, 47, 55, 36]
    ];
    var CLASS_COORDS = new Array(64);

    for (var classY = 0; classY < 8; classY += 1) {
        for (var classX = 0; classX < 8; classX += 1) {
            CLASS_COORDS[CLASS_MATRIX[classY][classX]] = { x: classX, y: classY };
        }
    }
    var RECIPIENT_OFFSETS = buildRecipientOffsets();

    // clamp 保留小數：工作緩衝是 Float32Array，過早 round 會改變誤差累積。
    var clampChannel = app.core.colorUtils.clampChannel;

    function normalizeErrorStrength(value) {
        var strength = Number(value);
        if (!Number.isFinite(strength)) {
            return 1;
        }
        return Math.max(0, Math.min(150, strength)) / 100;
    }

    function classAt(x, y) {
        return CLASS_MATRIX[y & 7][x & 7];
    }

    function buildRecipientOffsets() {
        var output = new Array(CLASS_COORDS.length);
        for (var currentClass = 0; currentClass < CLASS_COORDS.length; currentClass += 1) {
            var coord = CLASS_COORDS[currentClass];
            var offsetXs = [];
            var offsetYs = [];
            for (var offsetY = -1; offsetY <= 1; offsetY += 1) {
                for (var offsetX = -1; offsetX <= 1; offsetX += 1) {
                    if (offsetX === 0 && offsetY === 0) {
                        continue;
                    }
                    if (classAt(coord.x + offsetX, coord.y + offsetY) > currentClass) {
                        offsetXs.push(offsetX);
                        offsetYs.push(offsetY);
                    }
                }
            }
            output[currentClass] = {
                offsetX: new Int8Array(offsetXs),
                offsetY: new Int8Array(offsetYs),
                length: offsetXs.length
            };
        }
        return output;
    }

    function countRecipients(x, y, width, height, offsets) {
        var count = 0;
        for (var i = 0; i < offsets.length; i += 1) {
            var nx = x + offsets.offsetX[i];
            var ny = y + offsets.offsetY[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                count += 1;
            }
        }
        return count;
    }

    function diffuseRecipients(
        source,
        index,
        x,
        y,
        width,
        height,
        rowStride,
        offsets,
        errorR,
        errorG,
        errorB,
        count
    ) {
        var shareR = errorR / count;
        var shareG = errorG / count;
        var shareB = errorB / count;
        for (var i = 0; i < offsets.length; i += 1) {
            var offsetX = offsets.offsetX[i];
            var offsetY = offsets.offsetY[i];
            var nx = x + offsetX;
            var ny = y + offsetY;
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
            }
            var recipientIndex = index + offsetY * rowStride + offsetX * 4;
            source[recipientIndex] = clampChannel(source[recipientIndex] + shareR);
            source[recipientIndex + 1] = clampChannel(source[recipientIndex + 1] + shareG);
            source[recipientIndex + 2] = clampChannel(source[recipientIndex + 2] + shareB);
        }
    }

    app.pages.ditherEditor.dotDiffusion = {
        apply: function apply(imageData, options) {
            var width = imageData.width;
            var height = imageData.height;
            var source = new Float32Array(imageData.data);
            var output = new Uint8ClampedArray(source.length);
            var paletteMapper = app.pages.ditherEditor.paletteMapping.createMapper(options);
            var rowStride = width * 4;
            var errorStrength = normalizeErrorStrength(options.errorStrength);
            if (!paletteMapper.length) {
                return imageData;
            }

            for (var currentClass = 0; currentClass < CLASS_COORDS.length; currentClass += 1) {
                var coord = CLASS_COORDS[currentClass];
                var offsets = RECIPIENT_OFFSETS[currentClass];
                for (var y = coord.y; y < height; y += 8) {
                    for (var x = coord.x; x < width; x += 8) {
                        var index = (y * width + x) * 4;
                        var oldR = source[index];
                        var oldG = source[index + 1];
                        var oldB = source[index + 2];
                        var nearest = paletteMapper.mapColor(oldR, oldG, oldB);
                        var errorR = (oldR - nearest.r) * errorStrength;
                        var errorG = (oldG - nearest.g) * errorStrength;
                        var errorB = (oldB - nearest.b) * errorStrength;
                        var count = countRecipients(x, y, width, height, offsets);

                        output[index] = nearest.r;
                        output[index + 1] = nearest.g;
                        output[index + 2] = nearest.b;
                        output[index + 3] = 255;

                        if (!count) {
                            continue;
                        }
                        diffuseRecipients(
                            source,
                            index,
                            x,
                            y,
                            width,
                            height,
                            rowStride,
                            offsets,
                            errorR,
                            errorG,
                            errorB,
                            count
                        );
                    }
                }
            }

            return new ImageData(output, width, height);
        }
    };

    app.pages.ditherEditor.ditherAlgorithmRegistry.registerProcessor({
        id: 'dot-diffusion',
        apply: function apply(imageData, options) {
            return app.pages.ditherEditor.dotDiffusion.apply(imageData, options);
        }
    });
})(window.DitherApp);
