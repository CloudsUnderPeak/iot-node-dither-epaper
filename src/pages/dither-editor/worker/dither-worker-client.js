(function (app) {
    // Dither worker client：主執行緒側的 worker 管理與 request/response 對應。
    // 任何失敗（file:// 無法建 worker、importScripts 失敗、runtime error）都會
    // 永久停用 worker 並讓呼叫端 fallback 回同步路徑，行為與無 worker 時一致。
    var WORKER_SCRIPT = 'src/pages/dither-editor/worker/dither-worker.js';
    var worker = null;
    var disabled = false;
    var nextRequestId = 1;
    var pending = {};

    function rejectAllPending(message) {
        Object.keys(pending).forEach(function (id) {
            var entry = pending[id];
            delete pending[id];
            entry.reject(new Error(message));
        });
    }

    function disableWorker(reason) {
        disabled = true;
        if (worker) {
            worker.terminate();
            worker = null;
        }
        rejectAllPending(reason || 'Dither worker is unavailable.');
    }

    function getWorker() {
        if (disabled) {
            return null;
        }
        if (worker) {
            return worker;
        }
        if (typeof Worker === 'undefined') {
            disabled = true;
            return null;
        }
        try {
            // Chrome/Edge 在 file:// 下會在這裡拋 SecurityError → 永久 fallback 同步路徑。
            worker = new Worker(WORKER_SCRIPT);
        } catch (error) {
            disabled = true;
            return null;
        }
        worker.onmessage = function (event) {
            var response = event.data;
            var entry = pending[response.id];
            if (!entry) {
                return;
            }
            delete pending[response.id];
            if (response.ok) {
                entry.resolve(new ImageData(
                    new Uint8ClampedArray(response.buffer),
                    response.width,
                    response.height
                ));
            } else {
                entry.reject(new Error(response.message || 'Dither worker failed.'));
            }
        };
        worker.onerror = function () {
            // importScripts 失敗或 worker 內未攔截錯誤：停用並讓 pending 走 fallback。
            disableWorker('Dither worker failed to load.');
        };
        return worker;
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.ditherWorkerClient = {
        // worker 可用時回傳 Promise<ImageData>；不可用時回傳 null，呼叫端走同步路徑。
        // 輸入 buffer 以複本 transfer，原 imageData 保持有效（stage cache 仍持有它）。
        run: function run(imageData, algorithm, options) {
            var target = getWorker();
            if (!target) {
                return null;
            }
            var id = nextRequestId;
            nextRequestId += 1;
            var buffer = imageData.data.slice().buffer;
            return new Promise(function (resolve, reject) {
                pending[id] = { resolve: resolve, reject: reject };
                target.postMessage({
                    id: id,
                    algorithm: algorithm,
                    options: options,
                    width: imageData.width,
                    height: imageData.height,
                    buffer: buffer
                }, [buffer]);
            });
        },
        // 頁面卸載時釋放 worker；之後再呼叫 run 會依需要重建。
        terminate: function terminate() {
            if (worker) {
                worker.terminate();
                worker = null;
            }
            rejectAllPending('Dither worker was terminated.');
        }
    };
})(window.DitherApp);
