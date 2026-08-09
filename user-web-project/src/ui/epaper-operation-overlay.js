(function (app) {
    function EpaperOperationOverlay() {
        this.node = app.utils.dom.el('div', {
            className: 'epaper-operation-overlay',
            attrs: { hidden: 'hidden', role: 'status', 'aria-live': 'polite', 'aria-busy': 'true' }
        });
        this.message = app.utils.dom.el('div', { className: 'epaper-operation-message app-loading-message' });
        this.percent = app.utils.dom.el('strong', { className: 'epaper-operation-percent' });
        this.bar = app.utils.dom.el('span', { className: 'epaper-operation-progress-bar app-loading-progress-bar' });
        this.node.appendChild(app.utils.dom.el('div', {
            className: 'epaper-operation-card app-loading-content',
            children: [
                app.utils.dom.el('span', { className: 'epaper-operation-spinner app-loading-spinner', attrs: { 'aria-hidden': 'true' } }),
                this.message,
                this.percent,
                app.utils.dom.el('div', {
                    className: 'epaper-operation-progress app-loading-progress',
                    attrs: { role: 'progressbar', 'aria-valuemin': '0', 'aria-valuemax': '100' },
                    children: [this.bar]
                })
            ]
        }));
        document.body.appendChild(this.node);
        var self = this;
        this.unsubscribe = app.device.epaper.subscribe(function (snapshot) { self.render(snapshot); });
        this.render(app.device.epaper.snapshot());
    }

    EpaperOperationOverlay.prototype.render = function render(snapshot) {
        var operation = snapshot.operation;
        this.node.hidden = !operation.active;
        if (!operation.active) {
            return;
        }
        var progress = Math.max(0, Math.min(100, Number(operation.progressPercent) || 0));
        this.message.textContent = app.i18n.t(operation.messageKey || 'epaperProgressPreflight');
        this.percent.textContent = Math.round(progress) + '%';
        this.bar.style.width = progress + '%';
        var progressNode = this.node.querySelector('[role="progressbar"]');
        progressNode.setAttribute('aria-valuenow', String(Math.round(progress)));
        this.node.setAttribute('aria-label', this.message.textContent + ' ' + this.percent.textContent);
    };

    app.ui.EpaperOperationOverlay = EpaperOperationOverlay;
})(window.DitherApp);
