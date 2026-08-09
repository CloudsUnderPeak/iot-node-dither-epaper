(function (app) {
    // Wi-Fi 設定表單：mode picker、STA/AP 欄位、dirty/valid/busy 三態與完整 replacement payload。
    // PUT /api/wifi 不是 partial merge；不適用的分類一律由已載入 baseline 帶入。
    var PRINTABLE_PATTERN = /^[\x20-\x7E]*$/;
    var STA_PASSWORD_PATTERN = /^[\x20-\x7E]{8,63}$/;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function ipv4Valid(value) {
        var match = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(value);
        if (!match) {
            return false;
        }
        for (var index = 1; index <= 4; index += 1) {
            if (Number(match[index]) > 255) {
                return false;
            }
        }
        return true;
    }

    function labeledRow(labelText, controlNode) {
        return app.utils.dom.el('label', {
            className: 'device-form-row',
            children: [
                app.utils.dom.el('span', { className: 'device-form-label', text: labelText }),
                controlNode
            ]
        });
    }

    function textInput(attrs) {
        return app.utils.dom.el('input', {
            className: 'device-input',
            attrs: Object.assign({ type: 'text', autocomplete: 'off', spellcheck: 'false' }, attrs || {})
        });
    }

    // 靜態 IP 欄位用 placeholder 給範例值，讓格式一眼可見（不預填、不影響驗證）。
    function ipExampleInput(example) {
        return textInput({ placeholder: t('wifiIpExample', { value: example }) });
    }

    var toggleSequence = 0;

    // 只有 <label> 包住的開關本身可切換；文字是純 span 並用 aria-labelledby 關聯，
    // 避免點到整列文字就誤觸開關。
    function toggleRow(labelText) {
        toggleSequence += 1;
        var labelId = 'device-toggle-label-' + toggleSequence;
        var input = app.utils.dom.el('input', {
            attrs: { type: 'checkbox', 'aria-labelledby': labelId }
        });
        var node = app.utils.dom.el('div', {
            className: 'device-toggle-row',
            children: [
                app.utils.dom.el('span', {
                    className: 'device-form-label',
                    text: labelText,
                    attrs: { id: labelId }
                }),
                app.utils.dom.el('label', {
                    className: 'toggle-switch',
                    children: [
                        input,
                        app.utils.dom.el('span', {
                            className: 'toggle-switch-track',
                            attrs: { 'aria-hidden': 'true' }
                        })
                    ]
                })
            ]
        });
        return { node: node, input: input };
    }

    // <details> 的非 summary 子節點會被瀏覽器包進 ::details-content 單一盒子，
    // 直接對 details 設 grid gap 只會作用在 summary 之後；欄位間距需要自己的 wrapper。
    function advancedPanel(titleText, rows) {
        return app.utils.dom.el('details', {
            className: 'device-advanced',
            children: [
                app.utils.dom.el('summary', { text: titleText }),
                app.utils.dom.el('div', { className: 'device-advanced-body', children: rows })
            ]
        });
    }

    function selectInput(optionList) {
        var select = app.utils.dom.el('select', { className: 'device-input' });
        optionList.forEach(function (item) {
            select.appendChild(app.utils.dom.option(item.value, item.label));
        });
        return select;
    }

    // 共用 select 包裝：自訂 chevron 與其他表單元件的結尾 svg 對齊。
    function selectField(select) {
        return app.ui.selectField.create(select);
    }

    // 從 GET /api/wifi 的資料正規化成表單 value；configured_mode 是使用者意圖。
    function normalize(data) {
        var interfaces = data.interfaces || {};
        var sta = interfaces.sta || {};
        var staIp = sta.ip_config || {};
        var apData = interfaces.ap || {};
        var apIp = apData.ip_config || {};
        var mode = data.configured_mode || data.mode;
        return {
            mode: mode === 'sta' || mode === 'ap_sta' ? mode : 'ap',
            fallbackToAp: !!data.fallback_to_ap,
            sta: {
                ssid: sta.ssid || '',
                security: sta.security === 'open' ? 'open' : 'wpa',
                ipMode: staIp.mode === 'static' ? 'static' : 'dhcp',
                address: staIp.address || '',
                netmask: staIp.netmask || '',
                gateway: staIp.gateway || '',
                dns1: (staIp.dns && staIp.dns[0]) || '',
                dns2: (staIp.dns && staIp.dns[1]) || ''
            },
            ap: {
                ssid: apData.ssid || '',
                passwordEnabled: !!apData.password_enabled,
                ipMode: apIp.mode === 'static' ? 'static' : 'default',
                address: apIp.address || '',
                netmask: apIp.netmask || ''
            }
        };
    }

    function createWifiForm(options) {
        options = options || {};
        var baseline = null;
        var busyKey = null;

        // ---- Mode picker ----
        var modeInputs = {};
        function modeOption(mode, titleKey, descKey) {
            var input = app.utils.dom.el('input', {
                attrs: { type: 'radio', name: 'wifi-mode', value: mode }
            });
            modeInputs[mode] = input;
            input.addEventListener('change', update);
            return app.utils.dom.el('label', {
                className: 'wifi-mode-option',
                children: [
                    input,
                    app.utils.dom.el('span', { className: 'wifi-mode-title', text: t(titleKey) }),
                    app.utils.dom.el('span', { className: 'wifi-mode-desc', text: t(descKey) })
                ]
            });
        }
        var modeGrid = app.utils.dom.el('div', {
            className: 'wifi-mode-grid',
            children: [
                modeOption('ap', 'wifiModeAp', 'wifiModeApDesc'),
                modeOption('sta', 'wifiModeSta', 'wifiModeStaDesc'),
                modeOption('ap_sta', 'wifiModeApSta', 'wifiModeApStaDesc')
            ]
        });

        // ---- STA 欄位 ----
        var staSsid = textInput({ maxlength: '32' });
        var scanButton = app.utils.dom.el('button', {
            className: 'secondary-button',
            text: t('wifiChooseNetwork'),
            attrs: { type: 'button' }
        });
        var staSecurity = selectInput([
            { value: 'wpa', label: t('wifiSecurityWpa') },
            { value: 'open', label: t('wifiSecurityOpen') }
        ]);
        var staPassword = app.ui.passwordField({ attrs: { autocomplete: 'off', maxlength: '63' } });
        var fallbackToggle = toggleRow(t('wifiFallbackLabel'));
        var staIpMode = selectInput([
            { value: 'dhcp', label: t('wifiIpDhcp') },
            { value: 'static', label: t('wifiIpStatic') }
        ]);
        var staAddress = ipExampleInput('192.168.1.50');
        var staNetmask = ipExampleInput('255.255.255.0');
        var staGateway = ipExampleInput('192.168.1.1');
        var staDns1 = ipExampleInput('192.168.1.1');
        var staDns2 = ipExampleInput('1.1.1.1');
        var staStaticRows = app.utils.dom.el('div', {
            className: 'device-static-rows',
            children: [
                labeledRow(t('wifiIpAddress'), staAddress),
                labeledRow(t('wifiIpNetmask'), staNetmask),
                labeledRow(t('wifiIpGateway'), staGateway),
                labeledRow(t('wifiIpDns1'), staDns1),
                labeledRow(t('wifiIpDns2'), staDns2)
            ]
        });
        var staAdvanced = advancedPanel(t('wifiAdvancedSta'), [
            fallbackToggle.node,
            labeledRow(t('wifiIpModeLabel'), selectField(staIpMode)),
            staStaticRows
        ]);
        var staSection = app.utils.dom.el('div', {
            className: 'wifi-section',
            children: [
                app.utils.dom.el('h3', { text: t('wifiStaSectionTitle') }),
                labeledRow(t('wifiSsidLabel'), app.utils.dom.el('div', {
                    className: 'device-input-with-action',
                    children: [staSsid, scanButton]
                })),
                labeledRow(t('wifiSecurityLabel'), selectField(staSecurity)),
                labeledRow(t('wifiPasswordLabel'), staPassword.node),
                app.utils.dom.el('div', { className: 'device-rule-hint', text: t('wifiPasswordRule') }),
                staAdvanced
            ]
        });

        // ---- AP 欄位 ----
        var apSsid = textInput({ maxlength: '32' });
        var apPasswordToggle = toggleRow(t('wifiApPasswordLabel'));
        var apIpMode = selectInput([
            { value: 'default', label: t('wifiIpDefault') },
            { value: 'static', label: t('wifiIpStatic') }
        ]);
        var apAddress = ipExampleInput('192.168.4.1');
        var apNetmask = ipExampleInput('255.255.255.0');
        var apStaticRows = app.utils.dom.el('div', {
            className: 'device-static-rows',
            children: [
                labeledRow(t('wifiIpAddress'), apAddress),
                labeledRow(t('wifiIpNetmask'), apNetmask)
            ]
        });
        var apAdvanced = advancedPanel(t('wifiAdvancedAp'), [
            labeledRow(t('wifiIpModeLabel'), selectField(apIpMode)),
            apStaticRows
        ]);
        var apSection = app.utils.dom.el('div', {
            className: 'wifi-section',
            children: [
                app.utils.dom.el('h3', { text: t('wifiApSectionTitle') }),
                labeledRow(t('wifiApSsidLabel'), apSsid),
                apPasswordToggle.node,
                app.utils.dom.el('div', { className: 'device-rule-hint', text: t('wifiApPasswordHint') }),
                apAdvanced
            ]
        });

        // ---- Save dock ----
        var saveState = app.utils.dom.el('span', { className: 'device-hint', text: '' });
        var resetButton = app.utils.dom.el('button', {
            className: 'secondary-button',
            text: t('wifiRevert'),
            attrs: { type: 'button' }
        });
        var saveButton = app.utils.dom.el('button', {
            className: 'primary-button',
            text: t('wifiSave'),
            attrs: { type: 'button', disabled: 'disabled' }
        });
        var saveDock = app.utils.dom.el('div', {
            className: 'save-dock',
            children: [
                saveState,
                app.utils.dom.el('div', { className: 'device-actions', children: [resetButton, saveButton] })
            ]
        });

        var fieldset = app.utils.dom.el('fieldset', {
            className: 'device-fieldset',
            children: [
                labeledRow(t('wifiModeLabel'), modeGrid),
                staSection,
                apSection,
                saveDock
            ]
        });
        var node = app.utils.dom.el('form', {
            className: 'wifi-form',
            attrs: { novalidate: 'novalidate', autocomplete: 'off' },
            children: [fieldset]
        });
        node.addEventListener('submit', function (event) {
            event.preventDefault();
        });

        function currentMode() {
            for (var mode in modeInputs) {
                if (modeInputs[mode].checked) {
                    return mode;
                }
            }
            return baseline ? baseline.mode : 'ap';
        }

        function getValue() {
            return {
                mode: currentMode(),
                fallbackToAp: fallbackToggle.input.checked,
                sta: {
                    ssid: staSsid.value.trim(),
                    security: staSecurity.value,
                    ipMode: staIpMode.value,
                    address: staAddress.value.trim(),
                    netmask: staNetmask.value.trim(),
                    gateway: staGateway.value.trim(),
                    dns1: staDns1.value.trim(),
                    dns2: staDns2.value.trim()
                },
                ap: {
                    ssid: apSsid.value.trim(),
                    passwordEnabled: apPasswordToggle.input.checked,
                    ipMode: apIpMode.value,
                    address: apAddress.value.trim(),
                    netmask: apNetmask.value.trim()
                }
            };
        }

        function applyValue(value) {
            Object.keys(modeInputs).forEach(function (mode) {
                modeInputs[mode].checked = mode === value.mode;
            });
            fallbackToggle.input.checked = value.fallbackToAp;
            staSsid.value = value.sta.ssid;
            staSecurity.value = value.sta.security;
            staPassword.input.value = '';
            staIpMode.value = value.sta.ipMode;
            staAddress.value = value.sta.address;
            staNetmask.value = value.sta.netmask;
            staGateway.value = value.sta.gateway;
            staDns1.value = value.sta.dns1;
            staDns2.value = value.sta.dns2;
            apSsid.value = value.ap.ssid;
            apPasswordToggle.input.checked = value.ap.passwordEnabled;
            apIpMode.value = value.ap.ipMode;
            apAddress.value = value.ap.address;
            apNetmask.value = value.ap.netmask;
            update();
        }

        function applicable(mode) {
            return { sta: mode !== 'ap', ap: mode !== 'sta' };
        }

        // 目前 SSID 等於已儲存 WPA SSID 且 password 留空 → 沿用既有密碼。
        function passwordReused(value) {
            return !!baseline
                && baseline.sta.security === 'wpa'
                && baseline.sta.ssid !== ''
                && value.sta.security === 'wpa'
                && value.sta.ssid === baseline.sta.ssid
                && staPassword.input.value === '';
        }

        function staValid(value) {
            if (!value.sta.ssid || !PRINTABLE_PATTERN.test(value.sta.ssid)) {
                return false;
            }
            if (value.sta.security === 'wpa' && !passwordReused(value)
                && !STA_PASSWORD_PATTERN.test(staPassword.input.value)) {
                return false;
            }
            if (value.sta.ipMode === 'static') {
                if (!ipv4Valid(value.sta.address) || !ipv4Valid(value.sta.netmask) || !ipv4Valid(value.sta.gateway)) {
                    return false;
                }
                if (value.sta.dns1 && !ipv4Valid(value.sta.dns1)) {
                    return false;
                }
                if (value.sta.dns2 && !ipv4Valid(value.sta.dns2)) {
                    return false;
                }
            }
            return true;
        }

        function apValid(value) {
            if (!value.ap.ssid || !PRINTABLE_PATTERN.test(value.ap.ssid)) {
                return false;
            }
            if (value.ap.ipMode === 'static'
                && (!ipv4Valid(value.ap.address) || !ipv4Valid(value.ap.netmask))) {
                return false;
            }
            return true;
        }

        function valid() {
            var value = getValue();
            var scope = applicable(value.mode);
            if (scope.sta && !staValid(value)) {
                return false;
            }
            if (scope.ap && !apValid(value)) {
                return false;
            }
            return true;
        }

        // dirty 比對只使用目前 mode 適用的分類。
        function hasChanges() {
            if (!baseline) {
                return false;
            }
            var value = getValue();
            if (value.mode !== baseline.mode) {
                return true;
            }
            var scope = applicable(value.mode);
            if (value.mode === 'sta' && value.fallbackToAp !== baseline.fallbackToAp) {
                return true;
            }
            if (scope.sta) {
                if (staPassword.input.value !== '') {
                    return true;
                }
                var staKeys = ['ssid', 'security', 'ipMode', 'address', 'netmask', 'gateway', 'dns1', 'dns2'];
                for (var staIndex = 0; staIndex < staKeys.length; staIndex += 1) {
                    if (value.sta[staKeys[staIndex]] !== baseline.sta[staKeys[staIndex]]) {
                        return true;
                    }
                }
            }
            if (scope.ap && hasApChanges()) {
                return true;
            }
            return false;
        }

        function hasApChanges() {
            if (!baseline) {
                return false;
            }
            var value = getValue();
            var apKeys = ['ssid', 'passwordEnabled', 'ipMode', 'address', 'netmask'];
            for (var index = 0; index < apKeys.length; index += 1) {
                if (value.ap[apKeys[index]] !== baseline.ap[apKeys[index]]) {
                    return true;
                }
            }
            return false;
        }

        function staPayloadFrom(staValue, includeTypedPassword) {
            var payload = {
                ssid: staValue.ssid,
                security: staValue.security,
                ip_config: staValue.ipMode === 'static'
                    ? {
                        mode: 'static',
                        address: staValue.address,
                        netmask: staValue.netmask,
                        gateway: staValue.gateway,
                        dns: [staValue.dns1, staValue.dns2].filter(function (item) {
                            return item !== '';
                        })
                    }
                    : { mode: 'dhcp', address: '', gateway: '', netmask: '', dns: [] }
            };
            // password 欄位不存在時 firmware 保留既有 STA password。
            if (includeTypedPassword && staValue.security === 'wpa' && staPassword.input.value !== '') {
                payload.password = staPassword.input.value;
            }
            return payload;
        }

        function apPayloadFrom(apValue) {
            return {
                ssid: apValue.ssid,
                password_enabled: apValue.passwordEnabled,
                ip_config: apValue.ipMode === 'static'
                    ? { mode: 'static', address: apValue.address, netmask: apValue.netmask }
                    : { mode: 'default' }
            };
        }

        // 完整 replacement payload：不適用分類從 baseline 帶入，不送隱藏 draft。
        function buildPayload() {
            var value = getValue();
            var scope = applicable(value.mode);
            return {
                mode: value.mode,
                fallback_to_ap: value.mode === 'sta' ? value.fallbackToAp : baseline.fallbackToAp,
                interfaces: {
                    sta: scope.sta ? staPayloadFrom(value.sta, true) : staPayloadFrom(baseline.sta, false),
                    ap: scope.ap ? apPayloadFrom(value.ap) : apPayloadFrom(baseline.ap)
                }
            };
        }

        function update() {
            var value = getValue();
            var scope = applicable(value.mode);
            staSection.hidden = !scope.sta;
            apSection.hidden = !scope.ap;
            fallbackToggle.node.hidden = value.mode !== 'sta';
            staStaticRows.hidden = value.sta.ipMode !== 'static';
            apStaticRows.hidden = value.ap.ipMode !== 'static';
            // 標出裝置目前已儲存的 mode。
            Object.keys(modeInputs).forEach(function (mode) {
                modeInputs[mode].parentNode.classList.toggle('is-current', !!baseline && baseline.mode === mode);
            });
            staPassword.input.placeholder = passwordReused(value) ? t('wifiPasswordKeepPlaceholder') : '';
            staPassword.input.disabled = value.sta.security !== 'wpa';
            staPassword.toggle.disabled = value.sta.security !== 'wpa';
            var dirty = hasChanges();
            var isValid = valid();
            saveButton.disabled = !!busyKey || !dirty || !isValid;
            resetButton.disabled = !!busyKey || !dirty;
            saveState.textContent = busyKey
                ? t(busyKey)
                : !dirty ? t('wifiNoChanges') : isValid ? t('wifiReadyToSave') : t('wifiFixInvalid');
        }

        [staSsid, staAddress, staNetmask, staGateway, staDns1, staDns2, apSsid, apAddress, apNetmask, staPassword.input]
            .forEach(function (input) {
                input.addEventListener('input', update);
            });
        [staSecurity, staIpMode, apIpMode].forEach(function (select) {
            select.addEventListener('change', update);
        });
        fallbackToggle.input.addEventListener('change', update);
        apPasswordToggle.input.addEventListener('change', update);
        scanButton.addEventListener('click', function () {
            if (options.onScanRequest) {
                options.onScanRequest(fillScanResult);
            }
        });
        resetButton.addEventListener('click', function () {
            if (baseline) {
                applyValue(baseline);
            }
        });
        saveButton.addEventListener('click', function () {
            if (options.onSave) {
                options.onSave();
            }
        });

        // 掃描選用只回填 SSID 與可推斷 security，不自動送出。
        function fillScanResult(ssid, security) {
            staSsid.value = ssid;
            staSecurity.value = security;
            staPassword.input.value = '';
            update();
        }

        // baseline 載入前先同步一次可見性與儲存列狀態。
        update();

        return {
            node: node,
            // force 用於初始載入與儲存成功後；背景輪詢用 if-clean（有變更就不覆蓋草稿）。
            setBaseline: function setBaseline(data, force) {
                var next = normalize(data);
                var shouldApply = force || !baseline || !hasChanges();
                baseline = next;
                if (shouldApply) {
                    applyValue(next);
                } else {
                    update();
                }
            },
            hasChanges: hasChanges,
            hasApChanges: hasApChanges,
            passwordEnabledChanged: function passwordEnabledChanged() {
                return !!baseline && getValue().ap.passwordEnabled !== baseline.ap.passwordEnabled;
            },
            buildPayload: buildPayload,
            // 儲存成功：目前表單值成為新 baseline，並清空 password input。
            acceptSaved: function acceptSaved() {
                staPassword.input.value = '';
                baseline = getValue();
                update();
            },
            setBusy: function setBusy(key) {
                busyKey = key || null;
                update();
            },
            valid: valid
        };
    }

    app.pages.deviceNetworkWifiForm = { create: createWifiForm };
})(window.DitherApp);
