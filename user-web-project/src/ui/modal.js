(function (app) {
    // 全站共用 modal：單一 active dialog、背景 inert、focus trap 與 Esc / backdrop 關閉。
    // dialog 內容 DOM 由呼叫端建立；這裡只負責生命週期與無障礙行為。
    var active = null;

    function focusableItems(node) {
        return Array.prototype.filter.call(
            node.querySelectorAll('button, input, select, textarea, a[href], [tabindex]'),
            function (item) {
                return !item.disabled && item.offsetParent !== null;
            }
        );
    }

    function handleKeydown(event) {
        if (!active) {
            return;
        }
        if (event.key === 'Escape') {
            event.preventDefault();
            requestClose();
            return;
        }
        if (event.key !== 'Tab') {
            return;
        }
        // Tab / Shift+Tab 焦點循環限制在 dialog 內。
        var items = focusableItems(active.dialog);
        if (!items.length) {
            event.preventDefault();
            return;
        }
        var first = items[0];
        var last = items[items.length - 1];
        if (event.shiftKey && document.activeElement === first) {
            event.preventDefault();
            last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
            event.preventDefault();
            first.focus();
        }
    }

    // 使用者主動關閉（Esc / backdrop / 取消）；busy 中的 dialog 可用 dismissible:false 擋住。
    function requestClose() {
        if (!active || active.options.dismissible === false) {
            return;
        }
        close();
    }

    function open(dialogNode, options) {
        close();
        options = options || {};
        var backdrop = app.utils.dom.el('div', { className: 'modal-backdrop', children: [dialogNode] });
        dialogNode.classList.add('modal-dialog');
        dialogNode.setAttribute('role', 'dialog');
        dialogNode.setAttribute('aria-modal', 'true');
        backdrop.addEventListener('mousedown', function (event) {
            if (event.target === backdrop) {
                requestClose();
            }
        });
        var appNode = document.getElementById('app');
        active = {
            dialog: dialogNode,
            backdrop: backdrop,
            options: options,
            previousFocus: document.activeElement,
            previousOverflow: document.body.style.overflow
        };
        document.addEventListener('keydown', handleKeydown, true);
        document.body.appendChild(backdrop);
        document.body.style.overflow = 'hidden';
        if (appNode) {
            appNode.setAttribute('inert', '');
        }
        var target = options.initialFocus || focusableItems(dialogNode)[0];
        if (target) {
            target.focus();
        }
    }

    function close() {
        if (!active) {
            return;
        }
        var record = active;
        active = null;
        document.removeEventListener('keydown', handleKeydown, true);
        record.backdrop.remove();
        document.body.style.overflow = record.previousOverflow;
        var appNode = document.getElementById('app');
        if (appNode) {
            appNode.removeAttribute('inert');
        }
        if (record.previousFocus && record.previousFocus.focus) {
            record.previousFocus.focus();
        }
        if (record.options.onClose) {
            record.options.onClose();
        }
    }

    app.ui.modal = {
        open: open,
        close: close,
        // busy 流程需要暫時擋住 Esc / backdrop 時，直接改 active dialog 的設定。
        setDismissible: function setDismissible(dismissible) {
            if (active) {
                active.options.dismissible = dismissible;
            }
        }
    };
})(window.DitherApp);
