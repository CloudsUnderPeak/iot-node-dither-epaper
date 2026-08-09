(function (app) {
    // Resize feature 是固定前置處理，會在 effects pipeline 前調整工作尺寸。
    // Resize 永遠鎖定等比；調整 width/height 任一邊時另一邊會依比例同步。
    var ui = app.pages.ditherEditor.panelUtils;
    var MIN_RESIZE_SIZE = 1;
    var constants = app.pages.ditherEditor.constants || {};
    var MAX_RESIZE_SIZE = constants.MAX_RESIZE_OUTPUT_SIZE || 4096;
    var panelRefs = null;

    function clampSize(value, maxSize) {
        maxSize = maxSize || MAX_RESIZE_SIZE;
        return Math.max(MIN_RESIZE_SIZE, Math.min(maxSize, Math.round(Number(value) || MIN_RESIZE_SIZE)));
    }

    function normalizedRatio(value) {
        value = Number(value);
        return Number.isFinite(value) && value > 0 ? value : 1;
    }

    function widthLimit(ratio) {
        ratio = normalizedRatio(ratio);
        return Math.max(MIN_RESIZE_SIZE, Math.min(MAX_RESIZE_SIZE, Math.floor(MAX_RESIZE_SIZE * ratio)));
    }

    function heightLimit(ratio) {
        ratio = normalizedRatio(ratio);
        return Math.max(MIN_RESIZE_SIZE, Math.min(MAX_RESIZE_SIZE, Math.floor(MAX_RESIZE_SIZE / ratio)));
    }

    function ratioFromSize(width, height) {
        width = clampSize(width);
        height = clampSize(height);
        return width / height;
    }

    function cropOutputSize(state, fallback) {
        // crop 輸出尺寸只能經 registry api 查詢；crop feature 停用時退回 fallback。
        var cropApi = app.pages.ditherEditor.featureRegistry.api('crop');
        var size = cropApi ? cropApi.getOutputSize(state) : null;
        if (size) {
            return {
                width: clampSize(size.width),
                height: clampSize(size.height)
            };
        }
        return {
            width: clampSize(fallback.width),
            height: clampSize(fallback.height)
        };
    }

    function resizeFromWidth(settings, width) {
        var ratio = normalizedRatio(settings.aspectRatio || ratioFromSize(settings.width, settings.height));
        var nextWidth = clampSize(width, widthLimit(ratio));
        return {
            width: nextWidth,
            height: clampSize(nextWidth / ratio),
            aspectRatio: ratio
        };
    }

    function resizeFromHeight(settings, height) {
        var ratio = normalizedRatio(settings.aspectRatio || ratioFromSize(settings.width, settings.height));
        var nextHeight = clampSize(height, heightLimit(ratio));
        return {
            width: clampSize(nextHeight * ratio),
            height: nextHeight,
            aspectRatio: ratio
        };
    }

    function sizeField(label, input) {
        return app.utils.dom.el('div', {
            className: 'resize-size-field',
            children: [
                app.utils.dom.el('label', { text: label }),
                input
            ]
        });
    }

    function linkedSizeIcon() {
        return app.utils.dom.el('span', {
            className: 'resize-size-link',
            children: [
                ui.svgIcon('assets/icons/editor/resize-link.svg', { fallbackText: '↔' })
            ],
            attrs: {
                role: 'img',
                title: ui.t('resizeLinkedTitle'),
                'aria-label': ui.t('resizeLinkedTitle')
            }
        });
    }

    function updatePanel(state) {
        if (!panelRefs || !state.settings || !state.settings.resize) {
            return;
        }
        var resize = state.settings.resize;
        resize.aspectRatio = normalizedRatio(resize.aspectRatio || ratioFromSize(resize.width, resize.height));
        var maxWidth = widthLimit(resize.aspectRatio);
        var maxHeight = heightLimit(resize.aspectRatio);
        panelRefs.width.setRange(MIN_RESIZE_SIZE, maxWidth);
        panelRefs.height.setRange(MIN_RESIZE_SIZE, maxHeight);
        panelRefs.width.setValue(resize.width);
        panelRefs.height.setValue(resize.height);
    }

    app.pages.ditherEditor.featureRegistry.register({
        id: 'resize',
        icon: '<>',
        iconPath: 'assets/icons/editor/resize.svg',
        labelKey: 'panelResize',
        panelGroup: 'edit',
        pipelineStage: 'fixedBefore',
        pipelineOrder: 20,
        // 預設輸出尺寸跟 display profile 一致。
        defaultSettings: function defaultSettings(context) {
            return {
                width: context.display.width,
                height: context.display.height,
                aspectRatio: ratioFromSize(context.display.width, context.display.height)
            };
        },
        // 新圖片載入後，resize 預設跟 crop 輸出尺寸同步。
        onImageLoaded: function onImageLoaded(context) {
            var size = cropOutputSize(context.state, context.result.workingSize);
            context.state.settings.resize.width = size.width;
            context.state.settings.resize.height = size.height;
            context.state.settings.resize.aspectRatio = ratioFromSize(size.width, size.height);
        },
        onPrepareCommitted: function onPrepareCommitted(context) {
            if (!context.state.settings.resize) {
                return;
            }
            var size = cropOutputSize(context.state, context.state.workingSize);
            var ratio = ratioFromSize(size.width, size.height);
            var next = resizeFromWidth(
                Object.assign({}, context.state.settings.resize, { aspectRatio: ratio }),
                context.state.settings.resize.width || size.width
            );
            Object.assign(context.state.settings.resize, next);
        },
        onRender: function onRender(context) {
            updatePanel(context.state);
        },
        // 建立等比鎖定的 resize 寬高控制。
        buildPanel: function buildPanel(context) {
            var state = context.state;
            var controller = context.controller;
            var resize = state.settings.resize;
            if (!resize.aspectRatio) {
                resize.aspectRatio = ratioFromSize(resize.width, resize.height);
            }
            resize.aspectRatio = normalizedRatio(resize.aspectRatio);
            Object.assign(resize, resizeFromWidth(resize, resize.width));
            var maxWidth = widthLimit(resize.aspectRatio);
            var maxHeight = heightLimit(resize.aspectRatio);
            var widthInput = null;
            var heightInput = null;
            widthInput = ui.unitNumberInput(resize.width, MIN_RESIZE_SIZE, maxWidth, 1, 'px', function (value) {
                var next = resizeFromWidth(state.settings.resize, value);
                widthInput.setValue(next.width);
                if (heightInput) {
                    heightInput.setValue(next.height, true);
                }
                controller.updateSettings('resize', next);
            });
            heightInput = ui.unitNumberInput(resize.height, MIN_RESIZE_SIZE, maxHeight, 1, 'px', function (value) {
                var next = resizeFromHeight(state.settings.resize, value);
                widthInput.setValue(next.width, true);
                heightInput.setValue(next.height);
                controller.updateSettings('resize', next);
            });
            panelRefs = {
                width: widthInput,
                height: heightInput
            };
            return ui.section('panelResize', [
                app.utils.dom.el('div', {
                    className: 'resize-size-row',
                    children: [
                        sizeField(ui.t('labelWidth'), widthInput),
                        linkedSizeIcon(),
                        sizeField(ui.t('labelHeight'), heightInput)
                    ]
                })
            ], 'resize');
        },
        operation: {
            // Resize operation 在尺寸不同時才重採樣。
            run: function run(imageData, settings) {
                // 尺寸沒有變更時直接回傳原 ImageData，避免多一次 canvas resize。
                var width = clampSize(settings.width || imageData.width);
                var height = clampSize(settings.height || imageData.height);
                if (width === imageData.width && height === imageData.height) {
                    return imageData;
                }
                return app.core.canvasUtils.resizeImageData(imageData, width, height);
            }
        }
    });
})(window.DitherApp);
