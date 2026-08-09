(function (app) {
    // 密碼輸入欄 + 疊在欄位右側的顯示/隱藏切換。
    // 登入、修改密碼與 Wi-Fi 密碼共用；切換顯示時不清空輸入。
    function passwordField(options) {
        options = options || {};
        var attrs = Object.assign(
            {
                type: 'password',
                autocomplete: 'off',
                spellcheck: 'false'
            },
            options.attrs || {}
        );
        var input = app.utils.dom.el('input', { className: 'device-input password-input', attrs: attrs });
        var icon = app.ui.svgIcons.create('assets/icons/device/eye.svg');
        var toggle = app.utils.dom.el('button', {
            className: 'password-toggle',
            attrs: { type: 'button', 'aria-label': app.i18n.t('passwordShow'), title: app.i18n.t('passwordShow') },
            children: [icon]
        });
        toggle.addEventListener('click', function () {
            var show = input.type === 'password';
            input.type = show ? 'text' : 'password';
            var labelKey = show ? 'passwordHide' : 'passwordShow';
            toggle.setAttribute('aria-label', app.i18n.t(labelKey));
            toggle.title = app.i18n.t(labelKey);
            var nextIcon = app.ui.svgIcons.create(
                show ? 'assets/icons/device/eye-off.svg' : 'assets/icons/device/eye.svg'
            );
            toggle.replaceChild(nextIcon, toggle.firstChild);
        });
        var node = app.utils.dom.el('div', {
            className: 'password-field',
            children: [input, toggle]
        });
        return { node: node, input: input, toggle: toggle };
    }

    app.ui.passwordField = passwordField;
})(window.DitherApp);
