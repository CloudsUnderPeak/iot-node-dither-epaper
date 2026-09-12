(function (app) {
    // Version 1 persisted values retain the original strict workspace contract.
    function validPersistedSettings(value) {
            return value.format === 'png';
    }

    // Export feature 是 action，不是可拖曳的 pipeline tool。
    // 它使用目前 preview/result ImageData 直接輸出 PNG，不在面板中折疊。
    var ui = app.pages.ditherEditor.panelUtils;
    var actionRefs = null;

    app.pages.ditherEditor.featureRegistry.register({
        id: 'export',
        persistence: {
            version: 1,
            serializeSettings: function (settings) { return JSON.parse(JSON.stringify(settings)); },
            restoreSettings: function (value, version) {
                if (version !== 1 || !value || typeof value !== 'object' || Array.isArray(value) || !validPersistedSettings(value)) {
                    var error = new Error('Invalid export project settings.');
                    error.code = 'settings-invalid';
                    throw error;
                }
                return JSON.parse(JSON.stringify(value));
            }
        },
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
        // Standalone 只顯示 Export PNG；E-paper 顯示繪製與圖片專案下載兩個主動作。
        buildAction: function buildAction(context) {
            var label = app.utils.dom.el('span', { text: ui.t('actionExport') });
            var icon = ui.svgIcon('assets/icons/editor/export-download.svg');
            var exportButton = app.utils.dom.el('button', {
                className: 'primary-button export-button button-with-icon',
                attrs: { type: 'button' },
                children: [
                    icon,
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
            var projectLabel = app.utils.dom.el('span', { text: ui.t('actionExportImageProject') });
            var projectButton = app.utils.dom.el('button', {
                className: 'primary-button export-button button-with-icon',
                attrs: { type: 'button', hidden: 'hidden' },
                children: [
                    ui.svgIcon('assets/icons/editor/export-download.svg'),
                    projectLabel
                ]
            });
            projectButton.addEventListener('click', function () {
                context.controller.exportProject();
            });
            actionRefs = {
                button: exportButton,
                label: label,
                icon: icon,
                projectButton: projectButton,
                projectLabel: projectLabel
            };
            return app.utils.dom.el('div', {
                className: 'export-action',
                children: [exportButton, projectButton]
            });
        },
        // 依 status 同步按鈕文字與外觀：exporting 期間顯示 Cancel。
        onRender: function onRender(context) {
            if (!actionRefs) {
                return;
            }
            var exporting = context.state.status === 'exporting';
            var exportingProject = context.state.status === 'exporting-project';
            var epaper = app.pages.ditherEditor.targetPolicy.isEpaper(context.state);
            var actionAllowed = app.pages.ditherEditor.editorModeStateMachine.canUseAction(context.state, 'export');
            var snapshot = app.device.epaper.snapshot();
            actionRefs.label.textContent = epaper
                ? (snapshot.cooldownRemainingSeconds
                    ? ui.t('epaperCooldownButton', { seconds: snapshot.cooldownRemainingSeconds })
                    : ui.t('actionDrawEpaper'))
                : (exporting ? ui.t('actionCancelExport') : ui.t('actionExport'));
            var desiredIcon = epaper
                ? 'assets/icons/editor/credit-card-edit-svgrepo-com.svg'
                : 'assets/icons/editor/export-download.svg';
            var iconImage = actionRefs.icon.querySelector('img');
            if (iconImage) {
                iconImage.src = app.ui.svgIcons.iconUrl(desiredIcon);
            }
            actionRefs.button.disabled = !actionAllowed || exportingProject
                || (epaper && !app.device.epaper.canDraw());
            actionRefs.button.setAttribute('aria-disabled', actionRefs.button.disabled ? 'true' : 'false');
            actionRefs.button.classList.toggle('is-exporting', exporting);
            actionRefs.projectButton.hidden = !epaper;
            actionRefs.projectButton.disabled = !epaper || !actionAllowed || exportingProject;
            actionRefs.projectButton.setAttribute('aria-disabled', actionRefs.projectButton.disabled ? 'true' : 'false');
            actionRefs.projectLabel.textContent = ui.t('actionExportImageProject');
        },
        dispose: function dispose() {
            actionRefs = null;
        }
    });
})(window.DitherApp);
