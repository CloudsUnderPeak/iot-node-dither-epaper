(function (app) {
    // Image Input feature 是所有影像來源的入口：本機檔案、拖放、demo 與 Empty 畫布 dropzone。
    // 真正的解碼與格式驗證在 core.imageLoader，避免 UI 繞過規則。
    var ui = app.pages.ditherEditor.panelUtils;

    app.pages.ditherEditor.featureRegistry.register({
        id: 'input',
        icon: '+',
        iconPath: 'assets/icons/editor/image-input.svg',
        labelKey: 'panelImageInput',
        dockOrder: 10,
        panelGroup: 'source',
        // 建立圖片輸入面板，所有輸入來源最後都交給 controller。
        buildPanel: function buildPanel(context) {
            var controller = context.controller;
            var file = app.utils.dom.el('input', {
                className: 'image-input-file',
                attrs: {
                    type: 'file',
                    accept: app.core.imageLoader.acceptedImageTypes,
                    id: 'image-file-input',
                    tabindex: '-1'
                }
            });
            file.addEventListener('change', function () {
                var selected = file.files[0];
                file.value = '';
                if (selected) {
                    controller.loadFile(selected);
                }
            });

            var newButton = app.utils.dom.el('button', {
                className: 'secondary-button button-with-icon',
                attrs: { type: 'button' },
                children: [
                    ui.svgIcon('assets/icons/editor/new-image-stars.svg'),
                    app.utils.dom.el('span', { text: ui.t('actionNewImage') })
                ]
            });
            newButton.addEventListener('click', function () {
                file.click();
            });

            var demoButton = app.utils.dom.el('button', {
                className: 'secondary-button button-with-icon',
                attrs: { type: 'button' },
                children: [
                    ui.svgIcon('assets/icons/editor/load-demo-image.svg'),
                    app.utils.dom.el('span', { text: ui.t('actionLoadDemo') })
                ]
            });
            demoButton.addEventListener('click', function () {
                controller.loadDemo();
            });

            return ui.section('panelImageInput', [
                file,
                app.utils.dom.el('div', {
                    className: 'button-row image-input-actions',
                    children: [newButton, demoButton]
                })
            ], 'input');
        },
        // Empty 畫布中央的上傳 dropzone：input feature 擁有所有影像進入點。
        // page.js 只負責掛載與依 empty 狀態切換顯示。
        buildEmptyUpload: function buildEmptyUpload(context) {
            var controller = context.controller;

            function loadDroppedFile(file) {
                if (!file || !app.pages.ditherEditor.editorModeStateMachine.canUseSource(controller.state)) {
                    return;
                }
                controller.loadFile(file);
            }

            var fileInput = app.utils.dom.el('input', {
                className: 'preview-upload-file-input',
                attrs: {
                    type: 'file',
                    accept: app.core.imageLoader.acceptedImageTypes,
                    tabindex: '-1'
                }
            });
            fileInput.addEventListener('change', function () {
                var selected = fileInput.files[0];
                fileInput.value = '';
                loadDroppedFile(selected);
            });
            var browseButton = app.utils.dom.el('button', {
                className: 'secondary-button preview-upload-button',
                text: ui.t('actionBrowseFile'),
                attrs: { type: 'button' }
            });
            browseButton.addEventListener('click', function () {
                fileInput.click();
            });
            var dropzone = app.utils.dom.el('div', {
                className: 'preview-upload-dropzone',
                attrs: { hidden: 'hidden' },
                children: [
                    fileInput,
                    app.utils.dom.el('span', {
                        className: 'preview-upload-icon',
                        attrs: { 'aria-hidden': 'true' },
                        children: [
                            ui.svgIcon('assets/icons/editor/upload-share.svg', { fallbackText: '↑' })
                        ]
                    }),
                    app.utils.dom.el('div', { className: 'preview-upload-title', text: ui.t('uploadDropTitle') }),
                    app.utils.dom.el('div', { className: 'preview-upload-separator', text: ui.t('uploadDropSeparator') }),
                    browseButton
                ]
            });
            dropzone.addEventListener('dragover', function (event) {
                event.preventDefault();
                dropzone.classList.add('is-over');
            });
            dropzone.addEventListener('dragleave', function (event) {
                if (!dropzone.contains(event.relatedTarget)) {
                    dropzone.classList.remove('is-over');
                }
            });
            dropzone.addEventListener('drop', function (event) {
                event.preventDefault();
                dropzone.classList.remove('is-over');
                var dropped = event.dataTransfer && event.dataTransfer.files[0];
                loadDroppedFile(dropped);
            });
            return dropzone;
        }
    });
})(window.DitherApp);
