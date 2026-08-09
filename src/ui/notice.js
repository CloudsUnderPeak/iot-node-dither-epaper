(function (app) {
    // 頁面內 notice：一般成功訊息 2.2 秒自動消失，錯誤與需保留脈絡的訊息常駐。
    // generation 計數避免舊 timer 清掉較新的訊息。
    function createNotice(className) {
        var node = app.utils.dom.el('div', {
            className: 'device-notice' + (className ? ' ' + className : ''),
            attrs: { role: 'status', 'aria-live': 'polite', hidden: 'hidden' }
        });
        var generation = 0;

        function set(message, options) {
            options = options || {};
            generation += 1;
            var current = generation;
            node.textContent = message;
            node.classList.toggle('is-error', !!options.error);
            node.hidden = !message;
            if (message && !options.error && !options.sticky) {
                window.setTimeout(function () {
                    if (generation === current) {
                        clear();
                    }
                }, options.duration || 2200);
            }
        }

        function clear() {
            generation += 1;
            node.hidden = true;
            node.textContent = '';
            node.classList.remove('is-error');
        }

        return { node: node, set: set, clear: clear };
    }

    app.ui.createNotice = createNotice;
})(window.DitherApp);
