(function (app) {
    // Crop feature 採用「固定比例裁切框 + 移動/縮放/旋轉原圖」的互動模型。
    // 面板只暴露比例、zoom、rotation、flip；實際 x/y/width/height 由比例和來源尺寸推導。
    // 幾何計算在 crop-geometry.js、背景取色在 crop-auto-background.js；
    // 本檔只保留 feature 註冊、panel、state 正規化與正式裁切 operation。
    var ui = app.pages.ditherEditor.panelUtils;
    var geometry = app.pages.ditherEditor.cropGeometry;
    var background = app.pages.ditherEditor.cropBackground;
    var panelRefs = null;

    // 根據目前來源尺寸與設定推導出真正會被使用的 crop 狀態。
    function normalizedCrop(state) {
        var settings = state.settings.crop;
        var bounds = geometry.imageBounds(state);
        var frame = geometry.frameForBounds(bounds, settings.aspectRatioId);
        var zoom = geometry.clamp(settings.zoom || 1, geometry.MIN_ZOOM, geometry.MAX_ZOOM);
        var panLimit = geometry.maxPan(bounds, frame, zoom);

        return {
            // x/y/width/height 保留給舊 settings/pipeline 相容；面板不再直接顯示這些欄位。
            x: Math.round(frame.x),
            y: Math.round(frame.y),
            width: Math.round(frame.width),
            height: Math.round(frame.height),
            panX: Number(geometry.clamp(settings.panX, -panLimit.x, panLimit.x).toFixed(2)),
            panY: Number(geometry.clamp(settings.panY, -panLimit.y, panLimit.y).toFixed(2)),
            aspectRatioId: frame.ratio.id,
            zoom: Number(zoom.toFixed(2)),
            rotation: geometry.clamp(settings.rotation || 0, -180, 180),
            flipX: Boolean(settings.flipX),
            flipY: Boolean(settings.flipY),
            backgroundPreset: background.presetFor(settings.backgroundPreset).id,
            backgroundColor: background.normalizeHexColor(settings.backgroundColor),
            autoBackgroundColor: background.normalizeHexColor(settings.autoBackgroundColor || settings.backgroundColor)
        };
    }

    // 將推導後的 crop 狀態寫回 state.settings.crop。
    function applyNormalizedCrop(state) {
        Object.assign(state.settings.crop, normalizedCrop(state));
    }

    // 建立一份重設用的 crop 設定；size 決定初始裁切框。
    function defaultCropSettings(size) {
        return {
            x: 0,
            y: 0,
            width: size.width,
            height: size.height,
            panX: 0,
            panY: 0,
            aspectRatioId: geometry.DEFAULT_ASPECT_RATIO_ID,
            zoom: 1,
            rotation: 0,
            flipX: false,
            flipY: false,
            backgroundPreset: background.DEFAULT_PRESET,
            backgroundColor: background.DEFAULT_COLOR,
            autoBackgroundColor: background.DEFAULT_COLOR
        };
    }

    // render 後同步面板控制元件，避免 normalize 後 UI 顯示舊值。
    function updatePanel(state) {
        if (!panelRefs) {
            return;
        }
        var crop = state.settings.crop;
        panelRefs.aspectRatio.value = crop.aspectRatioId;
        panelRefs.zoom.setValue(Math.round((crop.zoom || 1) * 100));
        panelRefs.rotation.setValue(crop.rotation);
        panelRefs.backgroundPreset.value = crop.backgroundPreset;
        panelRefs.backgroundColor.value = background.backgroundColor(crop, state.sourceImageData);
        panelRefs.flipX.setAttribute('aria-pressed', crop.flipX ? 'true' : 'false');
        panelRefs.flipY.setAttribute('aria-pressed', crop.flipY ? 'true' : 'false');
    }

    function cropToImageData(imageData, settings) {
        // 正式輸出時，只建立裁切框大小的 target canvas，然後用同一組 transform 把原圖畫進去。
        // 這樣 preview 與 export 共用「原圖移動」的概念，不會變成移動裁切框。
        var frame = geometry.frameForBounds(imageData, settings.aspectRatioId);
        var width = Math.max(1, Math.round(frame.width));
        var height = Math.max(1, Math.round(frame.height));
        var canvas = app.core.canvasUtils.createCanvas(imageData.width, imageData.height);
        var ctx = canvas.getContext('2d');
        var target = app.core.canvasUtils.createCanvas(width, height);
        var targetCtx = target.getContext('2d', { willReadFrequently: true });
        var rotation = Number(settings.rotation || 0);
        var zoom = geometry.clamp(settings.zoom || 1, geometry.MIN_ZOOM, geometry.MAX_ZOOM);
        var backgroundColor = background.backgroundColor(settings, imageData);

        ctx.putImageData(imageData, 0, 0);
        targetCtx.fillStyle = backgroundColor;
        targetCtx.fillRect(0, 0, width, height);
        targetCtx.translate(width / 2 + Number(settings.panX || 0), height / 2 + Number(settings.panY || 0));
        targetCtx.rotate(rotation * Math.PI / 180);
        // signed scale 同時處理 zoom 與 flip，需與 viewport-renderer 的 preview transform 保持一致。
        targetCtx.scale(settings.flipX ? -zoom : zoom, settings.flipY ? -zoom : zoom);
        targetCtx.drawImage(canvas, -imageData.width / 2, -imageData.height / 2);
        return targetCtx.getImageData(0, 0, width, height);
    }

    function cropLabel(text) {
        return app.utils.dom.el('label', { text: text });
    }

    function cropField(label, input) {
        return app.utils.dom.el('div', {
            className: 'crop-field',
            children: [cropLabel(label), input]
        });
    }

    function cropBackgroundControl(select, colorInput) {
        return app.utils.dom.el('div', {
            className: 'crop-background-control',
            children: [select, colorInput]
        });
    }

    function cropIconButton(iconPath, fallbackText, label) {
        return app.utils.dom.el('button', {
            className: 'icon-button crop-icon-button',
            children: [ui.svgIcon(iconPath, { fallbackText: fallbackText })],
            attrs: { type: 'button', 'aria-label': label, title: label }
        });
    }

    // 對外的 crop 小 API：viewport 模組只透過這裡取用幾何與背景色，不重寫 crop 規則。
    app.pages.ditherEditor.crop = {
        ratios: geometry.ratios,
        frameForBounds: geometry.frameForBounds,
        previewLayout: geometry.previewLayout,
        backgroundColor: background.backgroundColor,
        normalize: applyNormalizedCrop
    };

    app.pages.ditherEditor.featureRegistry.register({
        id: 'crop',
        icon: '[]',
        iconPath: 'assets/icons/editor/crop.svg',
        labelKey: 'panelCrop',
        pipelineStage: 'fixedBefore',
        pipelineOrder: 10,
        panelGroup: 'prepare',
        // 跨 feature 查詢介面：resize 等 feature 只能經由這裡取得 crop 輸出尺寸。
        api: {
            // crop 設定的輸出尺寸；尚無有效設定時回 null，呼叫端退回自己的 fallback。
            getOutputSize: function getOutputSize(state) {
                var crop = state && state.settings && state.settings.crop;
                if (crop && crop.width && crop.height) {
                    return { width: crop.width, height: crop.height };
                }
                return null;
            }
        },
        // 建立 crop 預設設定，尺寸依目前 display profile。
        defaultSettings: function defaultSettings(context) {
            var state = {
                workingSize: { width: context.display.width, height: context.display.height },
                sourceImageData: null,
                settings: {
                    crop: defaultCropSettings(context.display)
                }
            };
            applyNormalizedCrop(state);
            return state.settings.crop;
        },
        // 新圖片載入時重設 crop transform，避免上一張圖的 pan/zoom 影響新圖。
        onImageLoaded: function onImageLoaded(context) {
            context.state.settings.crop = defaultCropSettings(context.result.workingSize);
            applyNormalizedCrop(context.state);
        },
        // 任一 crop 設定變更後都重新正規化，確保 pan/zoom 不超界。
        onSettingChanged: function onSettingChanged(context) {
            if (context.id !== 'crop') {
                return;
            }
            applyNormalizedCrop(context.state);
        },
        // 每次渲染後同步 panel refs。
        onRender: function onRender(context) {
            updatePanel(context.state);
        },
        // 建立 crop 控制面板：比例、zoom、rotation、flip。
        buildPanel: function buildPanel(context) {
            var state = context.state;
            var controller = context.controller;
            applyNormalizedCrop(state);
            var crop = state.settings.crop;
            var aspectRatioInput = ui.selectInput(
                crop.aspectRatioId,
                geometry.ratios.map(function (ratio) {
                    return { value: ratio.id, label: ratio.label };
                }),
                function (value) {
                    controller.updateSetting('crop', 'aspectRatioId', value);
                }
            );
            var zoomInput = ui.unitNumberInput(
                Math.round((crop.zoom || 1) * 100),
                geometry.MIN_ZOOM * 100,
                geometry.MAX_ZOOM * 100,
                1,
                '%',
                function (value) {
                    controller.updateSetting('crop', 'zoom', value / 100);
                }
            );
            var rotationInput = ui.unitNumberInput(crop.rotation, -180, 180, 1, 'deg', function (value) {
                controller.updateSetting('crop', 'rotation', value);
            });
            var backgroundColorInput = app.utils.dom.el('input', {
                className: 'crop-background-color',
                attrs: {
                    type: 'color',
                    value: background.backgroundColor(crop, state.sourceImageData),
                    'aria-label': ui.t('labelCropFillColor')
                }
            });
            var backgroundPresetInput = ui.selectInput(
                crop.backgroundPreset,
                background.presets.map(function (preset) {
                    return { value: preset.id, label: ui.t(preset.labelKey) };
                }),
                function (value) {
                    var preset = background.presetFor(value);
                    var current = controller.state.settings.crop;
                    var next = Object.assign({}, current, { backgroundPreset: preset.id });
                    var color = preset.id === background.PRESET_CUSTOM
                        ? background.normalizeHexColor(
                            current.backgroundPreset === background.PRESET_AUTO
                                ? current.autoBackgroundColor
                                : current.backgroundColor
                        )
                        : background.backgroundColor(next, controller.state.sourceImageData);
                    backgroundColorInput.value = color;
                    controller.updateSettings('crop', {
                        backgroundPreset: preset.id,
                        backgroundColor: color,
                        autoBackgroundColor: preset.id === background.PRESET_AUTO
                            ? color
                            : background.normalizeHexColor(current.autoBackgroundColor)
                    });
                }
            );
            backgroundColorInput.addEventListener('input', function () {
                backgroundPresetInput.select.value = background.PRESET_CUSTOM;
                state.settings.crop.backgroundPreset = background.PRESET_CUSTOM;
                state.settings.crop.backgroundColor = background.normalizeHexColor(backgroundColorInput.value);
            });
            backgroundColorInput.addEventListener('change', function () {
                controller.updateSettings('crop', {
                    backgroundPreset: background.PRESET_CUSTOM,
                    backgroundColor: background.normalizeHexColor(backgroundColorInput.value)
                });
            });
            var rotateLeftButton = cropIconButton('assets/icons/editor/crop-rotate-left.svg', '↺', ui.t('actionRotateLeft'));
            var rotateRightButton = cropIconButton('assets/icons/editor/crop-rotate-right.svg', '↻', ui.t('actionRotateRight'));
            var flipXButton = cropIconButton('assets/icons/editor/crop-flip-horizontal.svg', '⇄', ui.t('actionFlipHorizontal'));
            var flipYButton = cropIconButton('assets/icons/editor/crop-flip-vertical.svg', '⇅', ui.t('actionFlipVertical'));
            flipXButton.setAttribute('aria-pressed', crop.flipX ? 'true' : 'false');
            flipYButton.setAttribute('aria-pressed', crop.flipY ? 'true' : 'false');
            rotateLeftButton.addEventListener('click', function () {
                var current = controller.state.settings.crop;
                controller.updateSetting('crop', 'rotation', geometry.steppedRotation(current.rotation, -90));
            });
            rotateRightButton.addEventListener('click', function () {
                var current = controller.state.settings.crop;
                controller.updateSetting('crop', 'rotation', geometry.steppedRotation(current.rotation, 90));
            });
            flipXButton.addEventListener('click', function () {
                var current = controller.state.settings.crop;
                // 已旋轉圖片反轉時同步鏡射 rotation/pan，讓按鈕語意以目前畫面座標為準。
                controller.updateSettings('crop', {
                    flipX: !current.flipX,
                    rotation: geometry.mirroredRotation(current.rotation),
                    panX: -Number(current.panX || 0)
                });
            });
            flipYButton.addEventListener('click', function () {
                var current = controller.state.settings.crop;
                controller.updateSettings('crop', {
                    flipY: !current.flipY,
                    rotation: geometry.mirroredRotation(current.rotation),
                    panY: -Number(current.panY || 0)
                });
            });

            panelRefs = {
                aspectRatio: aspectRatioInput.select,
                zoom: zoomInput,
                rotation: rotationInput,
                backgroundPreset: backgroundPresetInput.select,
                backgroundColor: backgroundColorInput,
                flipX: flipXButton,
                flipY: flipYButton
            };

            return ui.section('panelCrop', [
                app.utils.dom.el('div', {
                    className: 'crop-quadrant-grid',
                    children: [
                        cropField(ui.t('labelRatio'), aspectRatioInput.node),
                        cropField(ui.t('labelZoom'), zoomInput),
                        cropField(ui.t('labelRotate'), rotationInput),
                        cropField(ui.t('labelFill'), cropBackgroundControl(backgroundPresetInput.node, backgroundColorInput)),
                        app.utils.dom.el('div', {
                            className: 'crop-icon-button-row',
                            children: [rotateLeftButton, rotateRightButton, flipXButton, flipYButton]
                        })
                    ]
                })
            ], 'crop');
        },
        operation: {
            // Pipeline 中的 crop operation，輸出固定比例裁切後的 ImageData。
            run: function run(imageData, settings) {
                if (
                    Number(settings.rotation || 0) === 0 &&
                    Number(settings.panX || 0) === 0 &&
                    Number(settings.panY || 0) === 0 &&
                    Number(settings.zoom || 1) === 1 &&
                    !settings.flipX &&
                    !settings.flipY &&
                    settings.width === imageData.width &&
                    settings.height === imageData.height
                ) {
                    return imageData;
                }
                return cropToImageData(imageData, settings);
            }
        }
    });
})(window.DitherApp);
