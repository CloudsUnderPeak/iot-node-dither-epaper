(function (app) {
    // 網路頁：上半部是公開的連線狀態卡（10s 輪詢），
    // 下半部 Wi-Fi 設定需登入；背景輪詢用 if-clean，不覆蓋使用者草稿。
    var POLL_MS = 10000;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    // runtime state 翻成使用者語言 badge，不混用 API 原始英文值。
    function stateBadge(state) {
        var map = {
            connected: { key: 'wifiStateConnected', variant: 'ok' },
            active: { key: 'wifiStateActive', variant: 'ok' },
            connecting: { key: 'wifiStateConnecting', variant: 'warn' },
            starting: { key: 'wifiStateConnecting', variant: 'warn' },
            failed: { key: 'wifiStateFailed', variant: 'err' },
            disabled: { key: 'wifiStateDisabled', variant: 'muted' },
            inactive: { key: 'wifiStateDisabled', variant: 'muted' }
        };
        var info = map[state] || { key: null, variant: 'muted' };
        return app.utils.dom.el('span', {
            className: 'device-badge is-' + info.variant,
            text: info.key ? t(info.key) : String(state || '—')
        });
    }

    function modeLabel(mode) {
        var map = { ap: 'AP', sta: 'STA', ap_sta: 'AP + STA' };
        return map[mode] || t('wifiModeOff');
    }

    // 每欄開頭比照 builtin-web Interface status 放一顆 feature icon；
    // label／值／說明各自對齊：沒有 badge 的欄位仍保留同高的 label 列。
    // 全卡使用全站一般字體，不用 mono（等寬字與中文混排字距不一致）。
    function statusColumn(iconPath, labelText, badgeNode, valueNode, hintText) {
        var labelChildren = [app.utils.dom.el('span', { text: labelText })];
        if (badgeNode) {
            labelChildren.push(badgeNode);
        }
        return app.utils.dom.el('div', {
            className: 'network-status-column',
            children: [
                app.utils.dom.el('span', {
                    className: 'network-status-icon',
                    attrs: { 'aria-hidden': 'true' },
                    children: [app.ui.svgIcons.create(iconPath)]
                }),
                app.utils.dom.el('div', { className: 'device-field-label network-status-label', children: labelChildren }),
                valueNode,
                app.utils.dom.el('div', { className: 'device-hint network-status-hint', text: hintText || '' })
            ]
        });
    }

    app.pages.deviceNetworkPage = {
        id: 'device-network',
        titleKey: 'deviceNetworkTitle',
        mount: function mount(container) {
            var self = this;
            this.mounted = true;
            this.generation = 0;

            var latestWifi = null;
            var hostname = '';
            var form = null;

            var pageNotice = app.ui.createNotice();
            var modeBadge = app.utils.dom.el('span', { className: 'device-badge is-accent', text: '—' });
            var statusGrid = app.utils.dom.el('div', { className: 'network-status-grid' });
            var settingsHost = app.utils.dom.el('div', { className: 'device-content' });
            var section = app.utils.dom.el('section', {
                className: 'device-page',
                children: [
                    pageNotice.node,
                    app.utils.dom.el('section', {
                        className: 'panel-section device-gate',
                        children: [
                            app.utils.dom.el('div', {
                                className: 'device-card-header',
                                children: [
                                    app.utils.dom.el('h2', { text: t('wifiStatusCardTitle') }),
                                    modeBadge
                                ]
                            }),
                            app.utils.dom.el('div', {
                                className: 'panel-body device-card-body',
                                children: [statusGrid]
                            })
                        ]
                    }),
                    app.utils.dom.el('section', {
                        className: 'panel-section device-gate',
                        children: [
                            app.utils.dom.el('h2', { text: t('wifiSettingsCardTitle') }),
                            app.utils.dom.el('div', {
                                className: 'panel-body device-card-body',
                                children: [settingsHost]
                            })
                        ]
                    })
                ]
            });

            function renderStatus() {
                app.utils.dom.clear(statusGrid);
                if (!latestWifi) {
                    statusGrid.appendChild(app.utils.dom.el('div', {
                        className: 'device-hint',
                        text: t('deviceLoading')
                    }));
                    return;
                }
                var interfaces = latestWifi.interfaces || {};
                var sta = interfaces.sta || {};
                var apData = interfaces.ap || {};
                modeBadge.textContent = t('wifiModeBadge', { mode: modeLabel(latestWifi.mode) });

                var staValue = app.utils.dom.el('div', {
                    className: 'network-status-value',
                    text: sta.ssid || '—'
                });
                statusGrid.appendChild(statusColumn(
                    'assets/icons/device/wifi-strength-4.svg',
                    'STA',
                    stateBadge(sta.state),
                    staValue,
                    sta.ip && sta.ip !== '0.0.0.0' ? sta.ip : ''
                ));

                var apValue = app.utils.dom.el('div', {
                    className: 'network-status-value',
                    text: apData.ssid || '—'
                });
                statusGrid.appendChild(statusColumn(
                    'assets/icons/device/signal.svg',
                    'AP',
                    stateBadge(apData.state),
                    apValue,
                    (apData.ip || '') + ' · '
                        + t(apData.password_enabled ? 'wifiApPasswordOn' : 'wifiApPasswordOff')
                ));

                // mDNS 只有 STA 已連線時才可從 LAN 使用；沒連線時仍顯示網址但不加連結。
                var mdnsUrl = hostname ? 'http://' + hostname + '.local/' : '—';
                var staConnected = sta.state === 'connected';
                var mdnsValue = staConnected && hostname
                    ? app.utils.dom.el('a', {
                        className: 'network-status-value',
                        text: mdnsUrl,
                        attrs: { href: mdnsUrl }
                    })
                    : app.utils.dom.el('div', { className: 'network-status-value', text: mdnsUrl });
                statusGrid.appendChild(statusColumn(
                    'assets/icons/device/globe.svg',
                    t('wifiMdnsLabel'),
                    null,
                    mdnsValue,
                    t(staConnected ? 'wifiMdnsAvailable' : 'wifiMdnsNeedsSta')
                ));
            }

            function loadWifi() {
                self.generation += 1;
                var current = self.generation;
                app.device.api.resources.wifi().then(
                    function (data) {
                        if (!self.mounted || current !== self.generation) {
                            return;
                        }
                        latestWifi = data;
                        renderStatus();
                        // if-clean：表單有變更時 setBaseline 只更新 baseline，不覆蓋草稿。
                        if (form) {
                            form.setBaseline(data);
                        }
                    },
                    function () {}
                );
            }

            function loadHostname() {
                app.device.api.resources.device().then(
                    function (data) {
                        if (!self.mounted) {
                            return;
                        }
                        hostname = data.hostname || '';
                        renderStatus();
                    },
                    function () {}
                );
            }

            function renderSettings() {
                if (!self.mounted) {
                    return;
                }
                if (!app.device.auth.hasToken()) {
                    form = null;
                    app.utils.dom.clear(settingsHost);
                    settingsHost.appendChild(app.device.auth.createLockedCard({
                        textKey: 'wifiLockedText',
                        onUnlocked: renderSettings
                    }));
                    return;
                }
                if (form) {
                    return;
                }
                app.utils.dom.clear(settingsHost);
                form = app.pages.deviceNetworkWifiForm.create({
                    onSave: function () {
                        app.pages.deviceNetworkController.save({
                            form: form,
                            notice: pageNotice,
                            refresh: function () {
                                loadWifi();
                            },
                            isMounted: function () {
                                return self.mounted;
                            }
                        });
                    },
                    onScanRequest: function (fill) {
                        app.pages.deviceNetworkScanDialog.open({
                            onSelect: fill
                        });
                    }
                });
                settingsHost.appendChild(form.node);
                if (latestWifi) {
                    form.setBaseline(latestWifi, true);
                }
                // 背景驗證 session；token 已失效時退回鎖定卡。
                app.device.auth.ensureSession().then(function (valid) {
                    if (self.mounted && !valid && !app.device.auth.hasToken()) {
                        renderSettings();
                    }
                });
            }

            this.authUnsubscribe = app.device.auth.subscribe(renderSettings);
            this.gate = app.device.bindLiveGate(section, {
                onOnline: function () {
                    loadWifi();
                    loadHostname();
                }
            });
            section.insertBefore(this.gate.banner, section.children[0]);
            container.appendChild(section);
            this.timerId = window.setInterval(loadWifi, POLL_MS);
            renderStatus();
            loadWifi();
            loadHostname();
            renderSettings();
        },
        unmount: function unmount() {
            this.mounted = false;
            if (this.timerId) {
                window.clearInterval(this.timerId);
                this.timerId = null;
            }
            if (this.gate) {
                this.gate.unbind();
                this.gate = null;
            }
            if (this.authUnsubscribe) {
                this.authUnsubscribe();
                this.authUnsubscribe = null;
            }
        }
    };
})(window.DitherApp);
