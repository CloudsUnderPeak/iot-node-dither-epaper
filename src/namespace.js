(function () {
    // 全專案唯一 namespace。
    // 由於專案必須支援直接雙擊 index.html，不能用 ES Modules；
    // 所有模組都掛到 window.DitherApp，避免污染更多全域變數。
    window.DitherApp = window.DitherApp || {};
    window.DitherApp.app = window.DitherApp.app || {};
    window.DitherApp.assets = window.DitherApp.assets || {};
    window.DitherApp.config = window.DitherApp.config || {};
    window.DitherApp.core = window.DitherApp.core || {};
    window.DitherApp.device = window.DitherApp.device || {};
    window.DitherApp.i18n = window.DitherApp.i18n || {};
    window.DitherApp.pages = window.DitherApp.pages || {};
    window.DitherApp.pages.ditherEditor = window.DitherApp.pages.ditherEditor || {};
    window.DitherApp.ui = window.DitherApp.ui || {};
    window.DitherApp.utils = window.DitherApp.utils || {};
})(window);
