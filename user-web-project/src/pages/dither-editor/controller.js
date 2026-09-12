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
        this.disposed = false;
        this.loadGeneration = 0;
        var client = app.pages.ditherEditor.ditherWorkerClient;
        this.workerClient = client && client.create();
        this.jobs = createJobOwner(this.workerClient);
        this.stageCache = options.stageCache || app.pages.ditherEditor.pipelineRunner.createStageCache();
    }

    // One active computation and one replaceable preview, local to this mount.
    function createJobOwner(workerClient) {
        var generation = 0;
        var disposed = false;
        var active = null;
        var pending = null;
        function cancelled(reason) {
            var error = new Error(reason);
            error.code = 'job_cancelled';
            error.reason = reason;
            return error;
        }
        function invalidate(reason, terminate) {
            generation += 1;
            pending = null;
            if (terminate && workerClient) { workerClient.terminate(reason); }
            if (terminate) { active = null; }
        }
        function start(kind, task) {
            var version = generation;
            var job = {
                kind: kind,
                check: function () {
                    if (disposed || version !== generation) { throw cancelled(disposed ? 'disposed' : 'superseded'); }
                }
            };
            active = job;
            var work;
            try { job.check(); work = task(job); }
            catch (error) { work = Promise.reject(error); }
            job.promise = Promise.resolve(work)
                .catch(function (error) { if (error.code !== 'job_cancelled') { throw error; } })
                .finally(function () {
                    if (active !== job) { return; }
                    active = null;
                    var next = pending;
                    pending = null;
                    if (next && !disposed) { return start('preview', next); }
                });
            return job.promise;
        }
        return {
            preview: function (task) {
                if (disposed) { return Promise.resolve(); }
                if (active && active.kind !== 'preview') { pending = task; return active.promise; }
                generation += 1;
                if (active) { pending = task; return active.promise; }
                return start('preview', task);
            },
            heavy: function (task) {
                if (disposed || (active && active.kind !== 'preview')) { return Promise.resolve(); }
                invalidate('superseded', true);
                return start('heavy', task);
            },
            supersedePreview: function () {
                if (!active || active.kind === 'preview') { generation += 1; }
            },
            cancel: function (reason) { invalidate(reason || 'explicit', true); },
            dispose: function () { disposed = true; invalidate('disposed', true); },
            isHeavy: function () { return Boolean(active && active.kind !== 'preview'); },
            counts: function () { return { active: active ? 1 : 0, pending: pending ? 1 : 0 }; }
        };
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
        'demo-data-missing': 'errorDemoDataMissing',
        'file-too-large': 'errorProjectFileTooLarge',
        'not-project': 'errorNotProjectFile',
        'png-corrupt': 'errorProjectCorrupt',
        'project-data-missing': 'errorProjectDataMissing',
        'project-data-invalid': 'errorProjectCorrupt',
        'schema-unsupported': 'errorProjectVersion',
        'source-invalid': 'errorProjectSource',
        'feature-unsupported': 'errorProjectFeature',
        'settings-invalid': 'errorProjectSettings'
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
        if (this.disposed) { return; }
        var candidate = app.pages.ditherEditor.state.create();
        candidate.fileName = fileName || 'Untitled';
        candidate.sourceFile = result.sourceFile || null;
        candidate.sourceImageData = result.imageData;
        candidate.livePreview = null;
        candidate.originalSize = result.originalSize;
        candidate.workingSize = result.workingSize;
        app.pages.ditherEditor.featureRegistry.dispatch('onImageLoaded', { state: candidate, result: result }, {});
        app.pages.ditherEditor.targetPolicy.sync(candidate);
        candidate.status = 'ready';
        app.pages.ditherEditor.editorModeStateMachine.enterPrepare(candidate);
        candidate.uiRevision = (this.state.uiRevision || 0) + 1;
        app.pages.ditherEditor.pipelineRunner.clearStageCache(this.stageCache);
        this.state = candidate;
        // 新圖載入後若有 prepare 入口就直接跳到 prepare；否則進入 edit 並排正式 preview。
        if (this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            this.commitPrepareChanges();
            this.schedulePreview();
            return;
        }
        this.render(this.state);
    };

    // 建立預設尺寸白底圖；目前 UI 不暴露此入口，保留給後續 blank-canvas flow。
    DitherEditorController.prototype.newImage = function newImage() {
        this.beginSourceLoad();
        var size = app.pages.ditherEditor.constants.DEFAULT_NEW_IMAGE_SIZE;
        this.loadResult(app.core.imageLoader.createBlankImage(size.width, size.height), 'Untitled');
    };

    DitherEditorController.prototype.beginSourceLoad = function () {
        this.loadGeneration += 1;
        this.jobs.cancel('superseded');
        this.previewRunId = (this.previewRunId || 0) + 1;
        clearTimeout(this.previewTimer);
        this.previewTimer = null;
        this.previewPending = false;
        this.hidePreviewTimingLabel();
        return this.loadGeneration;
    };

    DitherEditorController.prototype.isCurrentLoad = function (generation) {
        return !this.disposed && generation === this.loadGeneration;
    };

    DitherEditorController.prototype.loadSource = function (decode) {
        var self = this;
        if (this.disposed) { return Promise.resolve(); }
        var generation = this.beginSourceLoad();
        this.state.status = 'loading-image';
        this.state.previewRenderDurationMs = null;
        this.render(this.state);
        return Promise.resolve().then(function () { return decode(generation); })
            .catch(function (error) {
                if (!self.isCurrentLoad(generation) || error.code === 'job_cancelled') { return; }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.loadDemo = function loadDemo() {
        var self = this;
        return this.loadSource(function (generation) {
            return app.core.imageLoader.loadDemoImage(app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE)
                .then(function (result) {
                    if (self.isCurrentLoad(generation)) { self.loadResult(result, result.fileName || 'Demo image'); }
                });
        });
    };

    DitherEditorController.prototype.loadFile = function loadFile(file) {
        var self = this;
        return this.loadSource(function (generation) {
            return app.core.projectFile.classify(file).then(function (route) {
                if (!self.isCurrentLoad(generation)) { return; }
                if (route.kind === 'project') { return self.restoreProjectRoute(route, generation); }
                return app.core.imageLoader.loadImageFromFile(file, app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE)
                    .then(function (result) {
                        if (self.isCurrentLoad(generation)) { self.loadResult(result, file.name); }
                    });
            });
        });
    };

    DitherEditorController.prototype.loadProjectRoute = function loadProjectRoute(route) {
        var self = this;
        return this.loadSource(function (generation) { return self.restoreProjectRoute(route, generation); });
    };

    DitherEditorController.prototype.restoreProjectRoute = function (route, generation) {
        var self = this;
        var project = app.core.projectFile.read(route);
        return app.core.imageLoader.loadWorkingImage(project.workingBlob, app.pages.ditherEditor.constants.MAX_INPUT_LONG_EDGE)
            .then(function (workingResult) {
                if (!self.isCurrentLoad(generation)) { return; }
                return self.jobs.heavy(function (job) {
                    var candidate = app.pages.ditherEditor.projectWorkspace.restore(project, workingResult);
                    var candidateCache = app.pages.ditherEditor.pipelineRunner.createStageCache();
                    var options = { stageCache: candidateCache, job: job, workerClient: self.workerClient };
                    app.pages.ditherEditor.targetPolicy.sync(candidate);
                    candidate.preparedImageData = app.pages.ditherEditor.pipelineRunner.runPanelGroup(
                        candidate.sourceImageData, candidate, app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE, options);
                    return app.pages.ditherEditor.pipelineRunner.runAsync(candidate.sourceImageData, candidate, options)
                        .then(function (resultImageData) {
                            job.check();
                            if (!self.isCurrentLoad(generation)) { return; }
                            candidate.previewImageData = resultImageData;
                            candidate.outputImageData = resultImageData;
                            candidate.status = candidate.mode === app.pages.ditherEditor.editorModeStateMachine.groups.PREPARE
                                ? 'ready' : 'preview-ready';
                            candidate.uiRevision = (self.state.uiRevision || 0) + 1;
                            app.pages.ditherEditor.editorModeStateMachine.normalize(candidate);
                            app.pages.ditherEditor.pipelineRunner.clearStageCache(self.stageCache);
                            self.state = candidate;
                            self.render(self.state);
                        }).finally(function () { app.pages.ditherEditor.pipelineRunner.clearStageCache(candidateCache); });
                });
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
        if (this.jobs.isHeavy() || this.disposed) { return; }
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
        if (this.disposed || this.jobs.isHeavy()) { return this.state.preparedImageData; }
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
        this.jobs.supersedePreview();
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
        if (this.disposed) { return; }
        this.jobs.supersedePreview();
        if (this.jobs.isHeavy()) {
            this.jobs.preview(function (job) { return self.computePreview(job); });
            return;
        }
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
        return this.jobs.preview(function (job) { return self.computePreview(job); });
    };

    DitherEditorController.prototype.computePreview = function computePreview(job) {
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
        var snapshot = app.pages.ditherEditor.projectWorkspace.snapshot(this.state);
        return Promise.resolve()
            .then(function () {
                self.runFeatureHook('onBeforePreview', {});
                // Preview 永遠從 sourceImageData 跑完整 pipeline，避免連續套用造成畫質累積劣化。
                return app.pages.ditherEditor.pipelineRunner.runAsync(
                    snapshot.sourceImageData,
                    snapshot,
                    { stageCache: self.stageCache, job: job, workerClient: self.workerClient }
                );
            })
            .then(function (imageData) {
                if (runId !== self.previewRunId || self.disposed) { return; }
                job.check();
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
                if (error.code === 'job_cancelled') { return; }
                if (runId !== self.previewRunId || self.disposed) { return; }
                job.check();
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
        return this.jobs.heavy(function (job) { return self.computeExportPng(job); });
    };

    DitherEditorController.prototype.computeExportPng = function computeExportPng(job) {
        var self = this;
        if (!this.state.sourceImageData || this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return Promise.resolve();
        }
        if (this.state.status === 'exporting') {
            return Promise.resolve();
        }
        this.exportRunId = (this.exportRunId || 0) + 1;
        var runId = this.exportRunId;
        this.runFeatureHook('onBeforeExport', {});
        var snapshot = app.pages.ditherEditor.projectWorkspace.snapshot(this.state);
        this.state.status = 'exporting';
        this.render(this.state);
        return Promise.resolve()
            .then(function () {
                job.check();
                // Export 不使用暫存 preview；重新跑正式 pipeline，確保輸出和最新 settings 一致。
                return app.pages.ditherEditor.pipelineRunner.runAsync(snapshot.sourceImageData, snapshot, { job: job, workerClient: self.workerClient });
            })
            .then(function (imageData) {
                job.check();
                if (runId !== self.exportRunId) {
                    return null;
                }
                self.state.outputImageData = imageData;
                return app.core.imageExporter.exportPng(imageData, 'dither-output.png');
            })
            .then(function () {
                job.check();
                if (runId !== self.exportRunId) {
                    return;
                }
                self.state.status = 'exported';
                self.runFeatureHook('onAfterExport', {});
                self.render(self.state);
            })
            .catch(function (error) {
                job.check();
                if (runId !== self.exportRunId) {
                    return;
                }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.exportProject = function exportProject() {
        var self = this;
        return this.jobs.heavy(function (job) { return self.computeExportProject(job); });
    };

    DitherEditorController.prototype.computeExportProject = function computeExportProject(job) {
        var self = this;
        if (!this.state.sourceImageData
            || !app.pages.ditherEditor.targetPolicy.isEpaper(this.state)
            || this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return Promise.resolve();
        }
        if (this.state.status === 'exporting-project') {
            return Promise.resolve();
        }
        this.projectExportRunId = (this.projectExportRunId || 0) + 1;
        var runId = this.projectExportRunId;
        this.runFeatureHook('onBeforeExport', {});
        var snapshot = app.pages.ditherEditor.projectWorkspace.snapshot(this.state);
        this.state.status = 'exporting-project';
        this.render(this.state);
        return Promise.resolve()
            .then(function () {
                return app.pages.ditherEditor.pipelineRunner.runAsync(snapshot.sourceImageData, snapshot, { job: job, workerClient: self.workerClient });
            })
            .then(function (resultImageData) {
                job.check();
                if (runId !== self.projectExportRunId) {
                    return null;
                }
                var manifest = app.pages.ditherEditor.projectWorkspace.createManifest(snapshot, resultImageData);
                return Promise.all([
                    app.core.canvasUtils.imageDataToBlob(resultImageData),
                    app.core.canvasUtils.imageDataToBlob(snapshot.sourceImageData)
                ]).then(function (blobs) {
                    job.check();
                    return app.core.projectFile.create(
                        blobs[0],
                        snapshot.sourceFile.blob,
                        blobs[1],
                        manifest
                    );
                });
            })
            .then(function (blob) {
                job.check();
                if (!blob || runId !== self.projectExportRunId) {
                    return;
                }
                app.core.imageExporter.downloadBlob(
                    blob,
                    app.pages.ditherEditor.projectWorkspace.exportFileName(snapshot.fileName)
                );
                self.state.status = 'project-exported';
                self.render(self.state);
            })
            .catch(function (error) {
                job.check();
                if (runId !== self.projectExportRunId) {
                    return;
                }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.drawEpaper = function drawEpaper() {
        var self = this;
        return this.jobs.heavy(function (job) { return self.computeDrawEpaper(job); });
    };

    DitherEditorController.prototype.computeDrawEpaper = function computeDrawEpaper(job) {
        var self = this;
        if (!this.state.sourceImageData || this.state.mode !== app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            return Promise.resolve();
        }
        app.pages.ditherEditor.targetPolicy.normalizeBeforePipeline(this.state);
        this.runFeatureHook('onBeforeExport', {});
        var snapshot = app.pages.ditherEditor.projectWorkspace.snapshot(this.state);
        var outputPalette = app.pages.ditherEditor.targetPolicy.displayColors();
        return app.device.epaper.beginOperation('upload', 'preflight')
            .then(function (operationId) {
                try { job.check(); } catch (error) {
                    app.device.epaper.failOperation(operationId, error);
                    throw error;
                }
                var submitted = false;
                app.device.epaper.setClientStage(operationId, 'processing');
                self.state.status = 'exporting';
                self.render(self.state);
                return app.pages.ditherEditor.pipelineRunner.runAsync(snapshot.sourceImageData, snapshot, { job: job, workerClient: self.workerClient })
                    .then(function (imageData) {
                        job.check();
                        app.device.epaper.setClientStage(operationId, 'encoding');
                        var outputImageData = app.pages.ditherEditor.targetPolicy.outputImageData(imageData, outputPalette);
                        self.state.outputImageData = outputImageData;
                        return app.core.epdimgEncoder.encode(outputImageData);
                    })
                    .then(function (encoded) {
                        job.check();
                        submitted = true;
                        return app.device.epaper.submitUpload(operationId, encoded.payload);
                    })
                    .then(function () {
                        job.check();
                        self.state.status = 'exported';
                        self.runFeatureHook('onAfterExport', {});
                        self.render(self.state);
                    })
                    .catch(function (error) {
                        if (!submitted || error.code !== 'job_cancelled') { app.device.epaper.failOperation(operationId, error); }
                        throw error;
                    });
            })
            .catch(function (error) {
                if (error.code === 'job_cancelled' || self.disposed) { return; }
                self.state.status = 'error';
                self.state.error = errorText(error);
                self.render(self.state);
            });
    };

    DitherEditorController.prototype.syncEpaperCalibration = function syncEpaperCalibration(options) {
        options = options || {};
        var previousTarget = this.state.target || {};
        var previousMode = previousTarget.mode;
        var previousRevision = previousTarget.calibrationRevision;
        var changed = app.pages.ditherEditor.targetPolicy.sync(this.state);
        var currentTarget = this.state.target || {};
        var targetChanged = previousMode !== currentTarget.mode;
        var calibrationChanged = previousRevision !== currentTarget.calibrationRevision;
        if (!changed && !targetChanged && !calibrationChanged) {
            if (!options.defer) {
                this.render(this.state);
            }
            return false;
        }

        clearTimeout(this.previewTimer);
        this.previewTimer = null;
        this.previewRunId = (this.previewRunId || 0) + 1;
        this.state.livePreview = null;
        this.state.outputImageData = null;
        this.state.uiRevision = (this.state.uiRevision || 0) + 1;
        app.pages.ditherEditor.pipelineRunner.clearStageCache(this.stageCache);
        if (options.defer) {
            return true;
        }
        if (this.state.sourceImageData
            && this.state.mode === app.pages.ditherEditor.editorModeStateMachine.groups.EDIT) {
            this.schedulePreview();
        } else {
            this.render(this.state);
        }
        return true;
    };

    DitherEditorController.prototype.syncEpaperTarget = function syncEpaperTarget() {
        return this.syncEpaperCalibration();
    };

    DitherEditorController.prototype.cancelExport = function cancelExport() {
        if (this.state.status !== 'exporting') {
            return;
        }
        this.jobs.cancel('explicit');
        this.exportRunId = (this.exportRunId || 0) + 1;
        this.state.status = this.state.previewImageData ? 'preview-ready' : 'ready';
        this.render(this.state);
    };

    // 頁面卸載時清掉 timer/frame 與 worker，避免背景頁面繼續更新。
    DitherEditorController.prototype.destroy = function destroy() {
        this.disposed = true;
        this.loadGeneration += 1;
        this.jobs.dispose();
        this.previewRunId = (this.previewRunId || 0) + 1;
        this.exportRunId = (this.exportRunId || 0) + 1;
        this.projectExportRunId = (this.projectExportRunId || 0) + 1;
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

    app.pages.ditherEditor.createJobOwner = createJobOwner;
    app.pages.ditherEditor.Controller = DitherEditorController;
})(window.DitherApp);
