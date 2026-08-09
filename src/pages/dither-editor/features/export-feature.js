(function (app) {
    // Export feature 是 action，不是可拖曳的 pipeline tool。
    // 它使用目前 preview/result ImageData 直接輸出 PNG，不在面板中折疊。
    var ui = app.pages.ditherEditor.panelUtils;
    var actionRefs = null;

    app.pages.ditherEditor.featureRegistry.register({
        id: 'export',
        labelKey: 'actionExport',
        pipelineStage: 'fixedAfter',
        pipelineOrder: 10,
        actionOrder: 10,
        dock: false,
        panelGroup: 'none',
        // 目前固定 PNG；保留 settings 結構方便未來新增格式。
        defaultSettings: function defaultSettings() {
            return { format: 'png' };
        },
        // 建立外露的 Export PNG 按鈕；export 進行中同一顆按鈕變成取消。
        buildAction: function buildAction(context) {
            var label = app.utils.dom.el('span', { text: ui.t('actionExport') });
            var exportButton = app.utils.dom.el('button', {
                className: 'primary-button export-button button-with-icon',
                attrs: { type: 'button' },
                children: [
                    ui.svgIcon('assets/icons/editor/export-download.svg'),
                    label
                ]
            });
            exportButton.addEventListener('click', function () {
                if (app.pages.ditherEditor.targetPolicy.isEpaper(context.controller.state)) {
                    context.controller.drawEpaper();
                    return;
                }
                if (context.controller.state.status === 'exporting') {
                    context.controller.cancelExport();
                } else {
                    context.controller.exportPng();
                }
            });
            actionRefs = { button: exportButton, label: label };
            return app.utils.dom.el('div', {
                className: 'export-action',
                children: [exportButton]
            });
        },
        // 依 status 同步按鈕文字與外觀：exporting 期間顯示 Cancel。
        onRender: function onRender(context) {
            if (!actionRefs) {
                return;
            }
            var exporting = context.state.status === 'exporting';
            var epaper = app.pages.ditherEditor.targetPolicy.isEpaper(context.state);
            var snapshot = app.device.epaper.snapshot();
            actionRefs.label.textContent = epaper
                ? (snapshot.cooldownRemainingSeconds
                    ? ui.t('epaperCooldownButton', { seconds: snapshot.cooldownRemainingSeconds })
                    : ui.t('actionDrawEpaper'))
                : (exporting ? ui.t('actionCancelExport') : ui.t('actionExport'));
            actionRefs.button.disabled = epaper && !app.device.epaper.canDraw();
            actionRefs.button.setAttribute('aria-disabled', actionRefs.button.disabled ? 'true' : 'false');
            actionRefs.button.classList.toggle('is-exporting', exporting);
        },
        dispose: function dispose() {
            actionRefs = null;
        }
    });
})(window.DitherApp);
