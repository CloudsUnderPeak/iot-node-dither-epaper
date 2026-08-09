(function (app) {
    // Preview toolbar：edit 模式的檢視切換（Original/Result/Expand）與
    // prepare 模式的 crop zoom/OK 控制列。page.js 只負責掛載與呼叫 render。
    function t(key) {
        return app.i18n.t(key);
    }

    function createPreviewToolbar(controller) {
        var ui = app.pages.ditherEditor.panelUtils;
        var machine = app.pages.ditherEditor.editorModeStateMachine;

        function viewModeInput(value) {
            var input = app.utils.dom.el('input', {
                attrs: { type: 'radio', name: 'preview-view-mode', value: value }
            });
            input.addEventListener('change', function () {
                if (input.checked) {
                    controller.setViewMode(value);
                }
            });
            return input;
        }

        function viewModeChoice(input, labelKey) {
            return app.utils.dom.el('label', {
                className: 'setting-choice preview-view-choice',
                children: [input, app.utils.dom.el('span', { text: t(labelKey) })]
            });
        }

        function adjustCropZoom(delta) {
            if (controller.state.mode !== machine.groups.PREPARE) {
                return;
            }
            controller.updateSetting('crop', 'zoom', Number(controller.state.settings.crop.zoom || 1) + delta);
        }

        var originalInput = viewModeInput('original');
        var resultInput = viewModeInput('result');
        var pixelInput = viewModeInput('pixel');

        var cropZoomInButton = app.utils.dom.el('button', {
            className: 'secondary-button crop-zoom-button',
            attrs: { type: 'button', title: t('actionCropZoomIn'), 'aria-label': t('actionCropZoomIn') },
            children: [ui.svgIcon('assets/icons/editor/zoom-in.svg', { fallbackText: '+' })]
        });
        cropZoomInButton.addEventListener('click', function () {
            adjustCropZoom(0.1);
        });
        var cropZoomOutButton = app.utils.dom.el('button', {
            className: 'secondary-button crop-zoom-button',
            attrs: { type: 'button', title: t('actionCropZoomOut'), 'aria-label': t('actionCropZoomOut') },
            children: [ui.svgIcon('assets/icons/editor/zoom-out.svg', { fallbackText: '-' })]
        });
        cropZoomOutButton.addEventListener('click', function () {
            adjustCropZoom(-0.1);
        });
        var cropOkButton = app.utils.dom.el('button', {
            className: 'primary-button crop-ok-button',
            text: t('actionCropOk'),
            attrs: { type: 'button' }
        });
        cropOkButton.addEventListener('click', function () {
            controller.closePrepareMode();
        });

        var cropControlRow = app.utils.dom.el('div', {
            className: 'button-row',
            children: [cropZoomInButton, cropZoomOutButton, cropOkButton]
        });
        var previewToggleRow = app.utils.dom.el('div', {
            className: 'setting-choice-list preview-view-choice-list',
            children: [
                viewModeChoice(originalInput, 'previewOriginal'),
                viewModeChoice(resultInput, 'previewResult'),
                viewModeChoice(pixelInput, 'previewExpand')
            ]
        });
        var element = app.utils.dom.el('div', {
            className: 'preview-toolbar',
            children: [previewToggleRow, cropControlRow]
        });

        // 依 mode 決定顯示 crop 控制列或檢視切換列；非兩者時保留高度避免版面跳動。
        function render(state) {
            var isPrepareMode = state.mode === machine.groups.PREPARE;
            var isEditMode = state.mode === machine.groups.EDIT;
            var shouldReserveToolbar = Boolean(state.sourceImageData && !isPrepareMode && !isEditMode);
            element.hidden = !isPrepareMode && !isEditMode && !shouldReserveToolbar;
            element.classList.toggle('is-reserved', shouldReserveToolbar);
            element.setAttribute('aria-hidden', shouldReserveToolbar ? 'true' : 'false');
            cropControlRow.hidden = !isPrepareMode;
            previewToggleRow.hidden = !isEditMode;
            cropZoomInButton.disabled = !isPrepareMode;
            cropZoomOutButton.disabled = !isPrepareMode;
            cropOkButton.disabled = !isPrepareMode;
            originalInput.checked = state.viewMode === 'original';
            resultInput.checked = state.viewMode === 'result';
            pixelInput.checked = state.viewMode === 'pixel';
            originalInput.disabled = !isEditMode;
            resultInput.disabled = !isEditMode;
            pixelInput.disabled = !isEditMode;
        }

        return {
            element: element,
            render: render
        };
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.createPreviewToolbar = createPreviewToolbar;
})(window.DitherApp);
