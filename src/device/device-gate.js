(function (app) {
    // 將裝置頁容器綁定 device-live：離線 banner、整頁反灰鎖定與恢復後自動刷新。
    // 頁面把要反灰的區塊加上 .device-gate class；表單包在 fieldset 內由這裡統一 disable。
    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function bindLiveGate(container, options) {
        options = options || {};
        var banner = app.utils.dom.el('div', {
            className: 'device-banner',
            attrs: { role: 'alert', hidden: 'hidden' }
        });
        var tickerId = null;
        var lastState = null;

        function stopTicker() {
            if (tickerId !== null) {
                window.clearInterval(tickerId);
                tickerId = null;
            }
        }

        function offlineText() {
            var lastOnlineAt = app.device.live.lastOnlineAt();
            if (!lastOnlineAt) {
                return t('deviceOfflineBanner');
            }
            var seconds = Math.max(0, Math.round((Date.now() - lastOnlineAt) / 1000));
            return t('deviceOfflineBanner') + ' ' + t('deviceOfflineLastSeen', { seconds: seconds });
        }

        function apply(state) {
            var blocked = state === 'offline' || state === 'standalone';
            container.classList.toggle('is-device-offline', blocked);
            Array.prototype.forEach.call(container.querySelectorAll('fieldset'), function (fieldset) {
                fieldset.disabled = blocked;
            });
            stopTicker();
            if (state === 'offline') {
                banner.hidden = false;
                banner.className = 'device-banner is-offline';
                banner.textContent = offlineText();
                // 每秒更新「最後成功連線」秒數，讓使用者知道重試仍在進行。
                tickerId = window.setInterval(function () {
                    banner.textContent = offlineText();
                }, 1000);
            } else if (state === 'standalone') {
                banner.hidden = false;
                banner.className = 'device-banner is-standalone';
                banner.textContent = t('deviceStandaloneBanner');
            } else if (state === 'online' && (lastState === 'offline' || lastState === 'standalone')) {
                // 恢復連線：短暫顯示成功訊息，並讓頁面自動重抓資料。
                banner.hidden = false;
                banner.className = 'device-banner is-recovered';
                banner.textContent = t('deviceRecoveredBanner');
                window.setTimeout(function () {
                    if (banner.className.indexOf('is-recovered') !== -1) {
                        banner.hidden = true;
                    }
                }, 2200);
                if (options.onOnline) {
                    options.onOnline();
                }
            } else {
                banner.hidden = true;
            }
            lastState = state;
        }

        var unsubscribe = app.device.live.subscribe(apply);
        apply(app.device.live.state());

        return {
            banner: banner,
            unbind: function unbind() {
                stopTicker();
                unsubscribe();
            }
        };
    }

    app.device.bindLiveGate = bindLiveGate;
})(window.DitherApp);
