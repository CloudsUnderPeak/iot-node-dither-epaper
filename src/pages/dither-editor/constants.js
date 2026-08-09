(function (app) {
    // Dither Editor 共用常數集中在這裡，避免 controller、loader、state 各自硬寫數值。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.constants = {
        MAX_INPUT_LONG_EDGE: 800,
        MAX_RESIZE_OUTPUT_SIZE: 4096,
        PREVIEW_DEBOUNCE_MS: 80,
        SHOW_PREVIEW_TIMING_LABEL: true,
        PREVIEW_TIMING_LABEL_HIDE_DELAY_MS: 2000,
        DEFAULT_DITHER_ALGORITHM_ID: 'floyd-steinberg',
        DEFAULT_PALETTE_MAPPING_ID: 'nearest-color',
        DEFAULT_COLOR_DISTANCE_ID: 'euclidean-bt709',
        DEFAULT_DITHER_ERROR_STRENGTH: 100,
        MIN_DITHER_ERROR_STRENGTH: 0,
        MAX_DITHER_ERROR_STRENGTH: 150,
        DITHER_ERROR_STRENGTH_STEP: 2,
        DEFAULT_ORIGINAL_PALETTE_SIZE: 8,
        MIN_ORIGINAL_PALETTE_SIZE: 2,
        MAX_ORIGINAL_PALETTE_SIZE: 32,
        DEFAULT_NEW_IMAGE_SIZE: {
            width: 800,
            height: 480
        }
    };
    if (app.app.projectCapabilities) {
        app.app.projectCapabilities.setFact(
            'maxInputLongEdge',
            app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE
        );
        app.app.projectCapabilities.setFact(
            'maxResizeOutputSize',
            app.pages.ditherEditor.constants.MAX_RESIZE_OUTPUT_SIZE
        );
    }
})(window.DitherApp);
