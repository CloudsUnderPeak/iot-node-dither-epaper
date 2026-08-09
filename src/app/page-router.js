(function (app) {
    // 純前端 hash router：同步 Menu 切頁、初始 URL、瀏覽器上一頁/下一頁。
    // Router 保存完整 route，但不保存任何 Dither editor 的圖片或 pipeline state。
    // PageRouter 持有頁面容器與 app context，負責 mount/unmount 目前頁。
    function PageRouter(host, context) {
        this.host = host;
        this.context = context;
        this.currentPage = null;
        this.currentRoute = '';
        this.defaultPageId = 'dither-editor';
        this.onPopState = this.handlePopState.bind(this);
        // 子文件頁透過同一個 router 寫入 history，避免各 page 自行監聽全域事件。
        this.context.navigate = this.navigate.bind(this);
        this.context.currentRoute = this.route.bind(this);
    }

    function pageTitle(page) {
        return page.titleKey ? app.i18n.t(page.titleKey) : page.title;
    }

    // 啟動時監聽瀏覽器返回/前進，並從 URL hash 還原頁面。
    PageRouter.prototype.start = function start(defaultPageId) {
        this.defaultPageId = defaultPageId || this.defaultPageId;
        window.addEventListener('popstate', this.onPopState);
        this.navigate(this.routeFromLocation(), { replace: true });
    };

    // 切換 route：第一段是 page id，其餘段落交給 page 自己解讀。
    PageRouter.prototype.navigate = function navigate(route, options) {
        options = options || {};
        route = this.normalizeRoute(route);
        if (app.app.state.blockingOperation && this.currentRoute && route !== this.currentRoute) {
            if (options.fromHistory) {
                this.writeHistory(this.currentRoute, true);
            }
            return false;
        }
        var pageId = this.pageIdFromRoute(route);
        var page = app.app.pageRegistry.get(pageId);
        if (!page) {
            throw new Error('Unknown page: ' + pageId);
        }
        if (this.currentPage && this.currentPage.id === page.id) {
            this.currentRoute = route;
            this.context.route = route;
            if (this.currentPage.onRouteChange) {
                this.currentPage.onRouteChange(route, this.context);
            }
            if (!options.fromHistory) {
                this.writeHistory(route, options.replace);
            }
            return;
        }
        if (this.currentPage && this.currentPage.unmount) {
            this.currentPage.unmount();
        }
        // page-host 每次切頁都清空；需要保留業務狀態的頁面必須自己在 unmount cache。
        app.utils.dom.clear(this.host);
        this.currentPage = page;
        this.currentRoute = route;
        this.context.route = route;
        app.app.state.activePageId = page.id;
        this.context.setTitle(pageTitle(page));
        page.mount(this.host, this.context);
        if (!options.fromHistory) {
            this.writeHistory(route, options.replace);
        }
        return true;
    };

    // 瀏覽器返回/前進時不新增 history，只依當前 URL 切頁。
    PageRouter.prototype.handlePopState = function handlePopState(event) {
        var route = event.state && event.state.route ? event.state.route : this.routeFromLocation();
        if (!app.app.pageRegistry.get(this.pageIdFromRoute(route))) {
            route = this.defaultPageId;
        }
        this.navigate(route, { fromHistory: true });
    };

    PageRouter.prototype.normalizeRoute = function normalizeRoute(route) {
        var normalized = String(route || '').replace(/^#\/?/, '').replace(/^\/+|\/+$/g, '');
        return normalized || this.defaultPageId;
    };

    PageRouter.prototype.pageIdFromRoute = function pageIdFromRoute(route) {
        return this.normalizeRoute(route).split('/')[0];
    };

    // 從 URL hash 讀取完整 route；未知第一段仍回退到預設頁。
    PageRouter.prototype.routeFromLocation = function routeFromLocation() {
        var route = this.normalizeRoute(window.location.hash);
        if (!app.app.pageRegistry.get(this.pageIdFromRoute(route))) {
            return this.defaultPageId;
        }
        return route;
    };

    // 保留舊的 pageId 查詢介面，呼叫端若只需要頁面層級仍可使用。
    PageRouter.prototype.pageIdFromLocation = function pageIdFromLocation() {
        return this.pageIdFromRoute(this.routeFromLocation());
    };

    PageRouter.prototype.route = function route() {
        return this.currentRoute || this.defaultPageId;
    };

    // 將完整 route 寫入 history，讓 Help 子文件也能前進、後退與直接分享。
    PageRouter.prototype.writeHistory = function writeHistory(route, replace) {
        route = this.normalizeRoute(route);
        var pageId = this.pageIdFromRoute(route);
        var hash = '#/' + route;
        if (window.location.hash === hash && !replace) {
            return;
        }
        var state = { pageId: pageId, route: route };
        if (replace) {
            window.history.replaceState(state, '', hash);
            return;
        }
        window.history.pushState(state, '', hash);
    };

    PageRouter.prototype.refreshCurrentPage = function refreshCurrentPage() {
        if (!this.currentPage) {
            return;
        }
        var page = this.currentPage;
        if (page.unmount) {
            page.unmount();
        }
        app.utils.dom.clear(this.host);
        this.context.route = this.currentRoute;
        this.context.setTitle(pageTitle(page));
        page.mount(this.host, this.context);
    };

    app.app.PageRouter = PageRouter;
})(window.DitherApp);
