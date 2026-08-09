(function (app) {
    // ViewportRenderer 只管理預覽 canvas 的繪製。
    // 它不負責 pipeline、面板或儲存，讓預覽重繪可以獨立最佳化。
    // ViewportRenderer 先把 ImageData 或 crop transform 畫到 buffer，再提交到可見 canvas。
    // 它不知道 tool panel、pipeline 或 storage，避免 canvas 呈現層和業務狀態耦合。
    // 建立 renderer 並保存上一次繪製的 ImageData/transform，用於跳過重複渲染。
    function ViewportRenderer(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.bufferCanvas = app.core.canvasUtils.createCanvas(1, 1);
        this.bufferCtx = this.bufferCanvas.getContext('2d');
        this.sourceCanvas = app.core.canvasUtils.createCanvas(1, 1);
        this.sourceCtx = this.sourceCanvas.getContext('2d');
        this.lastImageData = undefined;
        this.lastFilter = '';
        this.lastTransformKey = '';
    }

    ViewportRenderer.prototype.setCanvasSize = function setCanvasSize(canvas, width, height) {
        width = Math.max(1, Math.round(width));
        height = Math.max(1, Math.round(height));
        if (canvas.width !== width || canvas.height !== height) {
            canvas.width = width;
            canvas.height = height;
        }
    };

    ViewportRenderer.prototype.prepareBuffer = function prepareBuffer(width, height) {
        this.setCanvasSize(this.bufferCanvas, width, height);
        this.bufferCtx.clearRect(0, 0, this.bufferCanvas.width, this.bufferCanvas.height);
        return this.bufferCtx;
    };

    ViewportRenderer.prototype.commitBuffer = function commitBuffer() {
        this.setCanvasSize(this.canvas, this.bufferCanvas.width, this.bufferCanvas.height);
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        this.ctx.drawImage(this.bufferCanvas, 0, 0);
    };

    ViewportRenderer.prototype.render = function render(imageData) {
        // 一般預覽先畫到 buffer；同一份資料重複要求時會跳過重繪。
        if (!imageData) {
            if (this.lastImageData === null) {
                return;
            }
            this.lastImageData = null;
            this.lastTransformKey = '';
            var emptyCtx = this.prepareBuffer(800, 480);
            emptyCtx.fillStyle = '#f7f9fa';
            emptyCtx.fillRect(0, 0, this.bufferCanvas.width, this.bufferCanvas.height);
            emptyCtx.fillStyle = '#6b7c88';
            emptyCtx.font = '18px system-ui';
            emptyCtx.textAlign = 'center';
            emptyCtx.fillText('No image loaded', this.bufferCanvas.width / 2, this.bufferCanvas.height / 2);
            this.commitBuffer();
            return;
        }
        if (this.lastImageData === imageData && this.lastTransformKey === '') {
            // 同一張 ImageData 且沒有 transform 時可跳過重畫，降低 slider 操作時的無效 render。
            return;
        }
        this.lastImageData = imageData;
        this.lastTransformKey = '';
        this.prepareBuffer(imageData.width, imageData.height).putImageData(imageData, 0, 0);
        this.commitBuffer();
    };

    ViewportRenderer.prototype.renderTransformed = function renderTransformed(imageData, settings, layout) {
        // Crop 模式下 canvas 顯示的是「原圖被移動、縮放、旋轉後」的預覽。
        // 裁切框由 overlay renderer 負責，所以這裡只處理影像本身的 transform。
        layout = layout || app.pages.ditherEditor.crop.previewLayout(imageData, settings);
        var backgroundColor = app.pages.ditherEditor.crop.backgroundColor(settings, imageData);
        // transformKey 包含所有會影響 crop preview 的設定；漏掉 flip/rotation 會造成 UI 不更新。
        var transformKey = [
            settings.aspectRatioId,
            settings.panX,
            settings.panY,
            settings.zoom,
            settings.rotation,
            settings.flipX,
            settings.flipY,
            backgroundColor,
            layout.width,
            layout.height
        ].join('|');
        if (this.lastImageData === imageData && this.lastTransformKey === transformKey) {
            return;
        }
        this.lastImageData = imageData;
        this.lastTransformKey = transformKey;

        // putImageData 不能直接參與 rotate/scale，先轉成暫存 canvas 再 drawImage。
        this.setCanvasSize(this.sourceCanvas, imageData.width, imageData.height);
        this.sourceCtx.putImageData(imageData, 0, 0);

        var ctx = this.prepareBuffer(layout.width, layout.height);
        var frame = layout.frame || { x: 0, y: 0, width: layout.width, height: layout.height };
        ctx.fillStyle = backgroundColor;
        ctx.fillRect(frame.x, frame.y, frame.width, frame.height);
        ctx.save();
        ctx.translate(
            frame.x + frame.width / 2 + Number(settings.panX || 0),
            frame.y + frame.height / 2 + Number(settings.panY || 0)
        );
        // Transform 順序需與正式 crop operation 一致：中心 -> pan -> rotation -> signed scale -> draw source。
        ctx.rotate(Number(settings.rotation || 0) * Math.PI / 180);
        ctx.scale(
            settings.flipX ? -Number(settings.zoom || 1) : Number(settings.zoom || 1),
            settings.flipY ? -Number(settings.zoom || 1) : Number(settings.zoom || 1)
        );
        ctx.drawImage(this.sourceCanvas, -imageData.width / 2, -imageData.height / 2);
        ctx.restore();
        this.commitBuffer();
    };

    ViewportRenderer.prototype.setFilter = function setFilter(filter) {
        // Adjust 拖曳中的即時預覽使用 CSS filter；正式結果仍由 pipeline 產出 ImageData。
        filter = filter || '';
        if (this.lastFilter === filter) {
            return;
        }
        this.lastFilter = filter;
        this.canvas.style.filter = filter;
    };

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.ViewportRenderer = ViewportRenderer;
})(window.DitherApp);
