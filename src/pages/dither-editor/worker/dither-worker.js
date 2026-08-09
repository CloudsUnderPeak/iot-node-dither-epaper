/* Dither worker：在背景執行緒執行 CPU 重的擴散類 dither，避免凍結 UI。
   只載入純運算模組（無 DOM）；ordered/pattern 類走主執行緒（GPU 或單趟 CPU 已夠快）。
   classic worker + importScripts，維持 no-build 與 file:// 相容策略（file:// 下
   Chrome 無法建立 worker 時由 client fallback 回主執行緒同步路徑）。 */
self.window = self;

importScripts(
    '../../../namespace.js',
    '../../../core/color/color-utils.js',
    '../../../core/color/palette-utils.js',
    '../config/color-distance-metrics.js',
    '../dither/dither-algorithm-registry.js',
    '../dither/dither-matrices.js',
    '../dither/palette-mapping.js',
    '../dither/error-diffusion.js',
    '../dither/dot-diffusion.js'
);

self.onmessage = function (event) {
    var request = event.data;
    try {
        var imageData = new ImageData(
            new Uint8ClampedArray(request.buffer),
            request.width,
            request.height
        );
        var output = self.DitherApp.pages.ditherEditor.ditherAlgorithmRegistry.run(
            imageData,
            request.algorithm,
            request.options
        );
        self.postMessage(
            {
                id: request.id,
                ok: true,
                width: output.width,
                height: output.height,
                buffer: output.data.buffer
            },
            [output.data.buffer]
        );
    } catch (error) {
        self.postMessage({
            id: request.id,
            ok: false,
            message: error && error.message ? error.message : String(error)
        });
    }
};
