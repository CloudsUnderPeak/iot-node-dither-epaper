(function (app) {
    // Adapter around the vendored RgbQuant library.
    // Feature code should use this layer instead of depending on RgbQuant tuple/kernel details.
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    var RGBQUANT_OPTIONS = {
        method: 2,
        boxSize: [8, 8],
        boxPxls: 2,
        initColors: 4096,
        hueGroups: 10,
        minHueCols: 2000,
        initDist: 0.02,
        distIncr: 0.005,
        dithDelta: 0,
        reIndex: false,
        useCache: true,
        cacheFreq: 10
    };

    function createRgbQuant(options) {
        if (!window.RgbQuant) {
            throw new Error('RgbQuant is not loaded.');
        }
        return new window.RgbQuant(Object.assign({}, RGBQUANT_OPTIONS, options || {}));
    }

    function tupleToColor(tuple) {
        return {
            r: tuple[0],
            g: tuple[1],
            b: tuple[2]
        };
    }

    function normalizePaletteSize(value) {
        var constants = app.pages.ditherEditor.constants;
        var size = Number(value);
        if (!Number.isFinite(size)) {
            return constants.DEFAULT_ORIGINAL_PALETTE_SIZE;
        }
        return Math.max(
            constants.MIN_ORIGINAL_PALETTE_SIZE,
            Math.min(constants.MAX_ORIGINAL_PALETTE_SIZE, Math.round(size))
        );
    }

    app.pages.ditherEditor.rgbQuantAdapter = {
        extractPalette: function extractPalette(imageData, paletteSize) {
            if (!imageData || !imageData.data || !imageData.width || !imageData.height) {
                return [];
            }
            var size = normalizePaletteSize(paletteSize);
            var quantizer = createRgbQuant({
                colors: size,
                palette: [],
                colorDist: 'euclidean'
            });
            quantizer.sample(imageData);
            return quantizer.palette(true).slice(0, size).map(tupleToColor);
        }
    };
})(window.DitherApp);
