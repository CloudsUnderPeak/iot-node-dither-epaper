(function (app) {
    var DEFAULT_LANGUAGE = 'en';
    var SUPPORTED_LANGUAGES = ['en', 'zh-TW'];
    var languagePreference = 'auto';
    var currentLanguage = DEFAULT_LANGUAGE;

    function normalizePreference(language) {
        if (language === 'zh-TW' || language === 'en') {
            return language;
        }
        return 'auto';
    }

    function normalizeLanguage(language) {
        return SUPPORTED_LANGUAGES.indexOf(language) !== -1 ? language : DEFAULT_LANGUAGE;
    }

    function languageFromBrowser() {
        var languages = navigator.languages && navigator.languages.length
            ? navigator.languages
            : [navigator.language || DEFAULT_LANGUAGE];
        var matched = languages.find(function (language) {
            return /^zh-(tw|hk|mo|hant)/i.test(language);
        });
        return matched ? 'zh-TW' : DEFAULT_LANGUAGE;
    }

    function resolveLanguage(preference) {
        return normalizePreference(preference) === 'auto'
            ? languageFromBrowser()
            : normalizeLanguage(preference);
    }

    function setDocumentLanguage(language) {
        document.documentElement.setAttribute('lang', language);
    }

    function setLanguagePreference(language) {
        languagePreference = normalizePreference(language);
        currentLanguage = resolveLanguage(languagePreference);
        setDocumentLanguage(currentLanguage);
    }

    function interpolate(value, replacements) {
        if (typeof value !== 'string' || replacements === undefined || replacements === null) {
            return value;
        }
        return value.replace(/\{([a-zA-Z][a-zA-Z0-9_]*|\d+)\}/g, function (match, name) {
            var replacement = Array.isArray(replacements)
                ? replacements[Number(name)]
                : replacements[name];
            return replacement === undefined || replacement === null ? match : String(replacement);
        });
    }

    function t(key, replacements) {
        var active = app.i18n[currentLanguage] || app.i18n[DEFAULT_LANGUAGE] || {};
        var fallback = app.i18n[DEFAULT_LANGUAGE] || {};
        return interpolate(active[key] || fallback[key] || key, replacements);
    }

    app.i18n.normalizePreference = normalizePreference;
    app.i18n.resolveLanguage = resolveLanguage;
    app.i18n.setLanguagePreference = setLanguagePreference;
    app.i18n.interpolate = interpolate;
    app.i18n.t = t;
    app.i18n.languageOptions = [
        { id: 'auto', labelKey: 'languageAuto' },
        { id: 'zh-TW', labelKey: 'languageZhTw' },
        { id: 'en', labelKey: 'languageEnglish' }
    ];
    app.i18n.currentLanguage = function current() {
        return currentLanguage;
    };
    app.i18n.languagePreference = function preference() {
        return languagePreference;
    };

    setLanguagePreference('auto');
})(window.DitherApp);
