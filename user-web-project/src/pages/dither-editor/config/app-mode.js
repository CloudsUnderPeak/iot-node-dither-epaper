(function (app) {
    // App mode 預留給未來嵌入式/後端登入/裝置連線等模式切換。
    // 現階段 standalone 代表完全在瀏覽器本機完成影像處理。
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};
    app.pages.ditherEditor.config.appMode = 'standalone';
})(window.DitherApp);
