(function (app) {
    // Web Setting 管理網站層級偏好，例如 theme。
    // 設定值存在 app-state/settings-store，不放進 Dither Editor workspace。
    // 取得設定頁文字。
    function t(key) {
        return app.i18n.t(key);
    }

    function themeIcon(option) {
        var iconPath = option.id === 'dark'
            ? 'assets/icons/app/moon.svg'
            : 'assets/icons/app/sun.svg';
        return app.ui.svgIcons.create(iconPath, {
            className: 'setting-choice-icon'
        });
    }

    function themeOption(option) {
        // Radio 選項直接呼叫 app.app.setTheme，讓 DOM theme 與 localStorage 同步更新。
        var input = app.utils.dom.el('input', {
            attrs: {
                type: 'radio',
                name: 'web-theme',
                value: option.id
            }
        });
        input.checked = app.app.state.theme === option.id;
        input.addEventListener('change', function () {
            if (input.checked) {
                app.app.setTheme(option.id);
            }
        });

        return app.utils.dom.el('label', {
            className: 'setting-choice',
            children: [
                input,
                themeIcon(option),
                app.utils.dom.el('span', { text: t(option.labelKey) })
            ]
        });
    }

    function languageOption(option) {
        var input = app.utils.dom.el('input', {
            attrs: {
                type: 'radio',
                name: 'web-language',
                value: option.id
            }
        });
        input.checked = app.app.state.language === option.id;
        input.addEventListener('change', function () {
            if (input.checked) {
                app.app.setLanguage(option.id);
            }
        });

        return app.utils.dom.el('label', {
            className: 'setting-choice',
            children: [
                input,
                app.utils.dom.el('span', { text: t(option.labelKey) })
            ]
        });
    }

    app.pages.webSettingPage = {
        id: 'web-setting',
        titleKey: 'webSettingTitle',
        // 建立網站設定頁，提供 theme 與 language 選擇。
        mount: function mount(container) {
            container.appendChild(
                app.utils.dom.el('section', {
                    className: 'web-setting-page',
                    children: [
                        app.utils.dom.el('section', {
                            className: 'panel-section',
                            children: [
                                app.utils.dom.el('h2', { text: t('webSettingTheme') }),
                                app.utils.dom.el('div', {
                                    className: 'panel-body',
                                    children: [
                                        app.utils.dom.el('div', {
                                            className: 'setting-choice-list',
                                            children: app.app.themeOptions.map(themeOption)
                                        })
                                    ]
                                })
                            ]
                        }),
                        app.utils.dom.el('section', {
                            className: 'panel-section',
                            children: [
                                app.utils.dom.el('h2', { text: t('webSettingLanguage') }),
                                app.utils.dom.el('div', {
                                    className: 'panel-body',
                                    children: [
                                        app.utils.dom.el('div', {
                                            className: 'setting-choice-list',
                                            children: app.i18n.languageOptions.map(languageOption)
                                        })
                                    ]
                                })
                            ]
                        })
                    ]
                })
            );
        },
        // 設定頁沒有長生命週期資源需要釋放。
        unmount: function unmount() {}
    };
})(window.DitherApp);
