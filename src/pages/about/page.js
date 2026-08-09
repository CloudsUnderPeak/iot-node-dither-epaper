(function (app) {
    // About 是簡單靜態頁面，仍透過 pageRegistry 管理，讓 SPA 導航流程一致。
    app.pages.aboutPage = {
        id: 'about',
        titleKey: 'aboutTitle',
        // 將 About 內容掛進 page-host。
        mount: function mount(container) {
            container.appendChild(
                app.utils.dom.el('section', {
                    className: 'simple-page',
                    children: [
                        app.utils.dom.el('h1', { text: app.i18n.t('aboutTitle') }),
                        app.utils.dom.el('p', { text: app.i18n.t('placeholderAbout') })
                    ]
                })
            );
        },
        // 靜態頁目前沒有 listener 或 timer 需要清理。
        unmount: function unmount() {}
    };
})(window.DitherApp);
