(function (app) {
    // 裝置管理的認證流程：login dialog、session 驗證、登出與全域 401 處理。
    // token 只是本地快取；有效性永遠以裝置的 session API 為準。
    var listeners = [];

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function notify() {
        listeners.slice().forEach(function (listener) {
            listener();
        });
    }

    // 任何帶 token 的 request 收到 401：token 已被裝置作廢（重啟或他人登入）。
    app.device.api.onUnauthorized(function () {
        if (app.device.api.hasToken()) {
            app.device.api.setToken('');
            notify();
        }
    });

    function ensureSession() {
        if (!app.device.api.hasToken()) {
            return Promise.resolve(false);
        }
        return app.device.api.resources.session().then(
            function () {
                return true;
            },
            function () {
                // 401 已由全域攔截清除 token；transport 失敗保留 token，
                // 待裝置恢復後由頁面重新驗證，避免離線時誤登出。
                return false;
            }
        );
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

    // 開啟共用 login dialog；成功後原地呼叫 onSuccess，不跳頁。
    function openLoginDialog(options) {
        options = options || {};
        var usernameInput = app.utils.dom.el('input', {
            className: 'device-input',
            attrs: { type: 'text', value: 'admin', readonly: 'readonly', autocomplete: 'off' }
        });
        var password = app.ui.passwordField({ attrs: { autocomplete: 'off' } });
        var notice = app.ui.createNotice();
        var submitButton = app.utils.dom.el('button', {
            className: 'primary-button',
            text: t('loginSubmit'),
            attrs: { type: 'submit' }
        });
        var cancelButton = app.utils.dom.el('button', {
            className: 'secondary-button',
            text: t('loginCancel'),
            attrs: { type: 'button' }
        });
        var form = app.utils.dom.el('form', {
            className: 'device-dialog-form',
            attrs: { novalidate: 'novalidate', autocomplete: 'off' },
            children: [
                labeledRow(t('loginUsername'), usernameInput),
                labeledRow(t('loginPassword'), password.node),
                notice.node,
                app.utils.dom.el('div', { className: 'modal-actions', children: [submitButton, cancelButton] })
            ]
        });
        var dialog = app.utils.dom.el('section', {
            className: 'device-dialog',
            children: [
                app.utils.dom.el('h2', { text: t('loginTitle') }),
                form
            ]
        });

        // username 固定 admin，仍以公開 auth API 為準，不寫死在 client。
        app.device.api.resources.authInfo().then(
            function (data) {
                if (data.username) {
                    usernameInput.value = data.username;
                }
            },
            function () {}
        );

        cancelButton.addEventListener('click', function () {
            app.ui.modal.close();
        });
        form.addEventListener('submit', function (event) {
            event.preventDefault();
            if (!password.input.value) {
                notice.set(t('loginInvalid'), { error: true });
                password.input.focus();
                return;
            }
            submitButton.disabled = true;
            cancelButton.disabled = true;
            app.ui.modal.setDismissible(false);
            app.device.api.resources.login(usernameInput.value, password.input.value).then(
                function (data) {
                    app.device.api.setToken(data.token || '');
                    app.ui.modal.close();
                    notify();
                    if (options.onSuccess) {
                        options.onSuccess();
                    }
                },
                function (error) {
                    submitButton.disabled = false;
                    cancelButton.disabled = false;
                    app.ui.modal.setDismissible(true);
                    var message = error.status === 401 || error.code === 'unauthorized'
                        ? t('loginInvalid')
                        : app.device.errorText(error);
                    notice.set(message, { error: true });
                    password.input.focus();
                }
            );
        });

        app.ui.modal.open(dialog, { initialFocus: password.input });
    }

    // 受保護區塊的鎖定卡：未登入時取代內容，點擊解鎖開啟 login dialog。
    function createLockedCard(options) {
        options = options || {};
        var unlockButton = app.utils.dom.el('button', {
            className: 'primary-button',
            text: t('deviceUnlock'),
            attrs: { type: 'button' }
        });
        unlockButton.addEventListener('click', function () {
            openLoginDialog({ onSuccess: options.onUnlocked });
        });
        return app.utils.dom.el('div', {
            className: 'device-locked',
            children: [
                app.ui.svgIcons.create('assets/icons/device/lock.svg', { className: 'device-locked-icon' }),
                app.utils.dom.el('p', { text: t(options.textKey || 'deviceLockedText') }),
                unlockButton
            ]
        });
    }

    function logout() {
        var cleanup = function () {
            app.device.api.setToken('');
            notify();
        };
        // token 已失效時本地清理仍要執行。
        return app.device.api.resources.logout().then(cleanup, cleanup);
    }

    app.device.auth = {
        hasToken: function hasToken() {
            return app.device.api.hasToken();
        },
        ensureSession: ensureSession,
        openLoginDialog: openLoginDialog,
        createLockedCard: createLockedCard,
        logout: logout,
        // 修改密碼成功、AP 密碼開關重啟等流程主動作廢本地 session。
        invalidateSession: function invalidateSession() {
            app.device.api.setToken('');
            notify();
        },
        subscribe: function subscribe(listener) {
            listeners.push(listener);
            return function unsubscribe() {
                var index = listeners.indexOf(listener);
                if (index !== -1) {
                    listeners.splice(index, 1);
                }
            };
        }
    };
})(window.DitherApp);
