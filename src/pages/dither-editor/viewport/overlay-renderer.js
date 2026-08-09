(function (app) {
    var PREVIEW_FRAME_INSET = 32;
    var NARROW_PREVIEW_FRAME_INSET = 16;
    var NARROW_SCREEN_QUERY = '(max-width: 920px)';

    // ViewportOverlayRenderer 管理 preview stage 的 canvas CSS 尺寸與 crop frame overlay。
    // 它不修改 editor state，讓 page.js 只負責呼叫與 DOM 組合。
    function ViewportOverlayRenderer(options) {
        options = options || {};
        this.previewStage = options.previewStage;
        this.canvas = options.canvas;
        this.cropOverlay = options.cropOverlay;
        this.cropOverlayLabel = options.cropOverlayLabel;
        this.timingLabel = options.timingLabel;
        this.prepareMode = options.prepareMode;
        this.editMode = options.editMode;
        this.pixelPreviewKey = '';
    }

    ViewportOverlayRenderer.prototype.shouldShowCropOverlay = function shouldShowCropOverlay(state) {
        return Boolean(state.sourceImageData && state.mode === this.prepareMode);
    };

    ViewportOverlayRenderer.prototype.previewFrameInset = function previewFrameInset(isNarrowScreen) {
        return isNarrowScreen ? NARROW_PREVIEW_FRAME_INSET : PREVIEW_FRAME_INSET;
    };

    ViewportOverlayRenderer.prototype.fitPreviewFrame = function fitPreviewFrame(width, height, stageRect, inset) {
        if (!width || !height || !stageRect.width || !stageRect.height) {
            return null;
        }
        var maxWidth = Math.max(1, stageRect.width - inset * 2);
        var maxHeight = Math.max(1, stageRect.height - inset * 2);
        var scale = Math.min(1, maxWidth / width, maxHeight / height);
        if (!Number.isFinite(scale) || scale <= 0) {
            return null;
        }
        return {
            scale: scale,
            width: width * scale,
            height: height * scale
        };
    };

    ViewportOverlayRenderer.prototype.previewStageBox = function previewStageBox() {
        var rect = this.previewStage.getBoundingClientRect();
        var style = window.getComputedStyle(this.previewStage);
        var borderX = (parseFloat(style.borderLeftWidth) || 0) + (parseFloat(style.borderRightWidth) || 0);
        var borderY = (parseFloat(style.borderTopWidth) || 0) + (parseFloat(style.borderBottomWidth) || 0);
        return {
            width: Math.max(1, rect.width - borderX),
            height: Math.max(1, rect.height - borderY)
        };
    };

    ViewportOverlayRenderer.prototype.cropDisplayMetrics = function cropDisplayMetrics(state) {
        // Crop overlay 的 CSS 尺寸與 canvas 內部尺寸分開計算。
        // overlay 固定代表輸出框；canvas 可以比 overlay 大，用來顯示旋轉/平移後的原圖脈絡。
        var baseLayout = app.pages.ditherEditor.crop.previewLayout(state.sourceImageData, state.settings.crop);
        var layout = {
            width: baseLayout.width,
            height: baseLayout.height,
            frame: {
                x: baseLayout.frame.x,
                y: baseLayout.frame.y,
                width: baseLayout.frame.width,
                height: baseLayout.frame.height,
                ratio: baseLayout.frame.ratio
            }
        };
        var stageRect = this.previewStageBox();
        var isNarrowScreen = window.matchMedia(NARROW_SCREEN_QUERY).matches;
        var frameFit = this.fitPreviewFrame(
            layout.frame.width,
            layout.frame.height,
            stageRect,
            this.previewFrameInset(isNarrowScreen)
        ) || { scale: 1 };
        var scale = frameFit.scale;
        var stageWidthInImageSpace = Math.max(1, stageRect.width) / scale;
        if (Number.isFinite(stageWidthInImageSpace) && stageWidthInImageSpace > layout.width) {
            layout.frame.x += (stageWidthInImageSpace - layout.width) / 2;
            layout.width = stageWidthInImageSpace;
        }
        var stageHeightInImageSpace = Math.max(1, stageRect.height) / scale;
        if (Number.isFinite(stageHeightInImageSpace) && stageHeightInImageSpace > layout.height) {
            layout.frame.y += (stageHeightInImageSpace - layout.height) / 2;
            layout.height = stageHeightInImageSpace;
        }

        return {
            layout: layout,
            stageRect: stageRect,
            scale: scale,
            isNarrowScreen: isNarrowScreen
        };
    };

    ViewportOverlayRenderer.prototype.previewDisplayMetrics = function previewDisplayMetrics(imageData) {
        if (!imageData || !this.previewStage) {
            return null;
        }
        var stageRect = this.previewStageBox();
        var isNarrowScreen = window.matchMedia(NARROW_SCREEN_QUERY).matches;
        return this.fitPreviewFrame(
            imageData.width,
            imageData.height,
            stageRect,
            this.previewFrameInset(isNarrowScreen)
        );
    };

    ViewportOverlayRenderer.prototype.updatePixelPreviewOverflow = function updatePixelPreviewOverflow(imageData) {
        var stageRect = this.previewStageBox();
        var overflowX = Boolean(imageData && imageData.width > stageRect.width);
        var overflowY = Boolean(imageData && imageData.height > stageRect.height);
        this.previewStage.classList.toggle('is-pixel-overflow-x', overflowX);
        this.previewStage.classList.toggle('is-pixel-overflow-y', overflowY);
        this.previewStage.classList.toggle('is-pixel-overflowing', overflowX || overflowY);
    };

    ViewportOverlayRenderer.prototype.centerPixelPreview = function centerPixelPreview(imageData) {
        var stageRect = this.previewStageBox();
        var key = [
            imageData.width,
            imageData.height,
            Math.round(stageRect.width),
            Math.round(stageRect.height)
        ].join('|');
        if (this.pixelPreviewKey === key) {
            return;
        }
        this.pixelPreviewKey = key;
        this.previewStage.scrollLeft = Math.max(
            0,
            (imageData.width - stageRect.width) / 2
        );
        this.previewStage.scrollTop = Math.max(
            0,
            (imageData.height - stageRect.height) / 2
        );
    };

    ViewportOverlayRenderer.prototype.updateCanvasDisplay = function updateCanvasDisplay(state, cropVisible, imageData) {
        if (!this.canvas || !this.previewStage) {
            return null;
        }
        var pixelPreview = Boolean(!cropVisible && imageData && state.viewMode === 'pixel');
        this.previewStage.classList.toggle('is-crop-preview', cropVisible);
        this.previewStage.classList.toggle('is-sized-preview', Boolean(cropVisible || imageData));
        this.previewStage.classList.toggle('is-pixel-preview', pixelPreview);
        if (!pixelPreview) {
            this.pixelPreviewKey = '';
            this.updatePixelPreviewOverflow(null);
            this.previewStage.scrollLeft = 0;
            this.previewStage.scrollTop = 0;
        }
        if (!cropVisible || !state.sourceImageData) {
            this.canvas.style.width = '';
            this.canvas.style.height = '';
            this.canvas.style.left = '';
            this.canvas.style.top = '';
            this.previewStage.style.height = '';
            if (pixelPreview) {
                this.canvas.style.width = imageData.width + 'px';
                this.canvas.style.height = imageData.height + 'px';
                this.updatePixelPreviewOverflow(imageData);
                this.centerPixelPreview(imageData);
                return null;
            }
            var previewMetrics = this.previewDisplayMetrics(imageData);
            if (previewMetrics) {
                this.canvas.style.width = previewMetrics.width + 'px';
                this.canvas.style.height = previewMetrics.height + 'px';
            }
            return null;
        }

        this.previewStage.style.height = '';
        var metrics = this.cropDisplayMetrics(state);
        var width = metrics.layout.width * metrics.scale;
        var height = metrics.layout.height * metrics.scale;
        this.canvas.style.width = width + 'px';
        this.canvas.style.height = height + 'px';
        this.canvas.style.left = ((metrics.stageRect.width - width) / 2) + 'px';
        this.canvas.style.top = ((metrics.stageRect.height - height) / 2) + 'px';
        return metrics;
    };

    ViewportOverlayRenderer.prototype.holdPendingPreviewDisplay = function holdPendingPreviewDisplay() {
        this.previewStage.classList.add('is-crop-preview');
        this.previewStage.classList.add('is-sized-preview');
    };

    ViewportOverlayRenderer.prototype.renderCropOverlay = function renderCropOverlay(state, metrics, cropVisible) {
        if (!this.cropOverlay || !this.canvas || !cropVisible) {
            if (this.cropOverlay) {
                this.cropOverlay.hidden = true;
            }
            return;
        }

        var crop = state.settings.crop;
        metrics = metrics || this.cropDisplayMetrics(state);
        var frame = metrics.layout.frame;
        var stageRect = this.previewStage.getBoundingClientRect();
        var stageStyle = window.getComputedStyle(this.previewStage);
        var canvasRect = this.canvas.getBoundingClientRect();
        var borderLeft = parseFloat(stageStyle.borderLeftWidth) || 0;
        var borderTop = parseFloat(stageStyle.borderTopWidth) || 0;
        var scaleX = canvasRect.width / metrics.layout.width || metrics.scale;
        var scaleY = canvasRect.height / metrics.layout.height || metrics.scale;
        // overlay 必須跟 canvas 內的 crop frame 對齊；手機長圖時 canvas 可能溢出 stage，
        // 不能只用 stage 中央公式，否則畫面框選與正式 crop 取樣會偏移。
        this.cropOverlay.hidden = false;
        this.cropOverlay.style.left = (canvasRect.left - stageRect.left - borderLeft + frame.x * scaleX) + 'px';
        this.cropOverlay.style.top = (canvasRect.top - stageRect.top - borderTop + frame.y * scaleY) + 'px';
        this.cropOverlay.style.width = (frame.width * scaleX) + 'px';
        this.cropOverlay.style.height = (frame.height * scaleY) + 'px';
        this.cropOverlayLabel.textContent = [
            cropRatioLabel(crop),
            Math.round((crop.zoom || 1) * 100) + '%',
            Math.round(crop.rotation || 0) + 'deg'
        ].join(' | ');
    };

    // Preview timing label：定位貼齊 canvas 右下角（相對 stage），顯示 Rendering... 或耗時。
    ViewportOverlayRenderer.prototype.renderPreviewTimingLabel = function renderPreviewTimingLabel(state, cropVisible) {
        var label = this.timingLabel;
        if (!label || !this.canvas) {
            return;
        }
        var labelState = state.previewTimingLabel || {};
        var phase = labelState.phase;
        var durationMs = labelState.durationMs;
        var hasText = phase === 'rendering'
            || (phase === 'done' && Number.isFinite(durationMs));
        var visible = Boolean(
            app.pages.ditherEditor.constants.SHOW_PREVIEW_TIMING_LABEL === true
            && state.mode === this.editMode
            && state.viewMode === 'result'
            && !cropVisible
            && state.sourceImageData
            && !this.canvas.hidden
            && hasText
        );
        if (!visible) {
            label.hidden = true;
            return;
        }
        var stageRect = this.previewStage.getBoundingClientRect();
        var canvasRect = this.canvas.getBoundingClientRect();
        label.textContent = phase === 'rendering' ? app.i18n.t('previewRendering') : formatPreviewDuration(durationMs);
        label.style.right = Math.max(6, stageRect.right - canvasRect.right + 6) + 'px';
        label.style.bottom = Math.max(6, stageRect.bottom - canvasRect.bottom + 6) + 'px';
        label.hidden = false;
    };

    function formatPreviewDuration(durationMs) {
        if (durationMs < 1) {
            return '<1 ms';
        }
        if (durationMs < 1000) {
            return Math.round(durationMs) + ' ms';
        }
        return (durationMs / 1000).toFixed(1) + ' s';
    }

    // 將 crop 的 aspectRatioId 轉成使用者看得懂的比例文字。
    function cropRatioLabel(crop) {
        var ratios = app.pages.ditherEditor.crop && app.pages.ditherEditor.crop.ratios || [];
        var ratio = ratios.find(function (entry) {
            return entry.id === crop.aspectRatioId;
        });
        return ratio ? ratio.label : '';
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.ViewportOverlayRenderer = ViewportOverlayRenderer;
})(window.DitherApp);
