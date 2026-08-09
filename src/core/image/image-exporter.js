(function (app) {
    // 匯出目前固定為 PNG。輸出只接收已完成 pipeline 的 ImageData，
    // 不在這裡重新跑任何 operation，避免 exporter 知道 editor state。
    app.core.imageExporter = {
        // 將目前結果轉成 PNG 並觸發下載。
        exportPng: function exportPng(imageData, filename) {
            return app.core.canvasUtils.imageDataToBlob(imageData).then(function (blob) {
                var url = URL.createObjectURL(blob);
                var anchor = document.createElement('a');
                anchor.href = url;
                anchor.download = filename || 'dither-output.png';
                anchor.click();
                setTimeout(function () {
                    URL.revokeObjectURL(url);
                }, 0);
                return blob;
            });
        }
    };
})(window.DitherApp);
