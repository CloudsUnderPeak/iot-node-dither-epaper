(function (app) {
    // E-paper runtime owner：capability discovery、cached status、single operation、progress 與 cooldown。
    // Editor/test page 只透過此 service 操作，不各自建立 polling 或 admission state machine。
    var STATUS_SYNC_MS = 5000;
    var ACTIVE_POLL_MS = 750;
    var PROGRESS_TICK_MS = 100;
    var COOLDOWN_TICK_MS = 250;
    var COOLDOWN_READY_SYNC_MS = 500;
    var COMPLETE_HOLD_MS = 700;

    var PHASES = {
        preflight: { start: 0, end: 2, durationMs: 500, messageKey: 'epaperProgressPreflight' },
        processing: { start: 2, end: 8, durationMs: 1500, messageKey: 'epaperProgressProcessing' },
        encoding: { start: 8, end: 11, durationMs: 800, messageKey: 'epaperProgressEncoding' },
        uploading: { start: 11, end: 17, durationMs: 1500, messageKey: 'epaperProgressUploading' },
        queued: { start: 17, end: 20, durationMs: 800, messageKey: 'epaperProgressQueued' },
        prewake: { start: 17, end: 20, durationMs: 800, messageKey: 'epaperProgressPrewake' },
        initializing: { start: 20, end: 27, durationMs: 1800, messageKey: 'epaperProgressInitializing' },
        transferring: { start: 27, end: 42, durationMs: 4000, messageKey: 'epaperProgressTransferring' },
        refreshing: { start: 42, end: 90, durationMs: 17000, messageKey: 'epaperProgressRefreshing' },
        powering_off: { start: 90, end: 94, durationMs: 1000, messageKey: 'epaperProgressPoweringOff' },
        sleeping: { start: 94, end: 97, durationMs: 800, messageKey: 'epaperProgressSleeping' },
        quiescing: { start: 97, end: 99, durationMs: 500, messageKey: 'epaperProgressQuiescing' },
        confirming: { start: 97, end: 99, durationMs: 500, messageKey: 'epaperProgressConfirming' },
        success: { start: 100, end: 100, durationMs: 1, messageKey: 'epaperProgressComplete' }
    };

    var listeners = [];
    var started = false;
    var liveUnsubscribe = null;
    var discoveryRequest = null;
    var statusRequest = null;
    var statusTimer = null;
    var activePollTimer = null;
    var progressTimer = null;
    var cooldownTimer = null;
    var cooldownEndsAt = 0;
    var cooldownLastSyncAt = 0;
    var stageStartedAt = 0;
    var currentRunId = 0;
    var waiters = [];

    var state = {
        mode: 'standalone',
        discovery: 'unknown',
        capabilities: null,
        status: null,
        cooldownRemainingSeconds: 0,
        lastError: null,
        operation: {
            active: false,
            accepted: false,
            kind: null,
            stage: 'idle',
            phase: null,
            progressPercent: 0,
            messageKey: null,
            runId: 0
        }
    };

    function nowMs() {
        return window.performance && window.performance.now ? window.performance.now() : Date.now();
    }

    function copyOperation() {
        return Object.assign({}, state.operation);
    }

    function snapshot() {
        updateCooldownRemaining();
        return {
            mode: state.mode,
            discovery: state.discovery,
            capabilities: state.capabilities,
            status: state.status,
            cooldownRemainingSeconds: state.cooldownRemainingSeconds,
            lastError: state.lastError,
            operation: copyOperation()
        };
    }

    function notify() {
        var current = snapshot();
        listeners.slice().forEach(function (listener) {
            listener(current);
        });
    }

    function subscribe(listener) {
        listeners.push(listener);
        return function unsubscribe() {
            var index = listeners.indexOf(listener);
            if (index !== -1) {
                listeners.splice(index, 1);
            }
        };
    }

    function makeError(code, message, data) {
        var error = new Error(message || code || 'E-paper operation failed');
        error.code = code || 'epaper_error';
        error.data = data || {};
        return error;
    }

    function capabilityValid(data) {
        var panel = data && data.panel;
        var image = data && data.image;
        var capabilities = data && data.capabilities;
        var codes = panel && Array.isArray(panel.color_codes) ? panel.color_codes.slice().sort() : [];
        return Boolean(
            panel && panel.width === 800 && panel.height === 480 && panel.colors === 6
            && codes.join(',') === '0,1,2,3,5,6'
            && image && image.format === 'epdimg' && image.header_bytes === 40
            && image.frame_bytes === 192000 && image.upload_bytes === 192040
            && capabilities && capabilities.upload === true && capabilities.refresh === true
        );
    }

    function updateCooldownRemaining() {
        state.cooldownRemainingSeconds = cooldownEndsAt
            ? Math.max(0, Math.ceil((cooldownEndsAt - Date.now()) / 1000))
            : 0;
        if (cooldownEndsAt && state.cooldownRemainingSeconds === 0) {
            cooldownEndsAt = 0;
        }
    }

    function stopCooldownTimer() {
        if (cooldownTimer !== null) {
            window.clearInterval(cooldownTimer);
            cooldownTimer = null;
        }
    }

    function cooldownNeedsServerConfirmation() {
        return Boolean(state.status && state.status.state === 'cooldown');
    }

    function syncCooldownReadiness() {
        var current = Date.now();
        if (state.mode !== 'epaper' || app.device.live.state() !== 'online'
            || current - cooldownLastSyncAt < COOLDOWN_READY_SYNC_MS) {
            return;
        }
        cooldownLastSyncAt = current;
        refreshStatus().catch(function () {});
    }

    function ensureCooldownTimer() {
        if (cooldownTimer !== null) {
            return;
        }
        cooldownTimer = window.setInterval(function () {
            var previous = state.cooldownRemainingSeconds;
            updateCooldownRemaining();
            if (previous !== state.cooldownRemainingSeconds) {
                notify();
            }
            if (state.cooldownRemainingSeconds === 0) {
                if (cooldownNeedsServerConfirmation()) {
                    syncCooldownReadiness();
                } else {
                    stopCooldownTimer();
                }
            }
        }, COOLDOWN_TICK_MS);
    }

    function setCooldown(seconds) {
        seconds = Math.max(0, Number(seconds) || 0);
        cooldownEndsAt = seconds ? Date.now() + seconds * 1000 : 0;
        updateCooldownRemaining();
        if (!seconds && !cooldownNeedsServerConfirmation()) {
            stopCooldownTimer();
            return;
        }
        ensureCooldownTimer();
    }

    function setBlocking(active) {
        app.app.state.blockingOperation = active ? 'epaper' : null;
        document.body.classList.toggle('is-epaper-operation', active);
    }

    function stopProgressTimer() {
        if (progressTimer !== null) {
            window.clearInterval(progressTimer);
            progressTimer = null;
        }
    }

    function tickProgress() {
        if (!state.operation.active) {
            stopProgressTimer();
            return;
        }
        var config = PHASES[state.operation.stage];
        if (!config || state.operation.stage === 'success') {
            return;
        }
        var elapsed = Math.max(0, nowMs() - stageStartedAt);
        // 階段超過估計時間時仍以漸近曲線緩慢前進，直到 API 回報下一階段。
        // 這是 panel percentage，不代表硬體提供了真實百分比。
        var fraction = Math.min(0.995, 1 - Math.exp(-3 * elapsed / config.durationMs));
        var next = config.start + (config.end - config.start) * fraction;
        if (next > state.operation.progressPercent) {
            state.operation.progressPercent = Number(next.toFixed(1));
            notify();
        }
    }

    function setStage(stage, phase) {
        var config = PHASES[stage] || PHASES.queued;
        var nextPhase = phase || null;
        var changed = state.operation.stage !== stage || state.operation.phase !== nextPhase || !stageStartedAt;
        state.operation.stage = stage;
        state.operation.phase = nextPhase;
        state.operation.messageKey = config.messageKey;
        state.operation.progressPercent = Math.max(state.operation.progressPercent, config.start);
        // 每次 status polling 都可能回傳相同 phase；不得因此重設估時計時。
        if (changed) {
            stageStartedAt = nowMs();
        }
        if (progressTimer === null && stage !== 'success') {
            progressTimer = window.setInterval(tickProgress, PROGRESS_TICK_MS);
        }
        notify();
    }

    function settleWaiters(error, value) {
        var pending = waiters.slice();
        waiters = [];
        pending.forEach(function (waiter) {
            if (error) {
                waiter.reject(error);
            } else {
                waiter.resolve(value);
            }
        });
    }

    function waitForTerminal() {
        return new Promise(function (resolve, reject) {
            waiters.push({ resolve: resolve, reject: reject });
        });
    }

    function resetOperation() {
        state.operation = {
            active: false,
            accepted: false,
            kind: null,
            stage: 'idle',
            phase: null,
            progressPercent: 0,
            messageKey: null,
            runId: currentRunId
        };
        stageStartedAt = 0;
        stopProgressTimer();
        stopActivePoll();
        setBlocking(false);
        notify();
    }

    function completeOperation(status) {
        if (!state.operation.active) {
            return;
        }
        setStage('success');
        state.operation.progressPercent = 100;
        notify();
        settleWaiters(null, status);
        window.setTimeout(resetOperation, COMPLETE_HOLD_MS);
    }

    function failOperation(runId, error) {
        if (runId && runId !== state.operation.runId) {
            return;
        }
        error = error || makeError('epaper_error');
        state.lastError = error;
        settleWaiters(error);
        resetOperation();
    }

    function startObservedOperation(status) {
        if (state.operation.active) {
            return;
        }
        currentRunId += 1;
        state.operation = {
            active: true,
            accepted: true,
            kind: 'observed',
            stage: 'queued',
            phase: null,
            progressPercent: 17,
            messageKey: PHASES.queued.messageKey,
            runId: currentRunId
        };
        setBlocking(true);
        setStage(status.phase || (status.state === 'uploading' ? 'uploading' : 'queued'), status.phase);
        ensureActivePoll();
    }

    function asyncOperationError(status) {
        var operation = status && status.last_operation;
        var code = operation && operation.error_code && operation.error_code !== 'none'
            ? operation.error_code
            : 'epaper_error';
        return makeError(code, app.i18n.t('epaperErrorAsync', { code: code }), status);
    }

    function applyStatus(status) {
        state.status = status || null;
        if (!status) {
            notify();
            return status;
        }
        if (status.state === 'cooldown') {
            setCooldown(status.retry_after_seconds);
            if (state.operation.active) {
                if (status.last_operation && status.last_operation.result === 'failed') {
                    failOperation(state.operation.runId, asyncOperationError(status));
                } else {
                    completeOperation(status);
                }
            } else {
                notify();
            }
            return status;
        }
        if (status.state === 'uploading' || status.state === 'queued' || status.state === 'drawing') {
            if (!state.operation.active) {
                startObservedOperation(status);
            }
            var stage = status.phase || (status.state === 'uploading' ? 'uploading' : 'queued');
            setStage(stage, status.phase);
            ensureActivePoll();
            return status;
        }
        if (status.state === 'unavailable') {
            var unavailable = makeError('epaper_unavailable', app.i18n.t('epaperErrorUnavailable'), status);
            if (state.operation.active) {
                failOperation(state.operation.runId, unavailable);
            } else {
                state.lastError = unavailable;
                notify();
            }
            return status;
        }
        if (status.state === 'idle' && state.operation.active && state.operation.accepted
            && status.last_operation && status.last_operation.result === 'failed') {
            failOperation(state.operation.runId, asyncOperationError(status));
            return status;
        }
        if (status.state === 'idle') {
            setCooldown(0);
        }
        notify();
        return status;
    }

    function refreshStatus() {
        if (state.mode !== 'epaper' || app.device.live.state() !== 'online') {
            return Promise.resolve(state.status);
        }
        if (statusRequest) {
            return statusRequest;
        }
        statusRequest = app.device.api.resources.epaperStatus()
            .then(applyStatus)
            .catch(function (error) {
                // 202 後 transport failure 不解除 operation lock；reconnect 後再確認 server state。
                if (!state.operation.active) {
                    state.lastError = error;
                    notify();
                }
                throw error;
            })
            .finally(function () {
                statusRequest = null;
            });
        return statusRequest;
    }

    function stopActivePoll() {
        if (activePollTimer !== null) {
            window.clearInterval(activePollTimer);
            activePollTimer = null;
        }
    }

    function ensureActivePoll() {
        if (activePollTimer === null) {
            activePollTimer = window.setInterval(function () {
                refreshStatus().catch(function () {});
            }, ACTIVE_POLL_MS);
        }
    }

    function probe() {
        if (app.device.live.state() !== 'online') {
            return Promise.resolve(false);
        }
        if (discoveryRequest) {
            return discoveryRequest;
        }
        state.discovery = 'probing';
        notify();
        discoveryRequest = app.device.api.resources.epaperCapabilities()
            .then(function (capabilities) {
                if (!capabilityValid(capabilities)) {
                    throw makeError('epaper_unsupported', app.i18n.t('epaperErrorUnsupported'));
                }
                state.mode = 'epaper';
                state.discovery = 'supported';
                state.capabilities = capabilities;
                state.lastError = null;
                notify();
                return refreshStatus().catch(function () {
                    return null;
                }).then(function () {
                    return true;
                });
            })
            .catch(function (error) {
                if (state.mode !== 'epaper') {
                    state.mode = 'standalone';
                    state.discovery = error && error.code === 'epaper_unsupported' ? 'unsupported' : 'error';
                    state.capabilities = null;
                } else {
                    // Once a compatible panel is found, keep the editor target stable for this session.
                    state.discovery = 'supported';
                }
                state.lastError = error;
                notify();
                return false;
            })
            .finally(function () {
                discoveryRequest = null;
            });
        return discoveryRequest;
    }

    function statusAllowsOperation(status) {
        return Boolean(status && status.can_upload === true && status.can_draw === true);
    }

    function beginOperation(kind, firstStage) {
        if (state.operation.active) {
            return Promise.reject(makeError('epaper_busy', app.i18n.t('epaperErrorBusy')));
        }
        if (state.mode !== 'epaper' || app.device.live.state() !== 'online') {
            return Promise.reject(makeError('epaper_unavailable', app.i18n.t('epaperErrorUnavailable')));
        }
        return refreshStatus().then(function (status) {
            if (!statusAllowsOperation(status)) {
                var code = status && status.state === 'unavailable' ? 'epaper_unavailable' : 'epaper_busy';
                throw makeError(code, app.i18n.t(code === 'epaper_busy' ? 'epaperErrorBusy' : 'epaperErrorUnavailable'), status);
            }
            currentRunId += 1;
            state.lastError = null;
            state.operation = {
                active: true,
                accepted: false,
                kind: kind,
                stage: firstStage || 'preflight',
                phase: null,
                progressPercent: 0,
                messageKey: null,
                runId: currentRunId
            };
            setBlocking(true);
            setStage(firstStage || 'preflight');
            return currentRunId;
        });
    }

    function setClientStage(runId, stage) {
        if (!state.operation.active || state.operation.runId !== runId || state.operation.accepted) {
            return false;
        }
        setStage(stage);
        return true;
    }

    function acceptAndWait(runId, response) {
        if (!state.operation.active || state.operation.runId !== runId) {
            return Promise.reject(makeError('epaper_stale_operation'));
        }
        state.operation.accepted = true;
        setStage('queued');
        ensureActivePoll();
        var terminal = waitForTerminal();
        refreshStatus().catch(function () {});
        return terminal.then(function (status) {
            return { response: response, status: status };
        });
    }

    function submitUpload(runId, payload) {
        if (!payload || payload.byteLength !== 192040) {
            var invalid = makeError('invalid_epaper_image', app.i18n.t('epaperErrorInvalidImage'));
            failOperation(runId, invalid);
            return Promise.reject(invalid);
        }
        setClientStage(runId, 'uploading');
        return app.device.api.resources.epaperUpload(payload)
            .then(function (response) {
                return acceptAndWait(runId, response);
            })
            .catch(function (error) {
                if (!state.operation.accepted) {
                    failOperation(runId, error);
                }
                throw error;
            });
    }

    function actionRequest(name) {
        if (name === 'white') {
            return app.device.api.resources.epaperWhite();
        }
        if (name === 'palette') {
            return app.device.api.resources.epaperPalette();
        }
        if (name === 'refresh') {
            return app.device.api.resources.epaperRefresh();
        }
        return Promise.reject(makeError('epaper_unknown_action'));
    }

    function runAction(name) {
        return beginOperation(name, 'preflight').then(function (runId) {
            return actionRequest(name)
                .then(function (response) {
                    return acceptAndWait(runId, response);
                })
                .catch(function (error) {
                    if (!state.operation.accepted) {
                        failOperation(runId, error);
                    }
                    throw error;
                });
        });
    }

    function isSupported() {
        return state.mode === 'epaper' && Boolean(state.capabilities);
    }

    function canDraw() {
        return Boolean(
            isSupported()
            && app.device.live.state() === 'online'
            && !state.operation.active
            && state.cooldownRemainingSeconds === 0
            && statusAllowsOperation(state.status)
        );
    }

    function start() {
        if (started) {
            return;
        }
        started = true;
        liveUnsubscribe = app.device.live.subscribe(function (liveState) {
            if (liveState === 'online') {
                probe();
            } else {
                notify();
            }
        });
        statusTimer = window.setInterval(function () {
            if (state.mode === 'epaper' && app.device.live.state() === 'online' && !state.operation.active) {
                refreshStatus().catch(function () {});
            }
        }, STATUS_SYNC_MS);
        if (app.device.live.state() === 'online') {
            probe();
        }
    }

    app.device.epaper = {
        start: start,
        probe: probe,
        snapshot: snapshot,
        subscribe: subscribe,
        isSupported: isSupported,
        canDraw: canDraw,
        refreshStatus: refreshStatus,
        beginOperation: beginOperation,
        setClientStage: setClientStage,
        submitUpload: submitUpload,
        failOperation: failOperation,
        runAction: runAction,
        progressPhases: PHASES
    };
})(window.DitherApp);
