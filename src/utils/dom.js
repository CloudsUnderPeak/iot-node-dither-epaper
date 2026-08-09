(function (app) {
    // DOM 小工具只負責建立/清空元素，不知道任何 Dither 業務語意。
    // 讓頁面與 feature 可以用一致方式建立 classic script 時代的 DOM 結構。
    app.utils.dom = {
        // 建立 DOM element，統一處理 className、text、attrs、children。
        el: function el(tagName, options) {
            var node = document.createElement(tagName);
            options = options || {};
            if (options.className) {
                node.className = options.className;
            }
            if (options.text !== undefined && options.text !== null) {
                node.textContent = options.text;
            }
            if (options.attrs) {
                Object.keys(options.attrs).forEach(function (name) {
                    node.setAttribute(name, options.attrs[name]);
                });
            }
            if (options.children) {
                options.children.forEach(function (child) {
                    node.appendChild(child);
                });
            }
            return node;
        },
        // 清空節點內容，避免呼叫端直接操作 innerHTML。
        clear: function clear(node) {
            while (node.firstChild) {
                node.removeChild(node.firstChild);
            }
        },
        // 建立 select option，讓 panel code 保持簡短。
        option: function option(value, text) {
            var node = document.createElement('option');
            node.value = value;
            node.textContent = text;
            return node;
        }
    };
})(window.DitherApp);
