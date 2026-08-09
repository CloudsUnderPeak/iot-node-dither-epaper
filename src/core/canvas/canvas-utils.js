(function (app) {
    // Canvas helper 是 core 層唯一可以建立暫存 canvas 的地方。
    // 回傳值保持 ImageData / Blob / plain object，不保存任何頁面 canvas reference。
    var WHITE = { r: 255, g: 255, b: 255, a: 255 };

    // 建立指定尺寸 canvas；所有 ImageData 轉換都從這裡取得 canvas。
    function createCanvas(width, height) {
        var canvas = document.createElement('canvas');
        canvas.width = Math.max(1, Math.round(width));
        canvas.height = Math.max(1, Math.round(height));
        return canvas;
    }

    // 複製 ImageData，避免 pipeline operation 修改原始資料。
    function cloneImageData(imageData) {
        return new ImageData(
            new Uint8ClampedArray(imageData.data),
            imageData.width,
            imageData.height
        );
    }

    // 將透明像素合成到白底並把 alpha 固定為 255，方便後續演算法只處理 RGB。
    function normalizeAlpha(imageData) {
        var data = imageData.data;
        for (var i = 0; i < data.length; i += 4) {
            var alpha = data[i + 3] / 255;
            if (alpha < 1) {
                // MVP 不保留透明輸出；載入時先以白底合成，後續 pipeline 全部處理不透明 RGBA。
                data[i] = data[i] * alpha + WHITE.r * (1 - alpha);
                data[i + 1] = data[i + 1] * alpha + WHITE.g * (1 - alpha);
                data[i + 2] = data[i + 2] * alpha + WHITE.b * (1 - alpha);
                data[i + 3] = 255;
            }
        }
        return imageData;
    }

    app.core.canvasUtils = {
        createCanvas: createCanvas,
        cloneImageData: cloneImageData,
        normalizeAlpha: normalizeAlpha,
        // 將 HTMLImageElement 畫進 canvas 並取出 ImageData，可選擇限制長邊尺寸。
        imageToImageData: function imageToImageData(image, maxLongEdge) {
            var width = image.naturalWidth || image.width;
            var height = image.naturalHeight || image.height;
            // 載入階段限制工作圖長邊，避免後續 Dither / palette mapping 卡住主執行緒。
            var scale = Math.min(1, maxLongEdge / Math.max(width, height));
            var targetWidth = Math.max(1, Math.round(width * scale));
            var targetHeight = Math.max(1, Math.round(height * scale));
            var canvas = createCanvas(targetWidth, targetHeight);
            var ctx = canvas.getContext('2d', { willReadFrequently: true });
            ctx.drawImage(image, 0, 0, targetWidth, targetHeight);
            var imageData;
            try {
                imageData = ctx.getImageData(0, 0, targetWidth, targetHeight);
            } catch (error) {
                if (error && error.name === 'SecurityError') {
                    // SVG 或跨來源圖片可能污染 canvas；轉成帶 code 的產品錯誤，頁面層對應 i18n。
                    var blocked = new Error('Image could not be processed. Use a local PNG, JPEG, or WebP file without cross-origin content.');
                    blocked.code = 'image-processing-blocked';
                    throw blocked;
                }
                throw error;
            }
            return {
                imageData: normalizeAlpha(imageData),
                originalSize: { width: width, height: height },
                workingSize: { width: targetWidth, height: targetHeight },
                wasResized: scale < 1
            };
        },
        // 將 ImageData 轉成 PNG Blob，供下載流程使用。
        imageDataToBlob: function imageDataToBlob(imageData) {
            return new Promise(function (resolve) {
                var canvas = createCanvas(imageData.width, imageData.height);
                var ctx = canvas.getContext('2d');
                ctx.putImageData(imageData, 0, 0);
                canvas.toBlob(resolve, 'image/png');
            });
        },
        // 透過 canvas drawImage 重採樣 ImageData。
        resizeImageData: function resizeImageData(imageData, width, height) {
            var source = createCanvas(imageData.width, imageData.height);
            var sourceCtx = source.getContext('2d');
            var target = createCanvas(width, height);
            var targetCtx = target.getContext('2d', { willReadFrequently: true });
            sourceCtx.putImageData(imageData, 0, 0);
            targetCtx.imageSmoothingEnabled = true;
            targetCtx.imageSmoothingQuality = 'high';
            targetCtx.drawImage(source, 0, 0, target.width, target.height);
            return targetCtx.getImageData(0, 0, target.width, target.height);
        }
    };
})(window.DitherApp);
