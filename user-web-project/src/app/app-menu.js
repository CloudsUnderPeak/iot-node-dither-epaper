(function (app) {
    // 右上角 Menu 的 DOM 與互動。
    // Menu 只回報使用者選擇的 page id，實際切頁由 router / shell 處理。
    // AppMenu 接收觸發按鈕與頁面切換 callback，自己建立下拉選單 DOM。
    function AppMenu(button, onSelect) {
        this.button = button;
        this.onSelect = onSelect;
        this.button.setAttribute('aria-expanded', 'false');
        this.node = app.utils.dom.el('div', {
            className: 'app-menu',
            attrs: { hidden: 'hidden' }
        });
        document.body.appendChild(this.node);
        this.render();
        this.bind();
    }

    // 依目前 pageRegistry 內容重新渲染選單項目。
    // 每次展開都重新 render，因此登入狀態永遠即時。
    AppMenu.prototype.render = function render() {
        var self = this;
        app.utils.dom.clear(this.node);

        function addItem(id, label) {
            var button = app.utils.dom.el('button', { text: label, attrs: { type: 'button' } });
            button.addEventListener('click', function () {
                self.hide();
                self.onSelect(id);
            });
            self.node.appendChild(button);
        }

        function addDivider() {
            self.node.appendChild(app.utils.dom.el('hr', { className: 'app-menu-divider' }));
        }

        addItem('dither-editor', app.i18n.t('menuDitherEditor'));

        addDivider();
        self.node.appendChild(app.utils.dom.el('div', {
            className: 'app-menu-group',
            text: app.i18n.t('menuDeviceGroup')
        }));
        // 裝置連線狀態一律由 header 的 app-status 圓點呈現，選單不另外標示。
        addItem('device-info', app.i18n.t('menuDeviceInfo'));
        addItem('device-network', app.i18n.t('menuDeviceNetwork'));
        addItem('device-system', app.i18n.t('menuDeviceSystem'));

        addDivider();
        addItem('web-setting', app.i18n.t('menuWebSetting'));
        addItem('help', app.i18n.t('menuHelp'));
        addItem('about', app.i18n.t('menuAbout'));

        if (app.device.auth.hasToken()) {
            addDivider();
            var logoutButton = app.utils.dom.el('button', {
                text: app.i18n.t('menuLogout'),
                attrs: { type: 'button' }
            });
            logoutButton.addEventListener('click', function () {
                self.hide();
                app.device.auth.logout();
            });
            this.node.appendChild(logoutButton);
        }
    };

    // 綁定按鈕開關與外部點擊關閉，避免選單停留在畫面上。
    AppMenu.prototype.bind = function bind() {
        var self = this;
        this.button.addEventListener('click', function () {
            self.toggle();
        });
        document.addEventListener('click', function (event) {
            if (self.button.contains(event.target) || self.node.contains(event.target)) {
                return;
            }
            self.hide();
        });
    };

    // 切換選單顯示狀態；render 會在每次打開前更新選項。
    AppMenu.prototype.toggle = function toggle() {
        if (this.node.hidden) {
            this.render();
            this.node.hidden = false;
            this.button.setAttribute('aria-expanded', 'true');
        } else {
            this.hide();
        }
    };

    // 關閉選單並同步 aria-expanded。
    AppMenu.prototype.hide = function hide() {
        this.node.hidden = true;
        this.button.setAttribute('aria-expanded', 'false');
    };

    app.app.AppMenu = AppMenu;
})(window.DitherApp);
