(function (app) {
    // Palette 共用計算工具。
    // 預設使用未加權 RGB 的 Euclidean distance。
    var DEFAULT_COLOR_DISTANCE_ID = 'euclidean-rgb';
    // 依賴 core/color/color-utils.js 先載入（index.html 與 tools 的 script 順序保證）。
    var clampChannel = app.core.colorUtils.clampChannel;

    // 純量版距離函式是唯一的權重來源；物件版與 mapper 的逐像素熱路徑都委派到這裡。
    // GPU shader（threshold-dither-processor.js 的 distanceTo）無法共用程式碼，
    // 修改權重時必須同步更新該 shader。
    function euclideanRgbDistanceRgb(r1, g1, b1, r2, g2, b2) {
        var dr = r1 - r2;
        var dg = g1 - g2;
        var db = b1 - b2;
        return dr * dr + dg * dg + db * db;
    }

    function manhattanRgbDistanceRgb(r1, g1, b1, r2, g2, b2) {
        return Math.abs(r1 - r2) + Math.abs(g1 - g2) + Math.abs(b1 - b2);
    }

    function euclideanBt709DistanceRgb(r1, g1, b1, r2, g2, b2) {
        var dr = r1 - r2;
        var dg = g1 - g2;
        var db = b1 - b2;
        return 0.2126 * dr * dr + 0.7152 * dg * dg + 0.0722 * db * db;
    }

    function manhattanBt709DistanceRgb(r1, g1, b1, r2, g2, b2) {
        return 0.2126 * Math.abs(r1 - r2)
            + 0.7152 * Math.abs(g1 - g2)
            + 0.0722 * Math.abs(b1 - b2);
    }

    function euclideanRgbDistance(a, b) {
        return euclideanRgbDistanceRgb(a.r, a.g, a.b, b.r, b.g, b.b);
    }

    function manhattanRgbDistance(a, b) {
        return manhattanRgbDistanceRgb(a.r, a.g, a.b, b.r, b.g, b.b);
    }

    function euclideanBt709Distance(a, b) {
        return euclideanBt709DistanceRgb(a.r, a.g, a.b, b.r, b.g, b.b);
    }

    function manhattanBt709Distance(a, b) {
        return manhattanBt709DistanceRgb(a.r, a.g, a.b, b.r, b.g, b.b);
    }

    function srgbToLinear(value) {
        var channel = clampChannel(Number(value) || 0) / 255;
        return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4);
    }

    function labPivot(value) {
        var delta = 6 / 29;
        return value > delta * delta * delta ? Math.cbrt(value) : value / (3 * delta * delta) + 4 / 29;
    }

    function rgbToLab(color) {
        var r = srgbToLinear(color.r);
        var g = srgbToLinear(color.g);
        var b = srgbToLinear(color.b);
        var x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
        var y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
        var z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;
        var fx = labPivot(x / 0.95047);
        var fy = labPivot(y);
        var fz = labPivot(z / 1.08883);
        return {
            l: 116 * fy - 16,
            a: 500 * (fx - fy),
            b: 200 * (fy - fz)
        };
    }

    function degreesToRadians(value) {
        return value * Math.PI / 180;
    }

    function hueAngle(a, b) {
        if (a === 0 && b === 0) {
            return 0;
        }
        var hue = Math.atan2(b, a) * 180 / Math.PI;
        return hue < 0 ? hue + 360 : hue;
    }

    function ciede2000Distance(lab1, lab2) {
        var l1 = lab1.l;
        var a1 = lab1.a;
        var b1 = lab1.b;
        var l2 = lab2.l;
        var a2 = lab2.a;
        var b2 = lab2.b;
        var c1 = Math.sqrt(a1 * a1 + b1 * b1);
        var c2 = Math.sqrt(a2 * a2 + b2 * b2);
        var cBar = (c1 + c2) / 2;
        var cBar7 = Math.pow(cBar, 7);
        var g = 0.5 * (1 - Math.sqrt(cBar7 / (cBar7 + Math.pow(25, 7))));
        var a1Prime = (1 + g) * a1;
        var a2Prime = (1 + g) * a2;
        var c1Prime = Math.sqrt(a1Prime * a1Prime + b1 * b1);
        var c2Prime = Math.sqrt(a2Prime * a2Prime + b2 * b2);
        var h1Prime = hueAngle(a1Prime, b1);
        var h2Prime = hueAngle(a2Prime, b2);
        var deltaLPrime = l2 - l1;
        var deltaCPrime = c2Prime - c1Prime;
        var deltaHPrime = 0;

        if (c1Prime * c2Prime !== 0) {
            deltaHPrime = h2Prime - h1Prime;
            if (Math.abs(deltaHPrime) > 180) {
                deltaHPrime += deltaHPrime > 0 ? -360 : 360;
            }
        }

        var deltaBigHPrime = 2 * Math.sqrt(c1Prime * c2Prime)
            * Math.sin(degreesToRadians(deltaHPrime / 2));
        var lBarPrime = (l1 + l2) / 2;
        var cBarPrime = (c1Prime + c2Prime) / 2;
        var hBarPrime = h1Prime + h2Prime;
        if (c1Prime * c2Prime !== 0) {
            if (Math.abs(h1Prime - h2Prime) <= 180) {
                hBarPrime = (h1Prime + h2Prime) / 2;
            } else if (h1Prime + h2Prime < 360) {
                hBarPrime = (h1Prime + h2Prime + 360) / 2;
            } else {
                hBarPrime = (h1Prime + h2Prime - 360) / 2;
            }
        }
        var t = 1
            - 0.17 * Math.cos(degreesToRadians(hBarPrime - 30))
            + 0.24 * Math.cos(degreesToRadians(2 * hBarPrime))
            + 0.32 * Math.cos(degreesToRadians(3 * hBarPrime + 6))
            - 0.20 * Math.cos(degreesToRadians(4 * hBarPrime - 63));
        var deltaTheta = 30 * Math.exp(-Math.pow((hBarPrime - 275) / 25, 2));
        var cBarPrime7 = Math.pow(cBarPrime, 7);
        var rc = 2 * Math.sqrt(cBarPrime7 / (cBarPrime7 + Math.pow(25, 7)));
        var sl = 1 + (0.015 * Math.pow(lBarPrime - 50, 2))
            / Math.sqrt(20 + Math.pow(lBarPrime - 50, 2));
        var sc = 1 + 0.045 * cBarPrime;
        var sh = 1 + 0.015 * cBarPrime * t;
        var rt = -Math.sin(degreesToRadians(2 * deltaTheta)) * rc;
        var termL = deltaLPrime / sl;
        var termC = deltaCPrime / sc;
        var termH = deltaBigHPrime / sh;
        return Math.max(0, termL * termL + termC * termC + termH * termH + rt * termC * termH);
    }

    function normalizeColorDistanceId(id) {
        // 保留舊 id 相容，並轉成明確標示有無 BT.709 權重的新命名。
        if (id === 'bt709' || id === 'euclidean') {
            return 'euclidean-bt709';
        }
        if (id === 'rgb') {
            return 'euclidean-rgb';
        }
        if (id === 'manhattan') {
            return 'manhattan-rgb';
        }
        var metrics = app.pages
            && app.pages.ditherEditor
            && app.pages.ditherEditor.config
            && app.pages.ditherEditor.config.colorDistanceMetrics;
        var fallback = app.pages
            && app.pages.ditherEditor
            && app.pages.ditherEditor.constants
            && app.pages.ditherEditor.constants.DEFAULT_COLOR_DISTANCE_ID
            || DEFAULT_COLOR_DISTANCE_ID;
        if (metrics && metrics.some(function (metric) { return metric.id === id; })) {
            return id;
        }
        return fallback;
    }

    function createNearestColorFinder(palette, colorDistanceId) {
        var mode = normalizeColorDistanceId(colorDistanceId);
        var candidates = (palette || []).map(function (color) {
            var candidate = { color: color };
            if (mode === 'ciede2000') {
                candidate.lab = rgbToLab(color);
            }
            return candidate;
        });

        return function findNearestColor(color) {
            if (!candidates.length) {
                return color;
            }
            var best = candidates[0].color;
            var bestDistance = Infinity;
            var sourceLab = mode === 'ciede2000' ? rgbToLab(color) : null;
            candidates.forEach(function (candidate) {
                var current;
                if (mode === 'manhattan-rgb') {
                    current = manhattanRgbDistance(color, candidate.color);
                } else if (mode === 'manhattan-bt709') {
                    current = manhattanBt709Distance(color, candidate.color);
                } else if (mode === 'euclidean-bt709') {
                    current = euclideanBt709Distance(color, candidate.color);
                } else if (mode === 'ciede2000') {
                    current = ciede2000Distance(sourceLab, candidate.lab);
                } else {
                    current = euclideanRgbDistance(color, candidate.color);
                }
                if (current < bestDistance) {
                    bestDistance = current;
                    best = candidate.color;
                }
            });
            return best;
        };
    }

    function createColorDistanceMeasurer(colorDistanceId) {
        var mode = normalizeColorDistanceId(colorDistanceId);
        return function measureColorDistance(a, b) {
            if (mode === 'manhattan-rgb') {
                return manhattanRgbDistance(a, b);
            }
            if (mode === 'manhattan-bt709') {
                return manhattanBt709Distance(a, b);
            }
            if (mode === 'euclidean-bt709') {
                return euclideanBt709Distance(a, b);
            }
            if (mode === 'ciede2000') {
                return ciede2000Distance(rgbToLab(a), rgbToLab(b));
            }
            return euclideanRgbDistance(a, b);
        };
    }

    // 建立純量 (r,g,b) 距離 context，逐像素熱路徑用它避免物件配置。
    // ciede2000 等非 RGB 度量退回物件版 measurer，用共用 scratch 物件橋接。
    function createRgbDistanceContext(colorDistanceId) {
        var mode = normalizeColorDistanceId(colorDistanceId);
        if (mode === 'manhattan-rgb') {
            return { rgb: manhattanRgbDistanceRgb };
        }
        if (mode === 'manhattan-bt709') {
            return { rgb: manhattanBt709DistanceRgb };
        }
        if (mode === 'euclidean-rgb') {
            return { rgb: euclideanRgbDistanceRgb };
        }
        if (mode === 'euclidean-bt709') {
            return { rgb: euclideanBt709DistanceRgb };
        }

        var measure = createColorDistanceMeasurer(mode);
        var scratchA = { r: 0, g: 0, b: 0 };
        var scratchB = { r: 0, g: 0, b: 0 };
        return {
            rgb: function rgb(r1, g1, b1, r2, g2, b2) {
                scratchA.r = r1;
                scratchA.g = g1;
                scratchA.b = b1;
                scratchB.r = r2;
                scratchB.g = g2;
                scratchB.b = b2;
                return measure(scratchA, scratchB);
            }
        };
    }

    app.core.paletteUtils = {
        createNearestColorFinder: createNearestColorFinder,
        createColorDistanceMeasurer: createColorDistanceMeasurer,
        createRgbDistanceContext: createRgbDistanceContext,
        normalizeColorDistanceId: normalizeColorDistanceId,
        // 從 palette 中找出和目標色最接近的顏色。
        nearestColor: function nearestColor(color, palette, colorDistanceId) {
            return createNearestColorFinder(palette, colorDistanceId)(color);
        }
    };
})(window.DitherApp);
