(function (app) {
    // Page registry 只保存 page module 的公開介面。
    // app shell 透過這裡取得頁面，不直接知道各頁內部 controller 或 canvas。
    var pages = {};

    app.app.pageRegistry = {
        // 註冊頁面模組；頁面必須至少提供 id，mount/unmount 由 router 呼叫。
        register: function register(page) {
            pages[page.id] = page;
        },
        // 依 id 取得頁面模組。
        get: function get(id) {
            return pages[id];
        },
        // 回傳目前所有頁面，給 menu 產生選項。
        all: function all() {
            return Object.keys(pages).map(function (id) {
                return pages[id];
            });
        }
    };
})(window.DitherApp);
