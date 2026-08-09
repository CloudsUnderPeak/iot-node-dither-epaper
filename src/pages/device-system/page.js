(function (app) {
    // 系統設定頁：hostname 與管理員密碼，整頁需要有效 session。
    // 兩張卡各自獨立 dirty/valid/busy 與儲存回饋，互不影響。
    var HOSTNAME_PATTERN = /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,29}[A-Za-z0-9])?$/;
    // 內建網頁的管理員密碼 allowlist，比 REST API 的 printable ASCII 更嚴格。
    var PASSWORD_PATTERN = /^[A-Za-z0-9!@#$%^&*()_+=.,:?-]{8,63}$/;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
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

    // hostname 卡：規則驗證、mDNS 網址預覽與獨立儲存。
    function createHostnameCard(page) {
        var input = app.utils.dom.el('input', {
            className: 'device-input',
            attrs: { type: 'text', maxlength: '31', autocomplete: 'off', spellcheck: 'false' }
        });
        var preview = app.utils.dom.el('div', { className: 'device-hint' });
        var notice = app.ui.createNotice();
        var saveButton = app.utils.dom.el('button', {
            className: 'primary-button',
            text: t('deviceSave'),
            attrs: { type: 'button', disabled: 'disabled' }
        });
        var baseline = '';
        var busy = false;

        function updateAvailability() {
            var value = input.value.trim();
            var dirty = value !== baseline;
            var valid = HOSTNAME_PATTERN.test(value);
            preview.textContent = t('hostnamePreview', { hostname: valid ? value : baseline || '…' });
            saveButton.disabled = busy || !dirty || !valid;
        }

        input.addEventListener('input', updateAvailability);
        saveButton.addEventListener('click', function () {
            busy = true;
            updateAvailability();
            app.device.api.resources.systemUpdate({ hostname: input.value.trim() }).then(
                function (data) {
                    busy = false;
                    // 以裝置回應的 hostname 為準。
                    baseline = data.hostname || input.value.trim();
                    input.value = baseline;
                    updateAvailability();
                    // mDNS 重新套用期間連線可能短暫中斷；訊息需保留脈絡，不自動消失。
                    notice.set(t('hostnameSaved'), { sticky: true });
                },
                function (error) {
                    busy = false;
                    updateAvailability();
                    notice.set(app.device.errorText(error), { error: true });
                }
            );
        });

        var node = app.utils.dom.el('section', {
            className: 'panel-section device-gate',
            children: [
                app.utils.dom.el('h2', { text: t('hostnameCardTitle') }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [
                        app.utils.dom.el('fieldset', {
                            className: 'device-fieldset',
                            children: [
                                labeledRow(t('hostnameLabel'), input),
                                app.utils.dom.el('div', { className: 'device-rule-hint', text: t('hostnameRule') }),
                                preview,
                                notice.node,
                                app.utils.dom.el('div', { className: 'device-actions', children: [saveButton] })
                            ]
                        })
                    ]
                })
            ]
        });

        // baseline 取自公開 device API。
        app.device.api.resources.device().then(
            function (data) {
                if (!page.mounted) {
                    return;
                }
                baseline = data.hostname || '';
                if (!input.value) {
                    input.value = baseline;
                }
                updateAvailability();
            },
            function () {}
        );
        updateAvailability();
        return node;
    }

    // 管理員密碼卡：allowlist + 兩次輸入一致；成功後強制重新登入。
    function createPasswordCard(page, pageNotice) {
        var next = app.ui.passwordField({ attrs: { autocomplete: 'off' } });
        var confirm = app.ui.passwordField({ attrs: { autocomplete: 'off' } });
        var notice = app.ui.createNotice();
        var saveButton = app.utils.dom.el('button', {
            className: 'primary-button',
            text: t('passwordSubmit'),
            attrs: { type: 'button', disabled: 'disabled' }
        });
        var busy = false;
        var apPasswordEnabled = false;

        // AP 密碼保護開啟時，改密碼會讓裝置立即重啟；訊息需要區分。
        app.device.api.resources.wifi().then(
            function (data) {
                var ap = data.interfaces && data.interfaces.ap;
                apPasswordEnabled = !!(ap && ap.password_enabled);
            },
            function () {}
        );

        function updateAvailability() {
            var valid = PASSWORD_PATTERN.test(next.input.value) && next.input.value === confirm.input.value;
            saveButton.disabled = busy || !valid;
        }

        next.input.addEventListener('input', updateAvailability);
        confirm.input.addEventListener('input', updateAvailability);
        saveButton.addEventListener('click', function () {
            busy = true;
            updateAvailability();
            app.device.api.resources.changePassword(next.input.value).then(
                function () {
                    // 成功後 session 已被裝置作廢；訊息放在頁層 notice，
                    // 讓 auth 重新渲染鎖定卡後仍可見。
                    pageNotice.set(
                        t(apPasswordEnabled ? 'passwordChangedRestarting' : 'passwordChanged'),
                        { sticky: true }
                    );
                    app.device.auth.invalidateSession();
                },
                function (error) {
                    busy = false;
                    updateAvailability();
                    notice.set(app.device.errorText(error), { error: true });
                }
            );
        });

        return app.utils.dom.el('section', {
            className: 'panel-section device-gate',
            children: [
                app.utils.dom.el('h2', { text: t('passwordCardTitle') }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [
                        app.utils.dom.el('fieldset', {
                            className: 'device-fieldset',
                            children: [
                                labeledRow(t('passwordNew'), next.node),
                                labeledRow(t('passwordConfirm'), confirm.node),
                                app.utils.dom.el('div', { className: 'device-rule-hint', text: t('passwordRule') }),
                                notice.node,
                                app.utils.dom.el('div', { className: 'device-actions', children: [saveButton] })
                            ]
                        })
                    ]
                })
            ]
        });
    }

    function dangerIcon() {
        return app.utils.dom.el('span', {
            className: 'device-danger-icon',
            children: [app.ui.svgIcons.create('assets/icons/device/warning.svg')]
        });
    }

    // 送出前的二次確認：不可復原的操作一律先開 dialog，不直接綁在卡片按鈕上。
    function openResetDialog(pageNotice) {
        var confirmButton = app.utils.dom.el('button', {
            className: 'primary-button device-danger-submit',
            text: t('resetAll'),
            attrs: { type: 'button' }
        });
        var cancelButton = app.utils.dom.el('button', {
            className: 'secondary-button',
            text: t('deviceCancel'),
            attrs: { type: 'button' }
        });
        var dialog = app.utils.dom.el('section', {
            className: 'device-dialog device-danger-dialog',
            children: [
                dangerIcon(),
                app.utils.dom.el('h2', { text: t('resetConfirmTitle') }),
                app.utils.dom.el('p', { text: t('resetConfirmBody') }),
                app.utils.dom.el('div', { className: 'modal-actions', children: [confirmButton, cancelButton] })
            ]
        });

        cancelButton.addEventListener('click', function () {
            app.ui.modal.close();
        });
        confirmButton.addEventListener('click', function () {
            confirmButton.disabled = true;
            cancelButton.disabled = true;
            app.ui.modal.setDismissible(false);
            app.device.api.resources.systemReset().then(
                function () {
                    app.ui.modal.close();
                    // 裝置立刻重啟，session 一併作廢；訊息需常駐等使用者重新連線。
                    pageNotice.set(t('resetRestarting'), { sticky: true });
                    app.device.auth.invalidateSession();
                },
                function (error) {
                    app.ui.modal.close();
                    pageNotice.set(app.device.errorText(error), { error: true });
                }
            );
        });

        app.ui.modal.open(dialog, { initialFocus: cancelButton });
    }

    // Danger zone：唯一保留的完整重設，樣式與確認流程都刻意和其他卡片區隔。
    function createResetCard(pageNotice) {
        var resetButton = app.utils.dom.el('button', {
            className: 'secondary-button device-danger-button',
            text: t('resetAll'),
            attrs: { type: 'button' }
        });
        resetButton.addEventListener('click', function () {
            openResetDialog(pageNotice);
        });
        return app.utils.dom.el('section', {
            className: 'panel-section device-gate device-reset-card',
            children: [
                app.utils.dom.el('div', {
                    className: 'device-card-header device-reset-header',
                    children: [
                        dangerIcon(),
                        app.utils.dom.el('h2', { text: t('resetCardTitle') })
                    ]
                }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [
                        app.utils.dom.el('p', { className: 'device-reset-text', text: t('resetCardHint') }),
                        app.utils.dom.el('div', { className: 'device-actions', children: [resetButton] })
                    ]
                })
            ]
        });
    }

    app.pages.deviceSystemPage = {
        id: 'device-system',
        titleKey: 'deviceSystemTitle',
        mount: function mount(container) {
            var self = this;
            this.mounted = true;

            var pageNotice = app.ui.createNotice();
            var contentHost = app.utils.dom.el('div', { className: 'device-content' });
            var section = app.utils.dom.el('section', {
                className: 'device-page',
                children: [
                    pageNotice.node,
                    contentHost
                ]
            });

            function renderLocked() {
                app.utils.dom.clear(contentHost);
                contentHost.appendChild(app.utils.dom.el('section', {
                    className: 'panel-section device-gate',
                    children: [
                        app.utils.dom.el('div', {
                            className: 'panel-body device-card-body',
                            children: [app.device.auth.createLockedCard({ onUnlocked: render })]
                        })
                    ]
                }));
            }

            function render() {
                if (!self.mounted) {
                    return;
                }
                if (!app.device.auth.hasToken()) {
                    renderLocked();
                    return;
                }
                app.utils.dom.clear(contentHost);
                contentHost.appendChild(createHostnameCard(self));
                contentHost.appendChild(createPasswordCard(self, pageNotice));
                contentHost.appendChild(createResetCard(pageNotice));
                // 背景驗證 session；token 已失效時退回鎖定卡。
                app.device.auth.ensureSession().then(function (valid) {
                    if (self.mounted && !valid) {
                        renderLocked();
                    }
                });
            }

            // 401、登出或登入都重新決定鎖定狀態。
            this.authUnsubscribe = app.device.auth.subscribe(render);
            this.gate = app.device.bindLiveGate(section, { onOnline: render });
            section.insertBefore(this.gate.banner, section.children[0]);
            container.appendChild(section);
            render();
        },
        unmount: function unmount() {
            this.mounted = false;
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
