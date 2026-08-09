(function (app) {
    // Dither Editor 的協調器：維護 editor state、觸發 feature hooks、排程 preview/export。
    // 這裡不直接建立 DOM；畫面更新一律透過外部傳入的 render callback。
    // Controller 保存目前 editor state，並提供頁面 UI 可呼叫的操作方法。
    function DitherEditorController(options) {
        options = options || {};
        this.state = options.initialState || app.pages.ditherEditor.state.create();
        this.render = options.render;
        this.renderLivePreview = options.renderLivePreview || options.render;
        this.renderPreviewTimingLabel = options.renderPreviewTimingLabel || null;
        this.setStatus = options.setStatus;
        this.previewTimer = null;
        this.previewTimingHideTimer = null;
        this.livePreviewFrame = null;
        this.previewHoldDepth = 0;
        this.previewPending = false;
        this.stageCache = options.stageCache || app.pages.ditherEditor.pipelineRunner.createStageCache();
    }

    function nowMs() {
        return window.performance && window.performance.now
            ? window.performance.now()
            : Date.now();
    }

    // core 錯誤只帶 code；顯示文字在這裡對應 i18n，未知錯誤退回原訊息或通用文字。
    var ERROR_TEXT_KEYS = {
        'unsupported-format': 'errorUnsupportedFormat',
        'image-load-failed': 'errorImageLoadFailed',
        'image-processing-blocked': 'errorImageProcessingBlocked',
        'demo-load-failed': 'errorDemoLoadFailed',
        'demo-manifest-missing': 'errorDemoManifestMissing',
        'demo-data-missing': 'errorDemoDataMissing'
    };

    function errorText(error) {
        if (error && (error.code || error.status) && app.device.errorText) {
            return app.device.errorText(error);
        }
        var key = error && error.code ? ERROR_TEXT_KEYS[error.code] : null;
        if (key) {
            return app.i18n.t(key);
        }
        return (error && error.message) || app.i18n.t('errorGeneric');
    }

    // 將 imageLoader 回傳結果寫入 state，並通知 feature 進行 onImageLoaded 初始化。
    DitherEditorController.prototype.loadResult = function loadResult(result, fileName) {
        // 所有圖片來源（upload/demo/new image）最後都收斂到 loadResult，
        // 先重建預設 state，確保重新載圖時不沿用上一張圖的演算法設定。
        this.resetStateForImage();
        this.state.fileName = fileName || 'Untitled';
        this.state.sourceImageData = result.imageData;
        this.state.livePreview = null;
        this.state.originalSize = result.originalSize;
        this.state.workingSize = result.workingSize;
        this.runFeatureHook('onImageLoaded', { result: result });
        app.pages.ditherEditor.targetPolicy.sync(this.state);
        this.state.status = 'ready';
        app.pages.ditherEditor.editorModeStateMachine.enterPrepare(this.state);
        // 新圖載入後若有 prepare 入口就直接跳到 prepare；否則進入 edit 並排正式 preview。
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            this.commitPrepareChanges();
            this.schedulePreview();
            return;
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.resetStateForImage = function resetStateForImage() {
        var nextState = app.pages.ditherEditor.state.create();
        var state = this.state;
        var revision = (state.uiRevision || 0) + 1;
        Object.keys(state).forEach(function (key) {
            delete state[key];
        });
        Object.keys(nextState).forEach(function (key) {
            state[key] = nextState[key];
        });
        state.uiRevision = revision;
        this.previewPending = false;
        this.previewHoldDepth = 0;
        this.previewRunId = (this.previewRunId || 0) + 1;
        this.hidePreviewTimingLabel();
        app.pages.ditherEditor.pipelineRunner.clearStageCache(this.stageCache);
        clearTimeout(this.previewTimer);
        this.previewTimer = null;
        if (this.livePreviewFrame) {
            cancelAnimationFrame(this.livePreviewFrame);
            this.livePreviewFrame = null;
        }
    };

    // 建立預設尺寸白底圖；目前 UI 不暴露此入口，保留給後續 blank-canvas flow。
    DitherEditorController.prototype.newImage = function newImage() {
        var size = app.pages.ditherEditor.constants.DEFAULT_NEW_IMAGE_SIZE;
        this.loadResult(app.core.imageLoader.createBlankImage(size.width, size.height), 'Untitled');
    };

    // 載入專案內建 demo 圖，成功後走和一般檔案相同的 loadResult 流程。
    DitherEditorController.prototype.loadDemo = function loadDemo() {
        var self = this;
        var max = app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE;
        this.state.status = 'loading-image';
        this.state.previewRenderDurationMs = null;
        this.hidePreviewTimingLabel();
        this.render(this.state);
        return app.core.imageLoader
            .loadDemoImage(max)
            .then(function (result) {
                self.loadResult(result, result.fileName || 'Demo image');
            })
            .catch(function (error) {
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    // 載入使用者選取或拖放的檔案，格式檢查由 imageLoader 負責。
    DitherEditorController.prototype.loadFile = function loadFile(file) {
        var self = this;
        this.state.status = 'loading-image';
        this.state.previewRenderDurationMs = null;
        this.hidePreviewTimingLabel();
        this.render(this.state);
        return app.core.imageLoader
            .loadImageFromFile(file, app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE)
            .then(function (result) {
                self.loadResult(result, file.name);
            })
            .catch(function (error) {
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    // 更新單一 feature setting，觸發 feature hook、重繪與排程 preview。
    DitherEditorController.prototype.updateSetting = function updateSetting(group, key, value) {
        if (!app.pages.ditherEditor.editorModeStateMachine.canUseSettingGroup(this.state, group)) {
            return;
        }
        if (!app.pages.ditherEditor.targetPolicy.settingAllowed(this.state, group, key, value)) {
            return;
        }
        var previous = Object.assign({}, this.state.settings[group]);
        this.state.settings[group][key] = value;
        // previous 讓 feature 可以在 normalize 或 UI sync 時知道變更前狀態。
        this.runFeatureHook('onSettingChanged', { id: group, key: key, value: value, previous: previous }, { broadcast: true });
        app.pages.ditherEditor.targetPolicy.sync(this.state);
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE) {
            this.state.status = 'ready';
            this.render(this.state);
            return;
        }
        this.schedulePreview();
    };

    // 一次更新多個 setting，給 flip/rotation/pan 等需要同步修改的操作使用。
    DitherEditorController.prototype.updateSettings = function updateSettings(group, values) {
        if (!app.pages.ditherEditor.editorModeStateMachine.canUseSettingGroup(this.state, group)) {
            return;
        }
        if (!app.pages.ditherEditor.targetPolicy.settingAllowed(this.state, group, null, null)) {
            return;
        }
        var previous = Object.assign({}, this.state.settings[group]);
        Object.assign(this.state.settings[group], values);
        this.runFeatureHook('onSettingChanged', { id: group, key: null, values: values, previous: previous }, { broadcast: true });
        app.pages.ditherEditor.targetPolicy.sync(this.state);
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE) {
            this.state.status = 'ready';
            this.render(this.state);
            return;
        }
        this.schedulePreview();
    };

    // 使用者拖曳滑桿時進入 preview hold，先建立 live preview 基底。
    DitherEditorController.prototype.beginPreviewHold = function beginPreviewHold(id) {
        this.previewHoldDepth += 1;
        if (!this.state.livePreview) {
            var feature = app.pages.ditherEditor.featureRegistry.get(id);
            // livePreview 是拖曳期間的輕量回饋，不代表正式 pipeline 結果。
            this.state.livePreview = {
                id: id,
                baseImageData: feature && feature.createLivePreviewBase
                    ? feature.createLivePreviewBase({ state: this.state, stageCache: this.stageCache })
                    : null
            };
        }
        clearTimeout(this.previewTimer);
        this.previewTimer = null;
    };

    // 使用者放開滑桿後離開 live preview，清掉 filter 並補跑正式 pipeline。
    DitherEditorController.prototype.endPreviewHold = function endPreviewHold() {
        this.previewHoldDepth = Math.max(0, this.previewHoldDepth - 1);
        if (this.previewHoldDepth !== 0) {
            return;
        }
        if (this.previewPending) {
            this.previewPending = false;
            clearTimeout(this.previewTimer);
            this.previewTimer = null;
            this.schedulePreview();
            return;
        }
        this.state.livePreview = null;
        this.render(this.state);
    };

    // 用 requestAnimationFrame 合併 live preview 更新，避免 input 事件過密。
    DitherEditorController.prototype.scheduleLivePreview = function scheduleLivePreview() {
        var self = this;
        if (this.livePreviewFrame) {
            return;
        }
        this.livePreviewFrame = requestAnimationFrame(function () {
            self.livePreviewFrame = null;
            self.renderLivePreview(self.state);
        });
    };

    // 封裝 feature lifecycle hook 呼叫，並自動補上 state/controller。
    DitherEditorController.prototype.runFeatureHook = function runFeatureHook(name, context, options) {
        context = context || {};
        options = options || {};
        app.pages.ditherEditor.featureRegistry.dispatch(
            name,
            Object.assign({ state: this.state }, context),
            options.broadcast ? null : { id: context.id }
        );
    };

    // 啟用或停用 pipeline 中的某個 operation。
    DitherEditorController.prototype.toggleOperation = function toggleOperation(id, enabled) {
        if (this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return;
        }
        this.state.pipeline.enabled[id] = enabled;
        this.schedulePreview();
    };

    // 接收 sortable list 回傳的新順序，更新 effectsOrder。
    DitherEditorController.prototype.reorderEffects = function reorderEffects(order) {
        if (this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return;
        }
        this.state.pipeline.effectsOrder = order.slice();
        this.schedulePreview();
    };

    DitherEditorController.prototype.commitPrepareChanges = function commitPrepareChanges() {
        this.state.preparedImageData = null;
        this.runFeatureHook('onPrepareCommitted', {}, { broadcast: true });
    };

    DitherEditorController.prototype.setPreviewTimingPhase = function setPreviewTimingPhase(phase, durationMs) {
        clearTimeout(this.previewTimingHideTimer);
        this.previewTimingHideTimer = null;
        this.state.previewTimingLabel = {
            phase: phase,
            durationMs: Number.isFinite(durationMs) ? durationMs : null
        };
    };

    DitherEditorController.prototype.hidePreviewTimingLabel = function hidePreviewTimingLabel() {
        clearTimeout(this.previewTimingHideTimer);
        this.previewTimingHideTimer = null;
        this.state.previewTimingLabel = {
            phase: 'hidden',
            durationMs: null
        };
    };

    DitherEditorController.prototype.schedulePreviewTimingHide = function schedulePreviewTimingHide() {
        var self = this;
        var delayMs = app.pages.ditherEditor.constants.PREVIEW_TIMING_LABEL_HIDE_DELAY_MS;
        clearTimeout(this.previewTimingHideTimer);
        this.previewTimingHideTimer = setTimeout(function () {
            self.previewTimingHideTimer = null;
            self.state.previewTimingLabel = {
                phase: 'hidden',
                durationMs: null
            };
            if (self.renderPreviewTimingLabel) {
                self.renderPreviewTimingLabel(self.state);
            } else {
                self.render(self.state);
            }
        }, delayMs);
    };

    DitherEditorController.prototype.updatePreparedPreview = function updatePreparedPreview() {
        if (!this.state.sourceImageData) {
            this.state.preparedImageData = null;
            return null;
        }
        this.state.preparedImageData = app.pages.ditherEditor.pipelineRunner.runPanelGroup(
            this.state.sourceImageData,
            this.state,
            app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE,
            { stageCache: this.stageCache }
        );
        return this.state.preparedImageData;
    };

    // 切換 Original/Result/Expand 檢視，不改變 pipeline 結果。
    DitherEditorController.prototype.setViewMode = function setViewMode(mode) {
        if (this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return;
        }
        mode = mode === 'original' || mode === 'pixel' ? mode : 'result';
        try {
            if (mode === 'original') {
                this.updatePreparedPreview();
            }
            this.state.viewMode = mode;
        } catch (error) {
            this.state.status = 'error';
            this.state.error = errorText(error);
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.openPrepareMode = function openPrepareMode() {
        if (!this.state.sourceImageData) {
            return;
        }
        clearTimeout(this.previewTimer);
        this.previewTimer = null;
        this.state.livePreview = null;
        this.state.status = 'ready';
        app.pages.ditherEditor.editorModeStateMachine.enterPrepare(this.state);
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            this.commitPrepareChanges();
            this.schedulePreview();
            return;
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.openSourcePanel = function openSourcePanel(activeTool) {
        var wasPrepareMode = this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE;
        app.pages.ditherEditor.editorModeStateMachine.openSourcePanel(this.state, activeTool);
        if (wasPrepareMode && this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.SOURCE) {
            this.schedulePreview();
            return;
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.openEditPanel = function openEditPanel(activeTool) {
        if (!this.state.sourceImageData) {
            return;
        }
        var wasPrepareMode = this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE;
        app.pages.ditherEditor.editorModeStateMachine.openEditPanel(this.state, activeTool);
        if (wasPrepareMode && this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            this.commitPrepareChanges();
            this.schedulePreview();
            return;
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.closePrepareMode = function closePrepareMode() {
        if (!this.state.sourceImageData) {
            return;
        }
        app.pages.ditherEditor.editorModeStateMachine.enterEdit(this.state);
        this.commitPrepareChanges();
        this.schedulePreview();
    };

    // 將正式 preview 計算 debounce，避免連續設定變更時每次都重跑 pipeline。
    DitherEditorController.prototype.schedulePreview = function schedulePreview() {
        var self = this;
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE) {
            this.state.status = 'ready';
            this.render(this.state);
            return;
        }
        this.state.previewRenderDurationMs = null;
        if (this.previewHoldDepth > 0) {
            // 使用者拖曳 slider 時先更新輕量 live preview，完整 pipeline 延到互動結束。
            this.previewPending = true;
            this.scheduleLivePreview();
            return;
        }
        clearTimeout(this.previewTimer);
        this.setPreviewTimingPhase('rendering');
        this.state.status = 'processing-preview';
        if (this.state.viewMode === 'original') {
            try {
                this.updatePreparedPreview();
            } catch (error) {
                this.state.status = 'error';
                this.state.error = errorText(error);
                this.hidePreviewTimingLabel();
                this.render(this.state);
                return;
            }
        }
        this.render(this.state);
        // debounce 避免 slider/select 每次 input 都立即跑完整 pipeline。
        this.previewTimer = setTimeout(function () {
            self.previewTimer = null;
            self.runPreview();
        }, app.pages.ditherEditor.constants.PREVIEW_DEBOUNCE_MS);
    };

    // 實際執行 pipeline 並把 resultImageData 寫回 state。
    // pipeline 可能包含 worker stage，因此回傳 Promise；previewRunId 會丟棄較舊結果。
    DitherEditorController.prototype.runPreview = function runPreview() {
        var self = this;
        if (!this.state.sourceImageData) {
            this.state.status = 'empty';
            this.hidePreviewTimingLabel();
            this.render(this.state);
            return Promise.resolve();
        }
        this.previewRunId = (this.previewRunId || 0) + 1;
        var runId = this.previewRunId;
        var startMs = nowMs();
        return Promise.resolve()
            .then(function () {
                self.runFeatureHook('onBeforePreview', {});
                // Preview 永遠從 sourceImageData 跑完整 pipeline，避免連續套用造成畫質累積劣化。
                return app.pages.ditherEditor.pipelineRunner.runAsync(
                    self.state.sourceImageData,
                    self.state,
                    { stageCache: self.stageCache }
                );
            })
            .then(function (imageData) {
                if (runId !== self.previewRunId) {
                    return;
                }
                self.state.previewImageData = imageData;
                self.state.previewRenderDurationMs = nowMs() - startMs;
                self.setPreviewTimingPhase('done', self.state.previewRenderDurationMs);
                self.state.outputImageData = self.state.previewImageData;
                self.state.status = 'preview-ready';
                self.runFeatureHook('onAfterPreview', {});
                self.schedulePreviewTimingHide();
                self.state.livePreview = null;
                self.render(self.state);
            })
            .catch(function (error) {
                if (runId !== self.previewRunId) {
                    return;
                }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.state.previewRenderDurationMs = null;
                self.hidePreviewTimingLabel();
                self.state.livePreview = null;
                self.render(self.state);
            });
    };

    // 匯出目前結果；pipeline 可能走 worker，exportRunId 支援取消後丟棄在途結果。
    DitherEditorController.prototype.exportPng = function exportPng() {
        var self = this;
        if (!this.state.sourceImageData || this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return Promise.resolve();
        }
        if (this.state.status === 'exporting') {
            return Promise.resolve();
        }
        this.exportRunId = (this.exportRunId || 0) + 1;
        var runId = this.exportRunId;
        this.state.status = 'exporting';
        this.render(this.state);
        return Promise.resolve()
            .then(function () {
                self.runFeatureHook('onBeforeExport', {});
                // Export 不使用暫存 preview；重新跑正式 pipeline，確保輸出和最新 settings 一致。
                return app.pages.ditherEditor.pipelineRunner.runAsync(self.state.sourceImageData, self.state);
            })
            .then(function (imageData) {
                if (runId !== self.exportRunId) {
                    return null;
                }
                self.state.outputImageData = imageData;
                return app.core.imageExporter.exportPng(imageData, 'dither-output.png');
            })
            .then(function () {
                if (runId !== self.exportRunId) {
                    return;
                }
                self.state.status = 'exported';
                self.runFeatureHook('onAfterExport', {});
                self.render(self.state);
            })
            .catch(function (error) {
                if (runId !== self.exportRunId) {
                    return;
                }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.drawEpaper = function drawEpaper() {
        var self = this;
        if (!this.state.sourceImageData || this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return Promise.resolve();
        }
        return app.device.epaper.beginOperation('upload', 'preflight')
            .then(function (operationId) {
                app.pages.ditherEditor.targetPolicy.normalizeBeforePipeline(self.state);
                app.device.epaper.setClientStage(operationId, 'processing');
                self.state.status = 'exporting';
                self.render(self.state);
                self.runFeatureHook('onBeforeExport', {});
                return app.pages.ditherEditor.pipelineRunner.runAsync(self.state.sourceImageData, self.state)
                    .then(function (imageData) {
                        app.device.epaper.setClientStage(operationId, 'encoding');
                        var outputImageData = app.pages.ditherEditor.targetPolicy.outputImageData(imageData);
                        self.state.outputImageData = outputImageData;
                        return app.core.epdimgEncoder.encode(outputImageData);
                    })
                    .then(function (encoded) {
                        return app.device.epaper.submitUpload(operationId, encoded.payload);
                    })
                    .then(function () {
                        self.state.status = 'exported';
                        self.runFeatureHook('onAfterExport', {});
                        self.render(self.state);
                    })
                    .catch(function (error) {
                        app.device.epaper.failOperation(operationId, error);
                        throw error;
                    });
            })
            .catch(function (error) {
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.syncEpaperTarget = function syncEpaperTarget() {
        var previous = this.state.target && this.state.target.mode;
        app.pages.ditherEditor.targetPolicy.sync(this.state);
        if (previous !== this.state.target.mode) {
            this.state.uiRevision = (this.state.uiRevision || 0) + 1;
        }
        this.render(this.state);
    };

    DitherEditorController.prototype.cancelExport = function cancelExport() {
        if (this.state.status !== 'exporting') {
            return;
        }
        this.exportRunId = (this.exportRunId || 0) + 1;
        this.state.status = this.state.previewImageData ? 'preview-ready' : 'ready';
        this.render(this.state);
    };

    // 頁面卸載時清掉 timer/frame 與 worker，避免背景頁面繼續更新。
    DitherEditorController.prototype.destroy = function destroy() {
        this.previewRunId = (this.previewRunId || 0) + 1;
        this.exportRunId = (this.exportRunId || 0) + 1;
        if (app.pages.ditherEditor.ditherWorkerClient) {
            app.pages.ditherEditor.ditherWorkerClient.terminate();
        }
        clearTimeout(this.previewTimer);
        clearTimeout(this.previewTimingHideTimer);
        this.previewTimingHideTimer = null;
        if (this.livePreviewFrame) {
            cancelAnimationFrame(this.livePreviewFrame);
            this.livePreviewFrame = null;
        }
        app.pages.ditherEditor.pipelineRunner.clearStageCache(this.stageCache);
        this.state.livePreview = null;
        this.hidePreviewTimingLabel();
    };

    app.pages.ditherEditor.Controller = DitherEditorController;
})(window.DitherApp);
