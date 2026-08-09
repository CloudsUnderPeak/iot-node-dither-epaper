(function (app) {
    // Dither algorithm config 只註冊演算法 metadata；執行邏輯由 processor registry 提供。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    var registry = app.pages.ditherEditor.ditherAlgorithmRegistry;

    registry.register({
        id: 'floyd-steinberg',
        labelKey: 'algorithmFloydSteinberg',
        processorId: 'error-diffusion',
        helpFamily: 'error-diffusion',
        matrixId: 'floydSteinberg',
        supportsSerpentine: true,
        supportsErrorStrength: true
    });
    registry.register({
        id: 'atkinson',
        labelKey: 'algorithmAtkinson',
        processorId: 'error-diffusion',
        helpFamily: 'error-diffusion',
        matrixId: 'atkinson',
        supportsErrorStrength: true
    });
    registry.register({
        id: 'jarvis',
        labelKey: 'algorithmJarvis',
        processorId: 'error-diffusion',
        helpFamily: 'error-diffusion',
        matrixId: 'jarvis',
        supportsSerpentine: true,
        supportsErrorStrength: true
    });
    registry.register({
        id: 'sierra-lite',
        labelKey: 'algorithmSierraLite',
        processorId: 'error-diffusion',
        helpFamily: 'error-diffusion',
        matrixId: 'sierraLite',
        supportsSerpentine: true,
        supportsErrorStrength: true
    });
    registry.register({
        id: 'stevenson-arce',
        labelKey: 'algorithmStevensonArce',
        processorId: 'error-diffusion',
        helpFamily: 'error-diffusion',
        matrixId: 'stevensonArce',
        supportsErrorStrength: true
    });
    registry.register({
        id: 'adaptive-fs-3x3',
        labelKey: 'algorithmAdaptiveFs3',
        processorId: 'adaptive-error-diffusion',
        helpFamily: 'error-diffusion',
        adaptiveRadius: 1,
        supportsSerpentine: true,
        supportsErrorStrength: true
    });
    registry.register({
        id: 'bayer-4',
        labelKey: 'algorithmBayer4',
        processorId: 'ordered',
        helpFamily: 'ordered',
        matrixId: 'bayer4',
        thresholdScale: 70,
        supportsThresholdStrength: true
    });
    registry.register({
        id: 'bayer-8',
        labelKey: 'algorithmBayer8',
        processorId: 'ordered',
        helpFamily: 'ordered',
        matrixId: 'bayer8',
        thresholdScale: 70,
        supportsThresholdStrength: true
    });
    registry.register({
        id: 'blue-noise-64',
        labelKey: 'algorithmBlueNoise64',
        processorId: 'ordered',
        helpFamily: 'ordered',
        matrixId: 'blueNoise64',
        thresholdScale: 42,
        supportsThresholdStrength: true
    });
    registry.register({
        id: 'dot-diffusion-simple',
        labelKey: 'algorithmDotDiffusionSimple',
        processorId: 'dot-diffusion',
        helpFamily: 'dot',
        supportsErrorStrength: true
    });
    registry.register({
        id: 'pattern-dots',
        labelKey: 'algorithmPatternDots',
        processorId: 'pattern',
        helpFamily: 'dot',
        thresholdScale: 86,
        supportsDotDensity: true
    });

    Object.defineProperty(app.pages.ditherEditor.config, 'ditherAlgorithms', {
        configurable: true,
        get: function getDitherAlgorithms() {
            return registry.list();
        }
    });

    if (app.app.projectCapabilities) {
        app.app.projectCapabilities.replaceCollection('dither-algorithms', registry.list());
    }
})(window.DitherApp);
