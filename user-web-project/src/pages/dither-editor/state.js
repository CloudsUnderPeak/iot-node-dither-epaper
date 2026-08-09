(function (app) {
    // Editor state 由 enabled features 建立，避免 state.js 手寫每個 tool 的 settings。
    // 這是 feature plug-and-play 的關鍵：停用 feature 時，state/pipeline 也跟著消失。
    // 建立 Dither Editor 初始 state；所有 feature settings 都由 registry 動態產生。
    function defaultState() {
        var config = app.pages.ditherEditor.config;
        var preset = config.pipelinePresets[0];
        var display = config.displayProfiles[0];
        var palette = config.palettePresets[0];
        var featureRegistry = app.pages.ditherEditor.featureRegistry;
        var pipeline = {
            fixedBefore: featureRegistry.pipelineIds(preset, 'fixedBefore'),
            effectsOrder: featureRegistry.pipelineIds(preset, 'effectsOrder'),
            fixedAfter: featureRegistry.pipelineIds(preset, 'fixedAfter')
        };
        var settings = {};
        var context = { display: display, palette: palette };
        var presetEnabled = preset.enabled || {};
        var initialTool = null;
        var openToolPanels = {};

        featureRegistry.all().forEach(function (tool) {
            if (tool.defaultSettings) {
                settings[tool.id] = tool.defaultSettings(context);
            }
            if (!initialTool && tool.dock !== false) {
                // 第一個可見 dock tool 預設展開；目前通常是 Image Input。
                initialTool = tool.id;
            }
        });
        if (initialTool) {
            openToolPanels[initialTool] = true;
        }
        var enabled = {};
        pipeline.fixedBefore.concat(pipeline.effectsOrder).forEach(function (id) {
            // preset.enabled 只覆蓋 false；未列出的 operation 預設啟用。
            enabled[id] = presetEnabled[id] !== false;
        });
        pipeline.enabled = enabled;

        return {
            schemaVersion: 1,
            status: 'empty',
            mode: app.pages.ditherEditor.editorModeStateMachine.groups.SOURCE,
            fileName: 'Untitled',
            sourceImageData: null,
            preparedImageData: null,
            previewImageData: null,
            previewRenderDurationMs: null,
            previewTimingLabel: {
                phase: 'hidden',
                durationMs: null
            },
            outputImageData: null,
            livePreview: null,
            openToolPanels: openToolPanels,
            viewMode: 'result',
            originalSize: { width: 0, height: 0 },
            workingSize: { width: display.width, height: display.height },
            viewport: { zoom: 1, panX: 0, panY: 0 },
            pipeline: pipeline,
            settings: settings
        };
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.state = {
        create: defaultState
    };
})(window.DitherApp);
