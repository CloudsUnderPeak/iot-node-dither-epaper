(function (app) {
    // Dither feature 只負責選擇演算法並把目前 palette 傳給對應處理器。
    // palette 的建立與同步在 palette-feature.js；這裡不重複管理色票狀態。
    var ui = app.pages.ditherEditor.panelUtils;
    var constants = app.pages.ditherEditor.constants;

    function colorDistanceOptions() {
        return app.pages.ditherEditor.config.colorDistanceMetrics.map(function (metric) {
            return { value: metric.id, label: ui.t(metric.labelKey) };
        });
    }

    function paletteMappingOptions() {
        return app.pages.ditherEditor.config.paletteMappingModes.map(function (mode) {
            return { value: mode.id, label: ui.t(mode.labelKey) };
        });
    }

    function algorithmById(id) {
        return app.pages.ditherEditor.ditherAlgorithmRegistry.get(id);
    }

    // Dither 的目標色票由 palette feature 提供；palette 停用時回空陣列（dither no-op）。
    function activePalette(state) {
        var paletteApi = app.pages.ditherEditor.featureRegistry.api('palette');
        return paletteApi && state ? paletteApi.getActivePalette(state) : [];
    }

    function strengthLabelKey(id) {
        var registry = app.pages.ditherEditor.ditherAlgorithmRegistry;
        if (registry.supportsDotDensity(id)) {
            return 'labelDotDensity';
        }
        return registry.supportsThresholdStrength(id) ? 'labelDitherStrength' : 'labelErrorStrength';
    }

    function supportsStrength(id) {
        var registry = app.pages.ditherEditor.ditherAlgorithmRegistry;
        return registry.supportsErrorStrength(id)
            || registry.supportsThresholdStrength(id)
            || registry.supportsDotDensity(id);
    }

    function normalizeErrorStrength(value) {
        var number = Number(value);
        if (!Number.isFinite(number)) {
            return constants.DEFAULT_DITHER_ERROR_STRENGTH;
        }
        return Math.max(
            constants.MIN_DITHER_ERROR_STRENGTH,
            Math.min(constants.MAX_DITHER_ERROR_STRENGTH, number)
        );
    }

    app.pages.ditherEditor.featureRegistry.register({
        id: 'dither',
        icon: '..',
        iconPath: 'assets/icons/editor/dither.svg',
        labelKey: 'panelDither',
        panelGroup: 'edit',
        pipelineStage: 'effectsOrder',
        pipelineOrder: 30,
        // 預設以 Floyd-Steinberg error diffusion 處理；palette 由 palette feature 同步。
        defaultSettings: function defaultSettings() {
            // palette 不存在 dither settings 內：改由 registry api 向 palette feature 查詢，
            // stage cache 的失效由 operation.cacheKey 帶入 palette 內容。
            return {
                algorithm: constants.DEFAULT_DITHER_ALGORITHM_ID,
                paletteMapping: constants.DEFAULT_PALETTE_MAPPING_ID,
                serpentine: false,
                colorDistance: constants.DEFAULT_COLOR_DISTANCE_ID,
                errorStrength: constants.DEFAULT_DITHER_ERROR_STRENGTH
            };
        },
        // 跨 feature 查詢介面：palette/adjust 只能經由這裡取得 dither 狀態。
        api: {
            isActive: function isActive(state) {
                var settings = state && state.settings && state.settings.dither;
                return Boolean(settings && settings.algorithm && settings.algorithm !== 'none');
            },
            getColorDistance: function getColorDistance(state) {
                var settings = state && state.settings && state.settings.dither;
                return app.core.paletteUtils.normalizeColorDistanceId(
                    settings && settings.colorDistance
                );
            }
        },
        // 建立演算法、color distance、serpentine 與 error strength 控制。
        buildPanel: function buildPanel(context) {
            var state = context.state;
            var controller = context.controller;
            state.settings.dither.colorDistance = app.core.paletteUtils.normalizeColorDistanceId(
                state.settings.dither.colorDistance
            );
            state.settings.dither.paletteMapping = app.pages.ditherEditor.paletteMapping.normalizeId(
                state.settings.dither.paletteMapping
            );
            state.settings.dither.errorStrength = normalizeErrorStrength(state.settings.dither.errorStrength);
            var options = [{ value: 'none', label: ui.t('optionNone') }].concat(
                app.pages.ditherEditor.ditherAlgorithmRegistry.list().map(function (algorithm) {
                    return { value: algorithm.id, label: ui.t(algorithm.labelKey) };
                })
            );
            var errorStrengthControl = ui.labeledRange({
                value: state.settings.dither.errorStrength,
                min: constants.MIN_DITHER_ERROR_STRENGTH,
                max: constants.MAX_DITHER_ERROR_STRENGTH,
                step: constants.DITHER_ERROR_STRENGTH_STEP,
                normalize: normalizeErrorStrength,
                format: function (value) {
                    return value + '%';
                },
                className: 'dither-range-control',
                valueClassName: 'dither-range-value',
                interaction: ui.previewHoldHandlers(controller, 'dither'),
                onChange: function (value) {
                    controller.updateSetting('dither', 'errorStrength', value);
                }
            });
            errorStrengthControl.setDisabled(!supportsStrength(state.settings.dither.algorithm));
            var strengthRow = ui.row(
                ui.t(strengthLabelKey(state.settings.dither.algorithm)),
                errorStrengthControl
            );
            var strengthLabel = strengthRow.querySelector('label');
            function updateStrengthControl(algorithmId) {
                strengthLabel.textContent = ui.t(strengthLabelKey(algorithmId));
                errorStrengthControl.setDisabled(!supportsStrength(algorithmId));
            }
            var serpentineTip = ui.t('tipSerpentine');
            var serpentineLabel = app.utils.dom.el('label', {
                children: [
                    app.utils.dom.el('span', {
                        className: 'control-label-content has-tip',
                        children: [
                            app.utils.dom.el('span', {
                                className: 'control-label-text',
                                text: ui.t('labelSerpentine')
                            }),
                            app.utils.dom.el('span', {
                                className: 'control-tip-icon',
                                attrs: {
                                    role: 'img',
                                    tabindex: '0',
                                    'data-tooltip': serpentineTip,
                                    'aria-label': serpentineTip
                                },
                                children: [
                                    ui.svgIcon('assets/icons/editor/info-circle.svg')
                                ]
                            })
                        ]
                    })
                ]
            });
            var serpentineControl = ui.toggleSwitchInput(state.settings.dither.serpentine, function (value) {
                controller.updateSetting('dither', 'serpentine', value);
            });
            var rows = [
                ui.row(ui.t('labelAlgorithm'), ui.selectInput(state.settings.dither.algorithm, options, function (value) {
                    updateStrengthControl(value);
                    errorStrengthControl.setValue(constants.DEFAULT_DITHER_ERROR_STRENGTH);
                    controller.updateSettings('dither', {
                        algorithm: value,
                        errorStrength: constants.DEFAULT_DITHER_ERROR_STRENGTH
                    });
                }).node),
                ui.row(ui.t('labelPaletteMapping'), ui.selectInput(
                    state.settings.dither.paletteMapping,
                    paletteMappingOptions(),
                    function (value) {
                        controller.updateSetting('dither', 'paletteMapping', value);
                    }
                ).node),
                ui.row(ui.t('labelColorDistance'), ui.selectInput(
                    state.settings.dither.colorDistance,
                    colorDistanceOptions(),
                    function (value) {
                        controller.updateSetting('dither', 'colorDistance', value);
                    }
                ).node),
                app.utils.dom.el('div', {
                    className: 'control-row',
                    children: [serpentineLabel, serpentineControl]
                }),
                strengthRow
            ];
            return ui.section('panelDither', rows, 'dither');
        },
        operation: {
            pipeline: {
                draggable: false
            },
            cacheKey: function cacheKey(settings, context) {
                // palette 來自 palette feature（不在 dither settings 內），
                // 必須進 cache key，色票變更時 dither stage 才會重算。
                return { palette: activePalette(context && context.state) };
            },
            // 依演算法 registry 找到對應 processor；Dither feature 不硬寫 processor 清單。
            run: function run(imageData, settings, context) {
                // none 代表 pipeline 保留此工具但不套用任何 dithering。
                if (settings.algorithm === 'none') {
                    return imageData;
                }
                var registry = app.pages.ditherEditor.ditherAlgorithmRegistry;
                var algorithm = algorithmById(settings.algorithm) || registry.first();
                var palette = activePalette(context && context.state);
                if (!palette.length) {
                    return imageData;
                }
                var strength = normalizeErrorStrength(settings.errorStrength);
                var options = {
                    matrixId: algorithm.matrixId,
                    palette: palette,
                    paletteMapping: app.pages.ditherEditor.paletteMapping.normalizeId(settings.paletteMapping),
                    colorDistance: app.core.paletteUtils.normalizeColorDistanceId(settings.colorDistance),
                    serpentine: Boolean(settings.serpentine && algorithm.supportsSerpentine),
                    errorStrength: strength
                };
                if (algorithm.supportsThresholdStrength) {
                    options.thresholdScale = (algorithm.thresholdScale || 70) * strength / 100;
                    options.thresholdStrength = strength / 100;
                }
                if (algorithm.supportsDotDensity) {
                    options.dotDensity = strength / 100;
                }
                // 擴散類演算法是 CPU 重路徑，優先丟給 worker 避免凍結 UI；
                // ordered/pattern 留主執行緒（GPU 加速或單趟 CPU 已夠快）。
                // worker 不可用或失敗時 fallback 同步計算，輸出與 worker 一致。
                var isDiffusion = algorithm.processorId === 'error-diffusion'
                    || algorithm.processorId === 'adaptive-error-diffusion'
                    || algorithm.processorId === 'dot-diffusion';
                var workerClient = app.pages.ditherEditor.ditherWorkerClient;
                if (isDiffusion && workerClient) {
                    var workerRun = workerClient.run(imageData, algorithm, options);
                    if (workerRun) {
                        return workerRun.catch(function () {
                            return registry.run(imageData, algorithm, options);
                        });
                    }
                }
                return registry.run(imageData, algorithm, options);
            }
        }
    });
})(window.DitherApp);
