(function (app) {
    function settleImage(image) {
        if (typeof image.decode === 'function') {
            try {
                return image.decode().catch(function () {});
            } catch (error) {
                return Promise.resolve();
            }
        }
        if (image.complete) {
            return Promise.resolve();
        }
        return new Promise(function (resolve) {
            image.addEventListener('load', resolve, { once: true });
            image.addEventListener('error', resolve, { once: true });
        });
    }

    // App 解鎖前等待初始頁面已建立的本地圖示 settled；單一圖示失敗不阻斷主功能。
    function waitForInitialImages() {
        var appNode = document.getElementById('app');
        var images = appNode ? Array.prototype.slice.call(appNode.querySelectorAll('img')) : [];
        var settled = 0;
        if (app.startupGate) {
            app.startupGate.setProgress(images.length ? 84 : 94);
        }
        return Promise.all(images.map(function (image) {
            return settleImage(image).then(function () {
                settled += 1;
                if (app.startupGate) {
                    app.startupGate.setProgress(84 + (10 * settled / images.length));
                }
            });
        }));
    }

    // 讓已掛載 UI 至少完成一次 layout + paint，再移除全畫面 loading gate。
    function waitForPaint() {
        return new Promise(function (resolve) {
            window.requestAnimationFrame(function () {
                window.requestAnimationFrame(resolve);
            });
        });
    }

    function localizeStartupGate() {
        if (!app.startupGate) {
            return;
        }
        app.startupGate.setCopy({
            loading: app.i18n.t('startupLoading'),
            error: app.i18n.t('startupLoadFailed'),
            reload: app.i18n.t('startupReload')
        });
        app.app.applyShellCopy(
            document.querySelector('.app-title'),
            document.getElementById('app-menu-button')
        );
    }

    // App 啟動點：等待所有 page entry 完成註冊後，再交給 app shell mount。
    // 這裡不載入任何頁面內部邏輯，避免 main.js 成為第二份頁面 manifest。
    // 啟動整個 SPA：等待所有頁面 entry 完成註冊後，再建立 AppShell。
    function start() {
        localizeStartupGate();
        app.app
            .whenPageEntriesReady()
            .then(function () {
                if (app.startupGate) {
                    app.startupGate.setProgress(70);
                }
                var shell = new app.app.AppShell();
                shell.start();
                // Capability discovery runs beside the visual startup gate and never blocks it.
                app.device.epaper.start();
                app.device.live.start();
                if (app.startupGate) {
                    app.startupGate.setProgress(82);
                }
                return waitForInitialImages();
            })
            .then(function () {
                if (app.startupGate) {
                    app.startupGate.setProgress(96);
                }
                return waitForPaint();
            })
            .then(function () {
                if (app.startupGate) {
                    app.startupGate.complete();
                }
            })
            .catch(function (error) {
                if (app.startupGate) {
                    app.startupGate.fail();
                }
                window.console.error(error);
            });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', start);
    } else {
        start();
    }
})(window.DitherApp);
