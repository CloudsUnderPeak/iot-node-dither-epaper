(function (app) {
    var WORKER_SCRIPT = 'src/pages/dither-editor/worker/dither-worker.js';

    function clientError(code, reason) {
        var error = new Error(reason);
        error.code = code;
        error.reason = reason;
        return error;
    }

    function createClient() {
        var worker = null;
        var disabled = false;
        var epoch = 0;
        var nextId = 1;
        var pending = null;

        function stop(error, disable) {
            epoch += 1;
            disabled = Boolean(disable);
            if (worker) { worker.terminate(); worker = null; }
            if (pending) {
                var entry = pending;
                pending = null;
                entry.reject(error);
            }
        }

        function getWorker() {
            if (disabled || typeof Worker === 'undefined') { return null; }
            if (worker) { return worker; }
            try { worker = new Worker(WORKER_SCRIPT); }
            catch (error) { disabled = true; return null; }
            var currentEpoch = ++epoch;
            worker.onmessage = function (event) {
                if (currentEpoch !== epoch || !pending || !event.data || event.data.id !== pending.id) { return; }
                var entry = pending;
                pending = null;
                try {
                    var response = event.data;
                    if (!response.ok) { throw clientError('worker_failure', response.message || 'Worker failed.'); }
                    entry.resolve(new ImageData(new Uint8ClampedArray(response.buffer), response.width, response.height));
                } catch (error) { entry.reject(error); }
            };
            function failure() {
                if (currentEpoch === epoch) { stop(clientError('worker_failure', 'Worker failed.'), true); }
            }
            worker.onerror = failure;
            worker.onmessageerror = failure;
            return worker;
        }

        return {
            run: function (imageData, algorithm, options, job) {
                if (job) { job.check(); }
                // Admission precedes allocation/transfer; no hidden worker queue.
                if (pending) { return Promise.reject(clientError('job_cancelled', 'superseded')); }
                var target = getWorker();
                if (!target) { return null; }
                var id = nextId++;
                return new Promise(function (resolve, reject) {
                    pending = { id: id, resolve: resolve, reject: reject };
                    try {
                        var buffer = imageData.data.slice().buffer;
                        target.postMessage({ id: id, algorithm: algorithm, options: options,
                            width: imageData.width, height: imageData.height, buffer: buffer }, [buffer]);
                    } catch (error) { stop(clientError('worker_failure', error.message), true); }
                });
            },
            terminate: function (reason) { stop(clientError('job_cancelled', reason || 'explicit'), false); },
            pendingCount: function () { return pending ? 1 : 0; }
        };
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    // Standalone tools retain the classic-script adapter; controllers own separate clients.
    app.pages.ditherEditor.ditherWorkerClient = createClient();
    app.pages.ditherEditor.ditherWorkerClient.create = createClient;
})(window.DitherApp);
