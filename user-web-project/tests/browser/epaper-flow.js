(async function () {
    var passed = [];
    try {
        var timers = [], intervals = [], uploadCount = 0, refreshCount = 0, online = true;
        var status = {state: 'idle', can_draw: true, can_upload: true, last_operation: {result: 'success'}};
        var capability = {panel: {model: 'test', width: 2, height: 2, colors: 6, color_codes: [0,1,2,3,5,6]},
            image: {format: 'epdimg', header_bytes: 40, frame_bytes: 2, upload_bytes: 42,
                upload_uncompressed_bytes: 42, stored_encoding: 'gzip', upload_encodings: ['gzip'], max_compressed_bytes: 2088},
            capabilities: {upload: true, refresh: true}};
        var app = {core: {}, app: {state: {}}, pages: {}, i18n: {t: function (key) { return key; }},
            device: {live: {state: function () { return online ? 'online' : 'offline'; }, subscribe: function () { return function () {}; }},
                api: {resources: {epaperCapabilities: function () { return Promise.resolve(capability); },
                    epaperStatus: function () { refreshCount++; return Promise.resolve(status); },
                    epaperUpload: function (blob) { uploadCount++; return Promise.resolve({state: 'queued'}); },
                    epaperRefresh: function () { throw new Error('upload must not append refresh'); }}}}};
        var fakeWindow = {DitherApp: app, CompressionStream: window.CompressionStream,
            setTimeout: function (fn, delay) { timers.push({fn: fn, delay: delay}); return timers.length; },
            clearTimeout: function () {},
            setInterval: function (fn, delay) { intervals.push({fn: fn, delay: delay}); return intervals.length; },
            clearInterval: function () {}, performance: {now: function () { return 0; }}};
        var fakeDocument = {body: {classList: {toggle: function () {}}}};
        harness.execute('core/encoders/epaper-target.js', {window: fakeWindow});
        harness.execute('device/device-epaper.js', {window: fakeWindow, document: fakeDocument});
        await app.device.epaper.probe();
        var run = await app.device.epaper.beginOperation('upload');
        harness.assert(app.app.state.blockingOperation === 'epaper', 'preflight locks app');
        var busy = await app.device.epaper.beginOperation('upload').then(function () { return ''; }, function (error) { return error.code; });
        harness.assert(busy === 'epaper_busy', 'second mutation is busy');
        var submitted = app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target);
        for (var tick = 0; tick < 100 && uploadCount === 0; tick++) {
            await new Promise(function (resolve) { window.setTimeout(resolve, 10); });
        }
        harness.assert(uploadCount === 1, 'gzip upload sent exactly once');
        status = {state: 'queued', can_draw: false, can_upload: false};
        await app.device.epaper.refreshStatus();
        harness.assert(app.device.epaper.snapshot().operation.stage === 'queued', 'queued status advances progress');
        status = {state: 'drawing', phase: 'refreshing', can_draw: false, can_upload: false};
        await app.device.epaper.refreshStatus();
        harness.assert(app.device.epaper.snapshot().operation.stage === 'refreshing', 'drawing status advances phase');
        status = {state: 'cooldown', retry_after_seconds: 0, can_draw: false, can_upload: false,
            last_operation: {result: 'success'}};
        await app.device.epaper.refreshStatus();
        await submitted;
        timers.filter(function (item) { return item.delay === 700; }).pop().fn();
        harness.assert(!app.app.state.blockingOperation && uploadCount === 1, 'cooldown completes without second mutation');
        status = {state: 'idle', can_draw: true, can_upload: true, last_operation: {result: 'success'}};
        await app.device.epaper.refreshStatus();
        harness.assert(app.device.epaper.canDraw(), 'idle restores admission');
        passed.push('E-paper gzip upload, 202, queued, drawing, cooldown, idle and single mutation');

        app.device.api.resources.epaperUpload = function () {
            uploadCount++; var error = new Error('unauthorized'); error.code = 'unauthorized'; return Promise.reject(error);
        };
        run = await app.device.epaper.beginOperation('upload');
        var unauthorized = await app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target)
            .then(function () { return ''; }, function (error) { return error.code; });
        harness.assert(unauthorized === 'unauthorized' && !app.app.state.blockingOperation, '401 unlocks without retry');
        run = await app.device.epaper.beginOperation('upload');
        var cancelled = await app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target,
            function () { var error = new Error('cancelled'); error.code = 'job_cancelled'; throw error; })
            .then(function () { return ''; }, function (error) { return error.code; });
        harness.assert(cancelled === 'job_cancelled' && uploadCount === 2 && !app.app.state.blockingOperation,
            'cancel does not resend mutation and unlocks');
        passed.push('E-paper 401 and cancel failure cleanup');

        app.device.api.resources.epaperUpload = function () {
            uploadCount++;
            var error = new Error('upload timeout'); error.code = 'upload_timeout';
            return Promise.reject(error);
        };
        run = await app.device.epaper.beginOperation('upload');
        var timeout = await app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target)
            .then(function () { return ''; }, function (error) { return error.code; });
        harness.assert(timeout === 'upload_timeout' && !app.app.state.blockingOperation,
            'upload timeout clears operation lock without retry');
        var uploadsBeforeGzip = uploadCount;
        fakeWindow.CompressionStream = null;
        run = await app.device.epaper.beginOperation('upload');
        var gzipFailure = await app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target)
            .then(function () { return ''; }, function (error) { return error.code; });
        harness.assert(gzipFailure === 'gzip_unavailable' && uploadCount === uploadsBeforeGzip &&
            !app.app.state.blockingOperation, 'gzip failure never sends upload and unlocks');
        fakeWindow.CompressionStream = window.CompressionStream;
        passed.push('E-paper upload timeout and gzip failure cleanup');

        app.device.api.resources.epaperUpload = function () {
            uploadCount++; return Promise.resolve({state: 'queued'});
        };
        run = await app.device.epaper.beginOperation('upload');
        status = {state: 'queued', can_draw: false, can_upload: false};
        var disconnected = app.device.epaper.submitUpload(run, new Uint8Array(42), app.device.epaper.snapshot().target)
            .then(function () { return ''; }, function (error) { return error.code; });
        for (var wait = 0; wait < 100 && uploadCount === uploadsBeforeGzip; wait++) {
            await new Promise(function (resolve) { window.setTimeout(resolve, 10); });
        }
        await Promise.resolve(); await Promise.resolve();
        harness.assert(app.device.epaper.snapshot().operation.accepted, 'upload 202 is observed before disconnect');
        online = false;
        await app.device.epaper.refreshStatus();
        harness.assert(app.app.state.blockingOperation === 'epaper',
            'disconnect after 202 keeps operation locked until server status returns');
        online = true;
        status = {state: 'cooldown', retry_after_seconds: 0, can_draw: false, can_upload: false,
            last_operation: {result: 'failed', error_code: 'upload_timeout'}};
        await app.device.epaper.refreshStatus();
        var disconnectedCode = await disconnected;
        harness.assert(disconnectedCode === 'upload_timeout' && !app.app.state.blockingOperation,
            'reconnected failure status settles operation without another upload: ' + disconnectedCode);
        status = {state: 'idle', can_draw: true, can_upload: true, last_operation: {result: 'success'}};
        await app.device.epaper.refreshStatus();
        passed.push('E-paper disconnect, terminal failure and readiness recovery');
        document.getElementById('result').textContent = JSON.stringify({passed: passed});
    } catch (error) {
        document.getElementById('result').textContent = JSON.stringify({passed: passed, error: error.stack});
    }
})();
