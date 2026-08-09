(function (app) {
    // Adjust feature 負責亮度、對比、飽和度。
    // 拖曳滑桿時優先用 CSS filter 做即時視覺回饋，正式 ImageData 在互動結束後再計算。
    var ui = app.pages.ditherEditor.panelUtils;

    // Adjust 的預設值全部為 0，代表不改變圖片。
    function createDefaultSettings() {
        return { brightness: 0, contrast: 0, saturation: 0 };
    }

    function runCpu(imageData, settings) {
        // WebGL 不可用時的 CPU fallback。公式刻意貼近 CSS filter，避免即時預覽和正式結果差太多。
        var brightness = Number(settings.brightness || 0);
        var contrast = Number(settings.contrast || 0);
        var saturation = Number(settings.saturation || 0);
        if (brightness === 0 && contrast === 0 && saturation === 0) {
            return imageData;
        }
        var brightnessFactor = cssFactor(brightness);
        var contrastFactor = cssFactor(contrast);
        var saturationFactor = cssFactor(saturation);
        var data = new Uint8ClampedArray(imageData.data);

        for (var i = 0; i < data.length; i += 4) {
            var r = data[i] * brightnessFactor;
            var g = data[i + 1] * brightnessFactor;
            var b = data[i + 2] * brightnessFactor;
            r = (r - 128) * contrastFactor + 128;
            g = (g - 128) * contrastFactor + 128;
            b = (b - 128) * contrastFactor + 128;
            var gray = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            r = gray + (r - gray) * saturationFactor;
            g = gray + (g - gray) * saturationFactor;
            b = gray + (b - gray) * saturationFactor;
            data[i] = app.core.colorUtils.clampByte(r);
            data[i + 1] = app.core.colorUtils.clampByte(g);
            data[i + 2] = app.core.colorUtils.clampByte(b);
            data[i + 3] = 255;
        }

        return new ImageData(data, imageData.width, imageData.height);
    }

    // 將目前 adjust 設定轉成 CSS filter 字串，供拖曳中即時預覽使用。
    function livePreviewFilter(context) {
        var current = context.state.settings.adjust;
        if (!context.state.livePreview || !context.state.livePreview.baseImageData) {
            return '';
        }
        if (!canUseLivePreview(context.state, current)) {
            return '';
        }
        return [
            'brightness(' + cssFactor(Number(current.brightness || 0)) + ')',
            'contrast(' + cssFactor(Number(current.contrast || 0)) + ')',
            'saturate(' + cssFactor(Number(current.saturation || 0)) + ')'
        ].join(' ');
    }

    function createLivePreviewBase(context) {
        // 即時預覽基底會先跑完 adjust 之前的 pipeline，並把 adjust 歸零。
        // 之後滑桿拖曳只改 canvas filter，減少大圖逐像素重算的頻率。
        var state = context.state;
        if (!state.sourceImageData || !canUseLivePreview(state, state.settings.adjust)) {
            return null;
        }
        return app.pages.ditherEditor.pipelineRunner.run(state.sourceImageData, {
            pipeline: state.pipeline,
            settings: Object.assign({}, state.settings, { adjust: createDefaultSettings() })
        }, { stageCache: context.stageCache });
    }

    function canUseLivePreview(state, current) {
        // 只有沒有其他啟用中的效果時才用 CSS filter；否則正式 pipeline 結果可能和預覽不一致。
        if (state.pipeline.enabled.adjust === false) {
            return false;
        }
        return !hasActiveEffectOutsideAdjust(state);
    }

    // 檢查 adjust 以外是否有其他會改變像素的效果。
    function hasActiveEffectOutsideAdjust(state) {
        var effects = state.pipeline.effectsOrder || [];
        for (var i = 0; i < effects.length; i += 1) {
            var id = effects[i];
            if (id === 'adjust') {
                continue;
            }
            if (state.pipeline.enabled[id] === false) {
                continue;
            }
            if (isEffectActive(id, state)) {
                return true;
            }
        }
        return false;
    }

    // 透過 registry api 詢問各 effect 是否會改變像素；
    // 未提供 isActive 的 effect 一律視為 active（保守：不啟用 CSS live preview 捷徑）。
    function isEffectActive(id, state) {
        var featureApi = app.pages.ditherEditor.featureRegistry.api(id);
        if (featureApi && typeof featureApi.isActive === 'function') {
            return featureApi.isActive(state);
        }
        return true;
    }

    // 將 -100 到 100 的 UI 數值轉成 CSS/WebGL 使用的倍率。
    function cssFactor(value) {
        return Math.max(0.01, 1 + value / 100);
    }

    // 亮度/對比/飽和度共用的滑桿：統一走 panelUtils.labeledRange 樣板。
    function valueRangeInput(controller, key, value, interaction) {
        return ui.labeledRange({
            value: value,
            min: -100,
            max: 100,
            step: 1,
            className: 'adjust-range-control',
            valueClassName: 'adjust-range-value',
            interaction: interaction,
            onChange: function (nextValue) {
                controller.updateSetting('adjust', key, nextValue);
            }
        });
    }

    app.pages.ditherEditor.featureRegistry.register({
        id: 'adjust',
        icon: '~~',
        iconPath: 'assets/icons/editor/adjust.svg',
        labelKey: 'panelAdjust',
        panelGroup: 'edit',
        pipelineStage: 'effectsOrder',
        pipelineOrder: 10,
        defaultSettings: createDefaultSettings,
        createLivePreviewBase: createLivePreviewBase,
        livePreviewFilter: livePreviewFilter,
        // 建立亮度、對比、飽和度滑桿。
        buildPanel: function buildPanel(context) {
            var state = context.state;
            var controller = context.controller;
            var previewHold = ui.previewHoldHandlers(controller, 'adjust');
            var brightnessInput = valueRangeInput(controller, 'brightness', state.settings.adjust.brightness, previewHold);
            var contrastInput = valueRangeInput(controller, 'contrast', state.settings.adjust.contrast, previewHold);
            var saturationInput = valueRangeInput(controller, 'saturation', state.settings.adjust.saturation, previewHold);
            return ui.section('panelAdjust', [
                ui.row(ui.t('labelBrightness'), brightnessInput),
                ui.row(ui.t('labelContrast'), contrastInput),
                ui.row(ui.t('labelSaturation'), saturationInput)
            ], 'adjust');
        },
        operation: {
            pipeline: {
                draggable: false
            },
            // 正式 pipeline 執行 adjust；優先用 WebGL，失敗時 fallback 到 CPU。
            run: function run(imageData, settings) {
                var brightness = Number(settings.brightness || 0);
                var contrast = Number(settings.contrast || 0);
                var saturation = Number(settings.saturation || 0);
                if (brightness === 0 && contrast === 0 && saturation === 0) {
                    return imageData;
                }
                return app.pages.ditherEditor.adjustProcessor.apply(imageData, settings, function () {
                    return runCpu(imageData, settings);
                });
            }
        }
    });
})(window.DitherApp);
