(function (app) {
    // App shell 層級狀態。
    // 只保存全域 UI 偏好，不保存 Dither Editor 的圖片、canvas 或 pipeline。
    // 將外部傳入或儲存的 theme 正規化，避免未知值污染 DOM data-theme。
    function normalizeTheme(theme) {
        return theme === 'dark' ? 'dark' : 'light';
    }

    function normalizeLanguage(language) {
        return app.i18n.normalizePreference(language);
    }

    // 持久化網站偏好；失敗時不阻斷 UI，因為主功能仍可使用。
    function savePreferences() {
        app.core.settingsStore.save({
            theme: app.app.state.theme,
            language: app.app.state.language
        });
    }

    // 套用 theme 到 state 與 body attribute，必要時同步寫入 localStorage。
    function applyTheme(theme, options) {
        var nextTheme = normalizeTheme(theme);
        app.app.state.theme = nextTheme;
        document.body.setAttribute('data-theme', nextTheme);
        if (!options || options.save !== false) {
            savePreferences();
        }
    }

    function applyLanguage(language, options) {
        var nextLanguage = normalizeLanguage(language);
        app.app.state.language = nextLanguage;
        app.i18n.setLanguagePreference(nextLanguage);
        if (!options || options.save !== false) {
            savePreferences();
        }
        if (app.app.refreshUi) {
            app.app.refreshUi();
        }
    }

    var storedSettings = app.core.settingsStore.load() || {};

    app.app.state = {
        activePageId: null,
        blockingOperation: null,
        theme: normalizeTheme(storedSettings.theme),
        language: normalizeLanguage(storedSettings.language)
    };

    app.app.setTheme = applyTheme;
    app.app.setLanguage = applyLanguage;
    app.app.themeOptions = [
        { id: 'light', labelKey: 'themeLight' },
        { id: 'dark', labelKey: 'themeDark' }
    ];

    applyTheme(app.app.state.theme, { save: false });
    applyLanguage(app.app.state.language, { save: false });
})(window.DitherApp);
