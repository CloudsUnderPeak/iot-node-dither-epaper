(function (app) {
    // Wi-Fi 儲存流程：PUT /api/wifi 完整 replacement 與 202 safe transition 輪詢。
    // 1s 間隔、25s deadline 涵蓋 15s STA deadline + commit + 5s AP grace；
    // generation 序號丟棄過期回覆，transition 期間抑制 device-live 的離線判定。
    var POLL_MS = 1000;
    var DEADLINE_MS = 25000;
    var SUPPRESS_REASON = 'wifi-transition';
    var generation = 0;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    // 儲存後 runtime 重新套用設定；以遞增間隔重抓兩次讓狀態卡收斂。
    function scheduleRefresh(ctx) {
        [2000, 8000].forEach(function (delay) {
            window.setTimeout(function () {
                if (ctx.isMounted()) {
                    ctx.refresh();
                }
            }, delay);
        });
    }

    function pollTransition(current, payload, apDirty, ctx, startedAt) {
        window.setTimeout(function () {
            if (!ctx.isMounted() || current !== generation) {
                app.device.live.release(SUPPRESS_REASON);
                return;
            }
            app.device.api.resources.wifiConnectStatus().then(
                function (status) {
                    if (!ctx.isMounted() || current !== generation) {
                        app.device.live.release(SUPPRESS_REASON);
                        return;
                    }
                    if (status.state === 'connected') {
                        // terminal connected 才把送出值設為表單 baseline 並清除 password input。
                        app.device.live.release(SUPPRESS_REASON);
                        ctx.form.acceptSaved();
                        ctx.form.setBusy(null);
                        if (payload.mode === 'sta' && status.ap_shutdown_in_seconds > 0) {
                            ctx.notice.set(
                                t('wifiStaGrace', { ip: status.ip || '', seconds: status.ap_shutdown_in_seconds }),
                                { sticky: true }
                            );
                        } else if (payload.mode === 'ap_sta' && apDirty) {
                            ctx.notice.set(t('wifiApChangedReconnect'), { sticky: true });
                        } else {
                            ctx.notice.set(t('wifiConnectionReady', { ip: status.ip || '' }));
                        }
                        scheduleRefresh(ctx);
                        return;
                    }
                    if (status.state === 'failed') {
                        // 失敗已 rollback：原設定與 AP 可繼續使用；表單草稿保留。
                        app.device.live.release(SUPPRESS_REASON);
                        ctx.form.setBusy(null);
                        var failure = status.failure_code && status.failure_code !== 'none'
                            ? ' (' + status.failure_code + ')'
                            : '';
                        ctx.notice.set(t('wifiRollbackComplete') + failure, { error: true });
                        ctx.refresh();
                        return;
                    }
                    continueOrTimeout();
                },
                function () {
                    // AP/STA 交接期間的 transport 失敗屬預期；deadline 內持續重試。
                    continueOrTimeout();
                }
            );

            function continueOrTimeout() {
                if (Date.now() - startedAt >= DEADLINE_MS) {
                    app.device.live.release(SUPPRESS_REASON);
                    ctx.form.setBusy(null);
                    // 草稿保留；請使用者重連裝置 AP 或 mDNS 再確認結果。
                    ctx.notice.set(t('wifiTransitionRetryEnded'), { error: true });
                    return;
                }
                pollTransition(current, payload, apDirty, ctx, startedAt);
            }
        }, POLL_MS);
    }

    // ctx: { form, notice, refresh, isMounted }
    function save(ctx) {
        generation += 1;
        var current = generation;
        var payload = ctx.form.buildPayload();
        var apDirty = ctx.form.hasApChanges();
        var apPasswordChanged = ctx.form.passwordEnabledChanged();
        ctx.form.setBusy('wifiSaving');
        ctx.notice.clear();
        app.device.api.resources.wifiUpdate(payload).then(
            function (result) {
                if (!ctx.isMounted() || current !== generation) {
                    return;
                }
                if (result && result.state === 'connecting') {
                    // 202 safe transition：candidate 只在 RAM，輪詢 connect status 才知道結果。
                    app.device.live.suppress(SUPPRESS_REASON);
                    ctx.form.setBusy('wifiVerifying');
                    ctx.notice.set(t('wifiVerifying'), { sticky: true });
                    pollTransition(current, payload, apDirty, ctx, Date.now());
                    return;
                }
                if (apPasswordChanged) {
                    // AP 密碼保護開關變更：設定已持久化，裝置立即重啟、session 失效。
                    ctx.form.setBusy(null);
                    ctx.notice.set(t('wifiApRestarting'), { sticky: true });
                    app.device.auth.invalidateSession();
                    return;
                }
                ctx.form.acceptSaved();
                ctx.form.setBusy(null);
                ctx.notice.set(t('wifiSaved'));
                scheduleRefresh(ctx);
            },
            function (error) {
                if (!ctx.isMounted() || current !== generation) {
                    return;
                }
                ctx.form.setBusy(null);
                ctx.notice.set(app.device.errorText(error), { error: true });
            }
        );
    }

    app.pages.deviceNetworkController = { save: save };
})(window.DitherApp);
