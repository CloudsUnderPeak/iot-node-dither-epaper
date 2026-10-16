(function (app) {
    // Help 是唯一延後載入的頁面：啟動時只註冊輕量 page，內容在第一次進入或 startup 完成後 idle 預載時才載入。
    // 直接以 #/help 開啟時仍在 startup 期間載入，讓 loading 解除後畫面已完整。
    var scripts = [
        'src/pages/help/i18n/en.js',
        'src/pages/help/i18n/zh-TW.js',
        'src/pages/help/document-manifest.js',
        'src/pages/help/content-model.js',
        'src/pages/help/validation.js',
        'src/pages/help/visuals.js',
        'src/pages/help/page.js'
    ];
    var loading = null;
    var ready = false;
    var mounted = null;

    // Help 專屬雙語內容、文件樹與視覺元件依序載入；驗證只在 Help 載入時警告。
    function load() {
        if (!loading) {
            loading = app.app.scriptLoader.loadMany(scripts).then(function () {
                var validation = app.pages.help.validation.validate();
                validation.errors.concat(validation.warnings).forEach(function (message) {
                    window.console.warn('[Help] ' + message);
                });
                ready = true;
            });
        }
        return loading;
    }

    function initialRouteIsHelp() {
        return String(window.location.hash).replace(/^#\/?/, '').replace(/^\/+/, '').split('/')[0] === 'help';
    }

    app.app.pageRegistry.register({
        id: 'help',
        titleKey: 'helpTitle',
        preload: function preload() {
            load().catch(function () {});
        },
        mount: function mount(container, context) {
            var target = { live: false };
            mounted = target;
            function show() {
                if (mounted === target) {
                    target.live = true;
                    app.pages.helpPage.mount(container, context);
                }
            }
            // 已載入時同步 mount，startup 的 initial image 等待與首次 paint 才看得到完整內容。
            if (ready) {
                show();
                return;
            }
            // 載入期間的子 route 變更由 router 寫入 context.route，載入完成後直接渲染最新 route。
            load().then(show, function () {
                if (mounted === target) {
                    var notice = app.ui.createNotice();
                    container.appendChild(notice.node);
                    notice.set(app.i18n.t('helpLoadFailed'), { error: true });
                }
            });
        },
        onRouteChange: function onRouteChange(route, context) {
            if (mounted && mounted.live) {
                app.pages.helpPage.onRouteChange(route, context);
            }
        },
        unmount: function unmount() {
            if (mounted && mounted.live) {
                app.pages.helpPage.unmount();
            }
            mounted = null;
        }
    });

    if (initialRouteIsHelp()) {
        app.app.registerPageEntry(load());
    }
})(window.DitherApp);
