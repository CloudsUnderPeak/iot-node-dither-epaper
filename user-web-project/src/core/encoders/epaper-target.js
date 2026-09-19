(function (app) {
    // Browser allocation limit, shared with editor resize and input limits.
    var MAX_DIMENSION = 4096;
    function gcd(a, b) {
        while (b) { var next = a % b; a = b; b = next; }
        return a;
    }
    function geometry(width, height) {
        if (!Number.isSafeInteger(width) || !Number.isSafeInteger(height)
            || width <= 0 || height <= 0 || width % 2 !== 0
            || width > MAX_DIMENSION || height > MAX_DIMENSION) {
            throw new Error('Unsupported e-paper geometry');
        }
        var divisor = gcd(width, height);
        return Object.freeze({
            width: width, height: height, headerBytes: 40,
            frameBytes: width * height / 2, imageBytes: 40 + width * height / 2,
            ratioWidth: width / divisor, ratioHeight: height / divisor,
            landscapeRatioId: width / divisor + '-' + height / divisor,
            portraitRatioId: height / divisor + '-' + width / divisor
        });
    }
    function fromCapabilities(data) {
        var panel = data && data.panel;
        var image = data && data.image;
        var actions = data && data.capabilities;
        if (!panel || typeof panel.model !== 'string' || !panel.model.trim()
            || panel.colors !== 6 || !Array.isArray(panel.color_codes)
            || panel.color_codes.length !== 6
            || panel.color_codes.some(function (code) { return !Number.isInteger(code); })
            || panel.color_codes.slice().sort().join(',') !== '0,1,2,3,5,6'
            || !image || image.format !== 'epdimg' || image.stored_encoding !== 'gzip'
            || !Array.isArray(image.upload_encodings) || image.upload_encodings.indexOf('gzip') === -1
            || !actions || actions.upload !== true || actions.refresh !== true) {
            throw new Error('Unsupported e-paper capability');
        }
        var target = geometry(panel.width, panel.height);
        if (image.header_bytes !== target.headerBytes || image.frame_bytes !== target.frameBytes
            || image.upload_bytes !== target.imageBytes || image.upload_uncompressed_bytes !== target.imageBytes
            || !Number.isSafeInteger(image.max_compressed_bytes) || image.max_compressed_bytes <= 0
            || image.max_compressed_bytes > target.imageBytes * 2 + 2048) {
            throw new Error('Inconsistent e-paper capability');
        }
        return Object.freeze(Object.assign({}, target, {
            model: panel.model, maxCompressedBytes: image.max_compressed_bytes
        }));
    }
    app.core.epaperTarget = { geometry: geometry, fromCapabilities: fromCapabilities, MAX_DIMENSION: MAX_DIMENSION };
})(window.DitherApp);
