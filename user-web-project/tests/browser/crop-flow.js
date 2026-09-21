(async function () {
    var passed = [];
    var originalApp = window.DitherApp;
    try {
        function load(path) { harness.execute(path.replace(/^src[/]/, ''), {window: window}); }
        testEntryPaths.slice(0, testEntryPaths.indexOf('src/pages/dither-editor/entry.js')).forEach(function (path) {
            if (path !== 'src/device/device-mock.js') { load(path); }
        });
        var app = window.DitherApp;
        app.app.scriptLoader.loadMany = function (paths) {
            paths.forEach(load);
            if (paths.indexOf('src/pages/dither-editor/feature-manifest.js') !== -1) {
                var manifest = app.pages.ditherEditor.featureManifest;
                if (window.testCropMode === 'crop_removed') {
                    app.pages.ditherEditor.featureManifest = manifest.filter(function (entry) { return entry.id !== 'crop'; });
                } else {
                    manifest.find(function (entry) { return entry.id === 'crop'; }).enabled = false;
                }
            }
            return Promise.resolve();
        };
        load('src/pages/dither-editor/entry.js');
        await app.app.whenPageEntriesReady();
        var editor = app.pages.ditherEditor;
        var state = editor.state.create();
        harness.assert(!editor.featureRegistry.api('crop') && !state.settings.crop &&
            state.pipeline.fixedBefore.indexOf('crop') === -1, 'disabled crop has no API, settings or operation');
        var source = new ImageData(2, 2);
        source.data.set([255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255]);
        var overlay = new editor.ViewportOverlayRenderer({prepareMode: 'prepare'});
        harness.assert(!overlay.shouldShowCropOverlay({sourceImageData: source, settings: state.settings, mode: 'prepare'}),
            'missing crop never enters crop renderer');
        var controller = new editor.Controller({render: function () {}});
        app.core.imageLoader.loadDemoImage = function () { return Promise.resolve({imageData: source, sourceFile: null,
            fileName: 'sample.png', originalSize: {width: 2, height: 2}, workingSize: {width: 2, height: 2}}); };
        await controller.loadDemo();
        harness.assert(controller.state.sourceImageData === source && controller.state.mode !== 'prepare',
            'source load skips missing crop preparation');
        controller.state.settings.dither.algorithm = 'none';
        var output = await editor.pipelineRunner.runAsync(source, controller.state);
        harness.assert(output && output.data.length > 0, 'other effects remain runnable');
        var blob = await app.core.canvasUtils.imageDataToBlob(output);
        harness.assert(blob.type === 'image/png' && blob.size > 0, 'PNG export remains available');
        var target = app.core.epaperTarget.geometry(800, 480);
        app.device.epaper.isSupported = function () { return true; };
        app.device.epaper.snapshot = function () { return {target: target}; };
        editor.targetPolicy.sync(controller.state);
        harness.assert(controller.state.settings.resize.width === 800 &&
            controller.state.settings.palette.presetId === editor.targetPolicy.paletteId &&
            !editor.targetPolicy.settingAllowed(controller.state, 'resize', 'width', 2),
            'device size and palette guards survive without crop');
        controller.destroy();
        editor.featureRegistry.register({id: 'custom-action', dock: false, panelGroup: 'none',
            pipelineStage: 'fixedAfter', buildAction: function () {}});
        editor.featureRegistry.register({id: 'custom-operation', dock: false, panelGroup: 'none',
            pipelineStage: 'fixedBefore', operation: {run: function (image) { return image; }}});
        harness.assert(editor.featureRegistry.pipelineIds(null, 'fixedAfter').indexOf('custom-action') !== -1 &&
            editor.featureRegistry.pipelineIds(null, 'fixedBefore').indexOf('custom-operation') !== -1,
            'arbitrary action and operation IDs satisfy capability contract');
        var rejected = false;
        try { editor.featureRegistry.register({id: 'invalid-operation', dock: false, panelGroup: 'none',
            pipelineStage: 'effectsOrder'}); } catch (error) { rejected = true; }
        harness.assert(rejected, 'pipeline feature without action or run is rejected');
        passed.push('Crop ' + window.testCropMode + ': load, preview, effects, PNG and device guards');
        document.getElementById('result').textContent = JSON.stringify({passed: passed});
    } catch (error) {
        document.getElementById('result').textContent = JSON.stringify({passed: passed, error: error.stack});
    } finally {
        window.DitherApp = originalApp;
    }
})();
