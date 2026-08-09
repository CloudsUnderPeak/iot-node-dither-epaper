(function (app) {
    var WIDTH = 800;
    var HEIGHT = 480;
    var FRAME_BYTES = WIDTH * HEIGHT / 2;
    var HEADER_BYTES = 40;
    var COLOR_CODES = {
        '0,0,0': 0,
        '255,255,255': 1,
        '255,0,0': 3,
        '255,255,0': 2,
        '0,0,255': 5,
        '0,255,0': 6
    };

    function crc32(bytes) {
        var crc = 0xffffffff;
        for (var i = 0; i < bytes.length; i += 1) {
            crc ^= bytes[i];
            for (var bit = 0; bit < 8; bit += 1) {
                crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
            }
        }
        return (crc ^ 0xffffffff) >>> 0;
    }

    function codeAt(imageData, x, y) {
        var offset = (y * imageData.width + x) * 4;
        if (imageData.data[offset + 3] !== 255) {
            throw new Error('E-paper output contains transparent pixels.');
        }
        var key = imageData.data[offset] + ',' + imageData.data[offset + 1] + ',' + imageData.data[offset + 2];
        if (COLOR_CODES[key] === undefined) {
            throw new Error('E-paper output contains a color outside the fixed six-color palette.');
        }
        return COLOR_CODES[key];
    }

    function encode(imageData) {
        var portrait = imageData && imageData.width === 480 && imageData.height === 800;
        if (!imageData || (!portrait && !(imageData.width === WIDTH && imageData.height === HEIGHT))) {
            throw new Error('E-paper output must be 800x480 or 480x800.');
        }
        var frame = new Uint8Array(FRAME_BYTES);
        for (var y = 0; y < HEIGHT; y += 1) {
            for (var x = 0; x < WIDTH; x += 2) {
                var first = portrait ? codeAt(imageData, y, 799 - x) : codeAt(imageData, x, y);
                var second = portrait ? codeAt(imageData, y, 798 - x) : codeAt(imageData, x + 1, y);
                frame[(y * WIDTH + x) / 2] = (first << 4) | second;
            }
        }
        var payload = new Uint8Array(HEADER_BYTES + FRAME_BYTES);
        payload.set([69, 80, 68, 73, 77, 71, 0, 0], 0); // EPDIMG\0\0
        var view = new DataView(payload.buffer);
        view.setUint32(8, 1, true);
        view.setUint32(12, HEADER_BYTES, true);
        view.setUint32(16, WIDTH, true);
        view.setUint32(20, HEIGHT, true);
        view.setUint32(24, FRAME_BYTES, true);
        var checksum = crc32(frame);
        view.setUint32(28, checksum, true);
        var generation = new Uint32Array(2);
        window.crypto.getRandomValues(generation);
        if (generation[0] === 0 && generation[1] === 0) {
            generation[0] = 1;
        }
        view.setUint32(32, generation[0], true);
        view.setUint32(36, generation[1], true);
        payload.set(frame, HEADER_BYTES);
        return { payload: payload, crc32: checksum, rotated: portrait };
    }

    app.core.epdimgEncoder = { encode: encode, crc32: crc32 };
})(window.DitherApp);
