(async function () {
    var app = window.DitherApp;
    await app.app.whenPageEntriesReady();
    var editor = app.pages.ditherEditor;
    function assert(value, message) { if (!value) { throw new Error(message); } }
    var tooltip = document.createElement('span'); tooltip.className = 'control-tip-icon'; tooltip.dataset.tooltip = 'fixture';
    var helpLink = document.createElement('a'); helpLink.className = 'help-nav-link'; helpLink.style.setProperty('--help-nav-depth', '2');
    document.body.appendChild(tooltip); document.body.appendChild(helpLink);
    assert(getComputedStyle(tooltip, '::after').bottom === '22px', 'production tooltip calc');
    assert(getComputedStyle(helpLink).paddingLeft === '38px', 'production Help var/calc');
    var demo = await app.core.imageLoader.loadDemoImage(800);
    assert(demo.imageData.width > 0 && demo.imageData.height > 0, 'HTTP demo decode');
    var input = new ImageData(800, 480);
    for (var i = 0; i < input.data.length; i += 4) {
        input.data[i] = i % 251; input.data[i + 1] = i % 197; input.data[i + 2] = i % 137; input.data[i + 3] = 255;
    }
    var algorithm = editor.ditherAlgorithmRegistry.get('floyd-steinberg');
    var options = { palette: [{ r: 0, g: 0, b: 0 }, { r: 255, g: 255, b: 255 }],
        paletteMapping: 'nearest-color', colorDistance: 'euclidean-rgb', errorStrength: 100, matrixId: algorithm.matrixId };
    var client = editor.ditherWorkerClient.create();
    var pending = client.run(input, algorithm, options);
    assert(pending, 'HTTP worker must be available');
    var output = await pending;
    var cpu = editor.ditherAlgorithmRegistry.run(input, algorithm, options);
    assert(output.data.every(function (value, index) { return value === cpu.data[index]; }), 'HTTP worker CPU pixels');
    assert(input.data.byteLength === 800 * 480 * 4 && client.pendingCount() === 0, 'transfer ownership');
    client.terminate();
    var png = await app.core.canvasUtils.imageDataToBlob(output);
    assert(png.type === 'image/png' && png.size > 0, 'local PNG computation');
    app.device.api.setToken('contract-fixture');
    var session = await app.device.auth.ensureSession();
    assert(!session && !app.device.api.hasToken(), 'relative session 401 contract');
    var report = { passed: true, width: 800, height: 480, pngBytes: png.size, workerCpuEqual: true };
    document.getElementById('probe-result').textContent = JSON.stringify(report);
})().catch(function (error) {
    var report = { error: error.stack || error.message };
    document.getElementById('probe-result').textContent = JSON.stringify(report);
});
