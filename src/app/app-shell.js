(function (app) {
    // AppShell 擁有 header、status、menu 與 page-host。
    // 它只呼叫 page module 的 mount/unmount，不碰任何頁面內部實作。
    // 將 app state 的狀態物件轉成 header 右側的狀態圓點。
    function renderStatus(node, status) {
        var label = status || app.i18n.t('statusReady');
        var state = statusState(label);
        // 圓點同時承載裝置連線與頁面狀態，優先序：
        // 裝置離線（紅）> 頁面 error/busy > 裝置在線（綠）> 未偵測到裝置（灰）。
        var live = app.device.live ? app.device.live.state() : null;
        if (live === 'offline') {
            state = 'error';
            label = app.i18n.t('deviceLiveOffline');
        } else if (state === 'ready' && live === 'online') {
            label = app.i18n.t('deviceLiveOnline');
        } else if (state === 'ready' && live === 'standalone') {
            state = 'idle';
            label = app.i18n.t('deviceLiveStandalone');
        }
        node.className = 'app-status is-' + state;
        node.textContent = '';
        node.title = label;
        node.setAttribute('aria-label', label);
    }

    // 把錯誤/忙碌/就緒等狀態壓成 CSS class 需要的單一狀態名稱。
    function statusState(status) {
        var value = String(status || '').toLowerCase();
        if (value.indexOf('error') !== -1 || value.indexOf('錯誤') !== -1) {
            return 'error';
        }
        if (
            value.indexOf('processing') !== -1 ||
            value.indexOf('loading') !== -1 ||
            value.indexOf('exporting') !== -1 ||
            value.indexOf('uploading') !== -1 ||
            value.indexOf('處理') !== -1 ||
            value.indexOf('載入') !== -1 ||
            value.indexOf('正在匯出') !== -1 ||
            value.indexOf('上傳') !== -1
        ) {
            return 'busy';
        }
        if (
            value.indexOf('empty') !== -1 ||
            value.indexOf('begin') !== -1 ||
            value.indexOf('建立') !== -1 ||
            value.indexOf('開始') !== -1
        ) {
            return 'idle';
        }
        return 'ready';
    }

    // Startup gate 本地化時先同步 header placeholder，避免 loading 與 shell 暫時使用不同語言。
    function applyShellCopy(titleNode, menuButton) {
        document.title = app.i18n.t('appTitle');
        titleNode.textContent = app.i18n.t('appTitle');
        menuButton.setAttribute('aria-label', app.i18n.t('menuButtonLabel'));
        var menuLabel = menuButton.querySelector('span:not(.svg-icon)');
        if (menuLabel) {
            menuLabel.textContent = app.i18n.t('menuButtonLabel');
        }
    }

    // AppShell 持有 header、page-host、router、menu 的 DOM 參照。
    function AppShell() {
        this.host = document.getElementById('page-host');
        this.titleNode = document.querySelector('.app-title');
        this.statusNode = document.getElementById('app-status');
        this.menuButton = document.getElementById('app-menu-button');
        this.router = new app.app.PageRouter(this.host, {
            statusNode: this.statusNode,
            setTitle: this.setTitle.bind(this),
            setStatus: this.setStatus.bind(this)
        });
        this.menu = new app.app.AppMenu(this.menuButton, this.router.navigate.bind(this.router));
        this.epaperOverlay = new app.ui.EpaperOperationOverlay();
    }

    // 頁面切換時更新中央標題。
    AppShell.prototype.setTitle = function setTitle(title) {
        this.titleNode.textContent = title || app.i18n.t('appTitle');
    };

    // 讓頁面或 controller 可以更新全站狀態顯示。
    AppShell.prototype.setStatus = function setStatus(status) {
        this.lastStatus = status || null;
        renderStatus(this.statusNode, status);
    };

    // 建立選單與 router，並啟動預設頁面。
    AppShell.prototype.start = function start() {
        var self = this;
        this.applyShellText();
        app.app.refreshUi = this.refreshUi.bind(this);
        // 裝置連線狀態改變時，用最後一次頁面狀態重新合成圓點。
        app.device.live.subscribe(function () {
            renderStatus(self.statusNode, self.lastStatus);
        });
        app.device.epaper.subscribe(function () {
            self.menuButton.disabled = Boolean(app.app.state.blockingOperation);
            self.menu.render();
        });
        this.router.start('dither-editor');
    };

    // index.html 內的 header 文字只是 JS 啟動前的 placeholder；
    // 這裡統一改用 i18n 蓋章，讓所有使用者可見字串走同一份字典。
    AppShell.prototype.applyShellText = function applyShellText() {
        applyShellCopy(this.titleNode, this.menuButton);
        renderStatus(this.statusNode, null);
    };

    AppShell.prototype.refreshUi = function refreshUi() {
        this.applyShellText();
        this.menu.render();
        this.router.refreshCurrentPage();
    };

    app.app.AppShell = AppShell;
    app.app.applyShellCopy = applyShellCopy;
    app.app.renderStatus = renderStatus;
})(window.DitherApp);
