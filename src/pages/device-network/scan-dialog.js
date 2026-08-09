(function (app) {
    // Wi-Fi 掃描 dialog：過濾弱訊號與 hidden 網路、依 RSSI 排序、cooldown 倒數。
    // 選用只回填 SSID 與可推斷 security，不自動送出。
    var MIN_VISIBLE_RSSI = -75;
    var DEFAULT_COOLDOWN_SECONDS = 10;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function signalLevel(rssi) {
        if (rssi >= -50) {
            return 4;
        }
        if (rssi >= -60) {
            return 3;
        }
        if (rssi >= -68) {
            return 2;
        }
        return 1;
    }

    function openScanDialog(options) {
        options = options || {};
        var networks = [];
        var cooldownId = null;
        var scanning = false;

        var countText = app.utils.dom.el('p', { className: 'device-hint', text: '' });
        var filterInput = app.utils.dom.el('input', {
            className: 'device-input',
            attrs: { type: 'text', placeholder: t('wifiScanFilterPlaceholder'), autocomplete: 'off' }
        });
        var list = app.utils.dom.el('div', { className: 'scan-list' });
        var notice = app.ui.createNotice();
        // 倒數只換 label 文字，圖示保留；按鈕寬度由 CSS 固定，文字長短不改變版面。
        var rescanLabel = app.utils.dom.el('span', { text: t('wifiScanAgain') });
        var rescanButton = app.utils.dom.el('button', {
            className: 'secondary-button button-with-icon scan-rescan-button',
            attrs: { type: 'button' },
            children: [
                app.ui.svgIcons.create('assets/icons/device/refresh.svg'),
                rescanLabel
            ]
        });
        var closeButton = app.utils.dom.el('button', {
            className: 'secondary-button',
            text: t('deviceClose'),
            attrs: { type: 'button' }
        });
        var dialog = app.utils.dom.el('section', {
            className: 'device-dialog scan-dialog',
            children: [
                app.utils.dom.el('h2', { text: t('wifiScanTitle') }),
                app.utils.dom.el('div', {
                    className: 'scan-tools',
                    children: [
                        app.utils.dom.el('div', {
                            className: 'scan-filter',
                            children: [
                                app.ui.svgIcons.create('assets/icons/device/filter.svg', {
                                    className: 'scan-filter-icon'
                                }),
                                filterInput
                            ]
                        }),
                        rescanButton
                    ]
                }),
                countText,
                list,
                notice.node,
                app.utils.dom.el('div', { className: 'modal-actions', children: [closeButton] })
            ]
        });

        function stopCooldown() {
            if (cooldownId !== null) {
                window.clearInterval(cooldownId);
                cooldownId = null;
            }
        }

        // cooldown 倒數期間 disable rescan。
        function startCooldown(seconds) {
            stopCooldown();
            var remaining = Math.max(1, Math.round(seconds));
            rescanButton.disabled = true;
            rescanLabel.textContent = t('wifiScanCooldown', { seconds: remaining });
            cooldownId = window.setInterval(function () {
                remaining -= 1;
                if (remaining <= 0) {
                    stopCooldown();
                    rescanButton.disabled = scanning;
                    rescanLabel.textContent = t('wifiScanAgain');
                    return;
                }
                rescanLabel.textContent = t('wifiScanCooldown', { seconds: remaining });
            }, 1000);
        }

        function renderList() {
            app.utils.dom.clear(list);
            var keyword = filterInput.value.trim().toLowerCase();
            var visible = networks.filter(function (network) {
                return !keyword || network.ssid.toLowerCase().indexOf(keyword) !== -1;
            });
            countText.textContent = scanning
                ? t('wifiScanScanning')
                : t('wifiScanFound', { count: visible.length });
            if (!visible.length && !scanning) {
                list.appendChild(app.utils.dom.el('div', {
                    className: 'scan-empty device-hint',
                    text: t('wifiScanEmpty')
                }));
                return;
            }
            visible.forEach(function (network) {
                var selectButton = app.utils.dom.el('button', {
                    className: 'secondary-button scan-select-button',
                    text: t('wifiScanSelect'),
                    attrs: { type: 'button' }
                });
                selectButton.addEventListener('click', function () {
                    app.ui.modal.close();
                    if (options.onSelect) {
                        // security 由掃描結果推斷：open 之外一律視為 WPA 類。
                        options.onSelect(network.ssid, network.encryption === 'open' ? 'open' : 'wpa');
                    }
                });
                list.appendChild(app.utils.dom.el('div', {
                    className: 'scan-row',
                    children: [
                        app.ui.svgIcons.create(
                            'assets/icons/device/wifi-strength-' + signalLevel(network.rssi) + '.svg',
                            { className: 'scan-signal-icon' }
                        ),
                        app.utils.dom.el('span', { className: 'scan-ssid', text: network.ssid }),
                        app.utils.dom.el('span', {
                            className: 'scan-meta device-hint',
                            // encryption 一律用英文大寫呈現，與安全性選單的英文選項一致。
                            text: network.rssi + ' dBm · ch ' + network.channel
                                + (network.encryption ? ' · ' + String(network.encryption).toUpperCase() : '')
                        }),
                        selectButton
                    ]
                }));
            });
        }

        function scan() {
            if (scanning) {
                return;
            }
            scanning = true;
            rescanButton.disabled = true;
            notice.clear();
            renderList();
            app.device.api.resources.wifiScan().then(
                function (data) {
                    scanning = false;
                    // 隱藏 hidden、空 SSID 與弱訊號結果，依 RSSI 由強到弱排序。
                    networks = (data.networks || [])
                        .filter(function (network) {
                            return network.ssid && !network.hidden && network.rssi > MIN_VISIBLE_RSSI;
                        })
                        .sort(function (left, right) {
                            return right.rssi - left.rssi;
                        });
                    renderList();
                    startCooldown(DEFAULT_COOLDOWN_SECONDS);
                },
                function (error) {
                    scanning = false;
                    renderList();
                    var retryAfter = error.data && error.data.retry_after_seconds;
                    if (error.code === 'wifi_connect_busy' || error.code === 'wifi_scan_busy') {
                        // 裝置正在連線占用 radio：顯示稍候重試，不顯示泛化錯誤。
                        notice.set(t('wifiScanBusy'), { error: true });
                    } else if (error.status === 429) {
                        notice.set(t('wifiScanRateLimited'), { error: true });
                    } else {
                        notice.set(app.device.errorText(error), { error: true });
                    }
                    startCooldown(retryAfter || DEFAULT_COOLDOWN_SECONDS);
                }
            );
        }

        filterInput.addEventListener('input', renderList);
        rescanButton.addEventListener('click', scan);
        closeButton.addEventListener('click', function () {
            app.ui.modal.close();
        });

        app.ui.modal.open(dialog, {
            initialFocus: filterInput,
            onClose: stopCooldown
        });
        scan();
    }

    app.pages.deviceNetworkScanDialog = { open: openScanDialog };
})(window.DitherApp);
