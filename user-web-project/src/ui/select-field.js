(function (app) {
    // select 的共用包裝：隱藏原生下拉箭頭，改疊主題化的 chevron 圖示。
    // 盒位使用全站統一的結尾圖示尺寸，讓各表單元件結尾的 svg 對齊。
    function create(select) {
        return app.utils.dom.el('div', {
            className: 'select-field',
            children: [
                select,
                app.utils.dom.el('span', {
                    className: 'select-chevron',
                    attrs: { 'aria-hidden': 'true' },
                    children: [app.ui.svgIcons.create('assets/icons/editor/chevron-down.svg')]
                })
            ]
        });
    }

    app.ui.selectField = { create: create };
})(window.DitherApp);
