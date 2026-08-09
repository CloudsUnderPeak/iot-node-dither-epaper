(function (app) {
    // Palette feature 管理色票 UI、原圖代表色萃取，以及 Dither 關閉時的 palette quantization。
    // Dither 啟用時，preset/custom 色票只作為 Dither 的固定目標色集合。
    var ui = app.pages.ditherEditor.panelUtils;
    var constants = app.pages.ditherEditor.constants;
    var ORIGINAL_PRESET_ID = 'original';
    var CUSTOM_PRESET_ID = 'custom';
    var activePanelContext = null;

    function normalizeOriginalPaletteSize(value) {
        var size = Number(value);
        if (!Number.isFinite(size)) {
            return constants.DEFAULT_ORIGINAL_PALETTE_SIZE;
        }
        return Math.max(
            constants.MIN_ORIGINAL_PALETTE_SIZE,
            Math.min(constants.MAX_ORIGINAL_PALETTE_SIZE, Math.round(size))
        );
    }

    // 將色彩通道限制成 0-255 整數；round 語意統一由 core 提供。
    function clampByte(value) {
        return app.core.colorUtils.clampByte(Number(value) || 0);
    }

    // 複製單一 RGB 色彩並正規化通道。
    function copyColor(color) {
        return {
            r: clampByte(color && color.r),
            g: clampByte(color && color.g),
            b: clampByte(color && color.b)
        };
    }

    // 深拷貝 palette，避免 UI 編輯時直接改到 preset 常數。
    function copyPalette(palette) {
        return (palette || []).map(copyColor);
    }

    // 舊資料中的 none 或空值都視為 Original。
    function normalizePresetId(id) {
        return id === 'none' || !id ? ORIGINAL_PRESET_ID : id;
    }

    // 從靜態 palette preset 清單中找設定。
    function findPreset(id) {
        return app.pages.ditherEditor.config.palettePresets.find(function (entry) {
            return entry.id === id;
        });
    }

    // hex 與 RGB 互轉統一走 core.colorUtils，feature 不自寫轉換。
    function colorToHex(color) {
        return app.core.colorUtils.rgbToHex(color);
    }

    function hexToColor(value) {
        return app.core.colorUtils.hexToRgb(value);
    }

    function extractOriginalPalette(imageData, paletteSize) {
        // Original palette 直接使用 vendored RgbQuant，避免手寫 reducer 與 ditherit-v2 漂移。
        return app.pages.ditherEditor.rgbQuantAdapter.extractPalette(
            imageData,
            paletteSize
        );
    }

    function originalPaletteSource(state) {
        if (!state.sourceImageData) {
            return null;
        }
        return app.pages.ditherEditor.pipelineRunner.runPanelGroup(
            state.sourceImageData,
            state,
            app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE
        );
    }

    // Original palette samples the crop/prepare output before resize/adjust/dither.
    // 重新從裁切後範圍萃取 Original 色票。
    function refreshOriginalPalette(state, imageData) {
        var source = imageData || originalPaletteSource(state);
        if (!source) {
            return;
        }
        state.settings.palette.originalPaletteSize = normalizeOriginalPaletteSize(
            state.settings.palette.originalPaletteSize
        );
        state.settings.palette.originalPalette = extractOriginalPalette(
            source,
            state.settings.palette.originalPaletteSize
        );
    }

    // 面板打開時若尚未萃取色票，就補萃取一次。
    function ensureOriginalPalette(state) {
        if (!state.settings.palette.originalPalette || !state.settings.palette.originalPalette.length) {
            refreshOriginalPalette(state);
        }
    }

    function syncOriginalPalettePanel(state) {
        if (!activePanelContext || activePanelContext.state !== state) {
            return;
        }
        if (activePanelContext.renderSwatches) {
            activePanelContext.renderSwatches();
        }
        if (activePanelContext.updateOriginalControls) {
            activePanelContext.updateOriginalControls();
        }
    }

    // 依目前 preset/custom/original 設定取出實際要顯示或套用的 palette。
    function currentPalette(settings) {
        var presetId = normalizePresetId(settings.presetId);
        if (presetId === ORIGINAL_PRESET_ID) {
            return copyPalette(settings.originalPalette);
        }
        if (presetId === CUSTOM_PRESET_ID && settings.palette && settings.palette.length) {
            return copyPalette(settings.palette);
        }
        if (settings.palette && settings.palette.length) {
            return copyPalette(settings.palette);
        }
        var preset = findPreset(presetId);
        return preset ? copyPalette(preset.colors) : [];
    }

    // 透過 registry api 查詢 dither 是否啟用；dither feature 停用時視為未啟用。
    function ditherIsActive(state) {
        var ditherApi = app.pages.ditherEditor.featureRegistry.api('dither');
        if (!ditherApi || !state) {
            return false;
        }
        var enabled = state.pipeline && state.pipeline.enabled;
        return ditherApi.isActive(state) && (!enabled || enabled.dither !== false);
    }

    // dither 停用或未提供時退回專案預設 color distance。
    function resolvedColorDistance(state) {
        var ditherApi = app.pages.ditherEditor.featureRegistry.api('dither');
        return ditherApi
            ? ditherApi.getColorDistance(state)
            : app.core.paletteUtils.normalizeColorDistanceId(null);
    }

    // Dither 啟用時由 Dither 依目前 palette 落色；Palette 不先量化，避免抖動前誤差被吃掉。
    function shouldApplyPalette(settings, state) {
        return normalizePresetId(settings.presetId) !== ORIGINAL_PRESET_ID && !ditherIsActive(state);
    }

    function setCustomPalette(context, palette, options) {
        // 使用者新增、刪除或改色後立刻切到 custom palette，並同步給 Dither 使用。
        var colors = copyPalette(palette);
        context.state.settings.palette.palette = colors.length ? colors : null;
        context.state.settings.palette.presetId = colors.length ? CUSTOM_PRESET_ID : ORIGINAL_PRESET_ID;
        context.presetInput.value = context.state.settings.palette.presetId;
        if (context.updateOriginalControls) {
            context.updateOriginalControls();
        }
        if (!options || options.render !== false) {
            context.renderSwatches();
        }
        if (!options || options.preview !== false) {
            context.controller.schedulePreview();
        }
    }

    function buildPaletteEditor(context) {
        // 色票本身就是可編輯控制：點圓點改色，hover/focus 才顯示刪除按鈕。
        var swatches = app.utils.dom.el('div', { className: 'palette-swatches' });

        // 重新渲染色票列，通常在新增/刪除/切 preset 後使用。
        context.renderSwatches = function renderSwatches() {
            var colors = currentPalette(context.state.settings.palette);
            app.utils.dom.clear(swatches);
            colors.forEach(function (color, index) {
                var colorInput = app.utils.dom.el('input', {
                    className: 'palette-color-input',
                    attrs: {
                        type: 'color',
                        value: colorToHex(color),
                        'aria-label': ui.t('labelPaletteColor') + ' ' + (index + 1)
                    }
                });
                var removeButton = app.utils.dom.el('button', {
                    className: 'palette-remove-button',
                    children: [
                        ui.svgIcon('assets/icons/editor/palette-remove.svg', { fallbackText: 'x' })
                    ],
                    attrs: {
                        type: 'button',
                        'aria-label': ui.t('actionRemovePaletteColor') + ' ' + (index + 1)
                    }
                });
                colorInput.addEventListener('input', function () {
                    colors[index] = hexToColor(colorInput.value);
                    setCustomPalette(context, colors, { render: false, preview: false });
                });
                colorInput.addEventListener('change', function () {
                    colors[index] = hexToColor(colorInput.value);
                    setCustomPalette(context, colors, { render: false });
                });
                removeButton.addEventListener('click', function () {
                    colors.splice(index, 1);
                    setCustomPalette(context, colors);
                });
                swatches.appendChild(app.utils.dom.el('span', {
                    className: 'palette-swatch',
                    children: [colorInput, removeButton]
                }));
            });
            var addButton = app.utils.dom.el('button', {
                className: 'palette-add-button',
                children: [
                    ui.svgIcon('assets/icons/editor/palette-add.svg', { fallbackText: '+' })
                ],
                attrs: { type: 'button', 'aria-label': ui.t('actionAddPaletteColor') }
            });
            addButton.addEventListener('click', function () {
                setCustomPalette(context, colors.concat([{ r: 0, g: 0, b: 0 }]));
            });
            swatches.appendChild(addButton);
        };

        context.renderSwatches();
        return app.utils.dom.el('div', {
            className: 'palette-editor',
            children: [
                app.utils.dom.el('div', { className: 'palette-editor-label', text: ui.t('labelPalette') }),
                swatches
            ]
        });
    }

    app.pages.ditherEditor.featureRegistry.register({
        id: 'palette',
        icon: '#',
        iconPath: 'assets/icons/editor/palette.svg',
        labelKey: 'panelPalette',
        panelGroup: 'edit',
        pipelineStage: 'effectsOrder',
        pipelineOrder: 20,
        // 預設顯示 Original 代表色，不先套用固定色盤。
        defaultSettings: function defaultSettings() {
            return {
                presetId: ORIGINAL_PRESET_ID,
                palette: null,
                originalPalette: null,
                originalPaletteSize: constants.DEFAULT_ORIGINAL_PALETTE_SIZE
            };
        },
        // 新圖片載入後只重設 Original 狀態；prepare 結束時才萃取色票。
        onImageLoaded: function onImageLoaded(context) {
            var settings = context.state.settings.palette;
            if (normalizePresetId(settings.presetId) === ORIGINAL_PRESET_ID) {
                settings.presetId = ORIGINAL_PRESET_ID;
                settings.palette = null;
            }
        },
        // 只處理 presetId 變更；色票直接編輯由 setCustomPalette 管理。
        onSettingChanged: function onSettingChanged(context) {
            if (context.id !== 'palette' || context.key !== 'presetId') {
                return;
            }
            var presetId = normalizePresetId(context.value);
            context.state.settings.palette.presetId = presetId;
            if (presetId === ORIGINAL_PRESET_ID) {
                ensureOriginalPalette(context.state);
                context.state.settings.palette.palette = null;
                return;
            }
            if (presetId === CUSTOM_PRESET_ID) {
                var existingPalette = context.state.settings.palette.palette;
                context.state.settings.palette.palette = existingPalette && existingPalette.length
                    ? currentPalette(context.state.settings.palette)
                    : copyPalette(context.state.settings.palette.originalPalette);
                return;
            }
            var preset = findPreset(presetId) || app.pages.ditherEditor.config.palettePresets[0];
            context.state.settings.palette.palette = copyPalette(preset.colors);
        },
        onPrepareCommitted: function onPrepareCommitted(context) {
            refreshOriginalPalette(context.state);
            if (normalizePresetId(context.state.settings.palette.presetId) === ORIGINAL_PRESET_ID) {
                context.state.settings.palette.palette = null;
            }
            syncOriginalPalettePanel(context.state);
        },
        // 建立 palette 色票編輯器與 preset 下拉選單。
        buildPanel: function buildPanel(context) {
            var state = context.state;
            var controller = context.controller;
            state.settings.palette.presetId = normalizePresetId(state.settings.palette.presetId);
            state.settings.palette.originalPaletteSize = normalizeOriginalPaletteSize(
                state.settings.palette.originalPaletteSize
            );
            ensureOriginalPalette(state);
            var options = [
                { value: ORIGINAL_PRESET_ID, label: ui.t('paletteOriginal') },
                { value: CUSTOM_PRESET_ID, label: ui.t('paletteCustom') }
            ].concat(
                app.pages.ditherEditor.config.palettePresets.map(function (preset) {
                    return { value: preset.id, label: ui.t(preset.labelKey) };
                })
            );
            var paletteContext = {
                state: state,
                controller: controller,
                presetInput: null,
                renderSwatches: null,
                updateOriginalControls: null
            };
            activePanelContext = paletteContext;
            var originalSizeInput = ui.unitNumberInput(
                state.settings.palette.originalPaletteSize,
                constants.MIN_ORIGINAL_PALETTE_SIZE,
                constants.MAX_ORIGINAL_PALETTE_SIZE,
                1,
                '',
                function (value) {
                    state.settings.palette.originalPaletteSize = normalizeOriginalPaletteSize(value);
                    refreshOriginalPalette(state);
                    paletteContext.renderSwatches();
                    controller.schedulePreview();
                }
            );
            var originalSizeRow = ui.row(ui.t('labelNumberOfColors'), originalSizeInput);
            originalSizeRow.classList.add('palette-colors-row');
            paletteContext.updateOriginalControls = function updateOriginalControls() {
                originalSizeRow.style.display = normalizePresetId(state.settings.palette.presetId) === ORIGINAL_PRESET_ID
                    ? ''
                    : 'none';
            };
            var presetInput = ui.selectInput(state.settings.palette.presetId, options, function (value) {
                controller.updateSetting('palette', 'presetId', value);
                paletteContext.renderSwatches();
                paletteContext.updateOriginalControls();
            });
            var presetRow = ui.row(ui.t('labelPreset'), presetInput.node);
            paletteContext.presetInput = presetInput.select;
            paletteContext.updateOriginalControls();
            return ui.section('panelPalette', [
                buildPaletteEditor(paletteContext),
                presetRow,
                originalSizeRow
            ], 'palette');
        },
        // 跨 feature 查詢介面：dither/adjust 只能經由這裡取得 palette 狀態。
        api: {
            // 目前生效的 palette（original/custom/preset），無可用色票時回空陣列。
            getActivePalette: function getActivePalette(state) {
                var settings = state && state.settings && state.settings.palette;
                return settings ? currentPalette(settings) : [];
            },
            // palette quantization 是否會實際改變像素（dither 啟用時由 dither 落色，不算 active）。
            isActive: function isActive(state) {
                var settings = state && state.settings && state.settings.palette;
                if (!settings) {
                    return false;
                }
                return shouldApplyPalette(settings, state) && currentPalette(settings).length > 0;
            }
        },
        operation: {
            pipeline: {
                draggable: false
            },
            cacheKey: function cacheKey(settings, context) {
                var state = context && context.state;
                // 輸出只受「會不會套用」與 color distance 影響；palette 本身在 settings 序列化內。
                var applies = shouldApplyPalette(settings, state);
                return {
                    applies: applies,
                    colorDistance: applies ? resolvedColorDistance(state) : null
                };
            },
            // Dither 關閉時，Palette operation 會把每個像素替換成 palette 中的最近色。
            run: function run(imageData, settings, context) {
                var state = context && context.state;
                var palette = currentPalette(settings);
                if (!shouldApplyPalette(settings, state) || !palette.length) {
                    return imageData;
                }
                var data = new Uint8ClampedArray(imageData.data);
                var nearestColor = app.core.paletteUtils.createNearestColorFinder(
                    palette,
                    resolvedColorDistance(state)
                );
                for (var i = 0; i < data.length; i += 4) {
                    var nearest = nearestColor({ r: data[i], g: data[i + 1], b: data[i + 2] });
                    data[i] = nearest.r;
                    data[i + 1] = nearest.g;
                    data[i + 2] = nearest.b;
                    data[i + 3] = 255;
                }
                return new ImageData(data, imageData.width, imageData.height);
            }
        }
    });
})(window.DitherApp);
