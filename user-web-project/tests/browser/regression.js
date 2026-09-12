(async function () {
    var passed = [];
    var editor = { operationRegistry: { get: function () { return { run: function (image) { return image; } }; } } };
    harness.execute('pages/dither-editor/operations/pipeline-runner.js', { window: { DitherApp: { pages: { ditherEditor: editor } } } });
    var input = new ImageData(1, 1);
    var state = { pipeline: { fixedBefore: [], effectsOrder: ['test'], fixedAfter: [], enabled: {} }, settings: { test: {} } };
    var output = await editor.pipelineRunner.runAsync(input, state);
    harness.assert(output === input, 'Real pipeline must preserve no-op identity');
    passed.push('W00 real pipeline startup');
    var runner = editor.pipelineRunner;
    var cache = runner.createStageCache({ maxBytes: 16, maxEntries: 3 });
    var next;
    editor.operationRegistry.get = function () { return { run: function () { return next; } }; };
    function put(value, pixels) {
        state.settings.test = { value: value };
        next = pixels;
        var result = runner.run(input, state, { stageCache: cache });
        harness.assert(cache.retainedBytes <= cache.maxBytes && cache.entries.size <= cache.maxEntries, 'cache bounds');
        return result;
    }
    var backing = new ArrayBuffer(12);
    var alias = { width: 1, height: 1, data: new Uint8ClampedArray(backing, 0, 4) };
    put(1, alias);
    put(2, { width: 1, height: 1, data: new Uint8ClampedArray(backing, 4, 4) });
    harness.assert(cache.retainedBytes === 12 && cache.buffers.get(backing) === 2, 'alias and partial backing accounting');
    put(3, new ImageData(1, 1));
    var oversized = new ImageData(5, 1);
    harness.assert(put(4, oversized) === oversized && cache.entries.size === 3, 'oversized output preserves small entries');
    put(1, alias); // LRU hit
    put(5, new ImageData(1, 1));
    harness.assert(cache.entries.size === 2 && cache.retainedBytes === 16 && cache.buffers.get(backing) === 1, 'LRU eviction retains recently hit alias');
    runner.clearStageCache(cache);
    harness.assert(cache.retainedBytes === 0 && cache.buffers.size === 0 && cache.entries.size === 0, 'clear accounting');
    cache.maxBytes = 0;
    put(6, input);
    harness.assert(cache.entries.size === 0, 'zero budget');
    cache.maxBytes = 16;
    var delayed = harness.deferred();
    editor.operationRegistry.get = function () { return { run: function () { return delayed.promise; } }; };
    var late = runner.runAsync(input, state, { stageCache: cache });
    runner.clearStageCache(cache);
    delayed.resolve(input);
    harness.assert(await late === input && cache.retainedBytes === 0, 'clear fences late async write');
    passed.push('W01 byte/entry budgets, aliases, partial views, LRU, oversize, clear generation');
    // Execute the application's real classic-script manifests, replacing only script IO.
    function loadModule(path) { harness.execute(path.replace(/^src[/]/, ''), { window: window }); }
    testEntryPaths.slice(0, testEntryPaths.indexOf('src/pages/dither-editor/entry.js')).forEach(function (path) {
        if (path !== 'src/device/device-mock.js') { loadModule(path); }
    });
    var realApp = window.DitherApp;
    realApp.app.scriptLoader.loadMany = function (paths) {
        paths.forEach(loadModule);
        return Promise.resolve();
    };
    loadModule('src/pages/dither-editor/entry.js');
    await realApp.app.whenPageEntriesReady();
    var realEditor = realApp.pages.ditherEditor;
    cache = runner.createStageCache({ maxBytes: 16 });
    var replacements = [harness.deferred(), harness.deferred()], replacementIndex = 0;
    editor.operationRegistry.get = function () { return { run: function () { return replacements[replacementIndex++].promise; } }; };
    var replaceA = runner.runAsync(input, state, { stageCache: cache }), replaceB = runner.runAsync(input, state, { stageCache: cache });
    replacements[0].resolve(input); await replaceA;
    replacements[1].resolve(alias); await replaceB;
    harness.assert(cache.entries.size === 1 && cache.retainedBytes === 12 && cache.buffers.size === 1, 'same-key replacement accounting');
    var feature, fallbackRuns = 0;
    var workEditor = {
        panelUtils: {}, constants: { DEFAULT_DITHER_ERROR_STRENGTH: 100, MIN_DITHER_ERROR_STRENGTH: 0, MAX_DITHER_ERROR_STRENGTH: 150 },
        featureRegistry: { register: function (f) { feature = f; }, api: function () { return { getActivePalette: function () { return [{ r: 0, g: 0, b: 0 }, { r: 255, g: 255, b: 255 }]; } }; } },
        paletteMapping: { normalizeId: function (x) { return x; } },
        ditherAlgorithmRegistry: { get: realEditor.ditherAlgorithmRegistry.get, run: function () { fallbackRuns++; return realEditor.ditherAlgorithmRegistry.run.apply(null, arguments); } }
    };
    var workApp = { pages: { ditherEditor: workEditor }, core: { paletteUtils: { normalizeColorDistanceId: function (x) { return x; } } } };
    var workWindow = { DitherApp: workApp };
    harness.execute('pages/dither-editor/worker/dither-worker-client.js', { window: workWindow, Worker: harness.FakeWorker });
    harness.execute('pages/dither-editor/features/dither-feature.js', { window: workWindow });
    harness.execute('pages/dither-editor/controller.js', { window: workWindow });
    var client = workEditor.ditherWorkerClient.create();
    var pending = feature.operation.run(input, { algorithm: 'floyd-steinberg' }, { state: {}, workerClient: client });
    var cancelled = pending.catch(function (error) { return error.code; });
    client.terminate('disposed');
    harness.assert(await cancelled === 'job_cancelled' && fallbackRuns === 0 && client.pendingCount() === 0, 'cancel never falls back');
    var failed = feature.operation.run(input, { algorithm: 'floyd-steinberg' }, { state: {}, workerClient: client });
    var oldWorker = harness.FakeWorker.instances[harness.FakeWorker.instances.length - 1];
    oldWorker.onmessageerror();
    harness.assert((await failed).data.length === input.data.length && fallbackRuns === 1 && client.pendingCount() === 0, 'runtime failure fallback exactly once');
    client.terminate();
    var again = client.run(input, {}, {});
    var newWorker = harness.FakeWorker.instances[harness.FakeWorker.instances.length - 1];
    oldWorker.onerror();
    harness.assert(client.pendingCount() === 1 && !newWorker.terminated, 'old epoch cannot affect new worker');
    var response = { data: { id: newWorker.messages[0].message.id, ok: true, width: 1, height: 1, buffer: new ArrayBuffer(4) } };
    newWorker.onmessage(response); newWorker.onmessage(response);
    await again;
    harness.assert(client.pendingCount() === 0 && input.data.byteLength === 4, 'response settles once, input remains attached');
    newWorker.postMessage = function () { throw new Error('clone failed'); };
    await client.run(input, {}, {}).catch(function () {});
    harness.assert(client.pendingCount() === 0, 'postMessage throw drains pending');
    harness.execute('pages/dither-editor/worker/dither-worker-client.js', { window: workWindow, Worker: function () { throw new Error('Worker unavailable'); } });
    var unavailable = workEditor.ditherWorkerClient.create();
    var cpuFallback = feature.operation.run(input, { algorithm: 'floyd-steinberg' }, { state: {}, workerClient: unavailable });
    harness.assert(cpuFallback.data.length === 4 && fallbackRuns === 2, 'constructor failure uses real CPU processor once');
    var owner = workEditor.createJobOwner(client);
    var barrier = harness.deferred(), entered = harness.deferred(), runs = [];
    var first = owner.preview(async function (job) { runs.push(0); entered.resolve(); await barrier.promise; job.check(); });
    await entered.promise;
    for (var n = 1; n <= 20; n++) {
        (function (value) { owner.preview(function () { runs.push(value); }); })(n);
    }
    harness.assert(owner.counts().active === 1 && owner.counts().pending === 1, 'bounded queue');
    barrier.resolve(); await first;
    harness.assert(JSON.stringify(runs) === '[0,20]', 'only first and latest preview execute');
    var heavyEntered = harness.deferred(), heavyDone = harness.deferred(), heavyRuns = 0;
    var heavy = owner.heavy(async function () { heavyRuns++; heavyEntered.resolve(); await heavyDone.promise; });
    await heavyEntered.promise;
    await owner.heavy(function () { heavyRuns++; });
    heavyDone.resolve(); await heavy;
    harness.assert(heavyRuns === 1, 'heavy admission rejects duplicates');
    owner.dispose();
    await owner.preview(function () { throw new Error('disposed owner ran'); });
    passed.push('W02 cancellation, failure, worker epoch/settlement, transfer copy, bounded jobs');
    var waiting = [], started = harness.deferred();
    function decode() { var d = harness.deferred(); waiting.push(d); started.resolve(); return d.promise; }
    var originalDemoLoader = realApp.core.imageLoader.loadDemoImage;
    realApp.core.imageLoader.loadImageFromFile = decode;
    realApp.core.imageLoader.loadDemoImage = decode;
    var renders = 0;
    var controller = new realEditor.Controller({ render: function () { renders++; } });
    function result(name) { return { imageData: new ImageData(2, 2), fileName: name,
        originalSize: { width: 2, height: 2 }, workingSize: { width: 2, height: 2 } }; }
    async function begin(load) { started = harness.deferred(); var promise = load(); await started.promise; return { promise: promise, deferred: waiting[waiting.length - 1] }; }
    var loadA = await begin(function () { return controller.loadDemo(); });
    var loadB = await begin(function () { return controller.loadFile(new File(['image'], 'B.jpg', { type: 'image/jpeg' })); });
    loadB.deferred.resolve(result('B')); await loadB.promise;
    var current = controller.state;
    loadA.deferred.resolve(result('A')); await loadA.promise;
    harness.assert(controller.state === current && controller.state.fileName === 'B.jpg', 'late source cannot overwrite newest source');
    var lateFailure = await begin(function () { return controller.loadDemo(); });
    var latest = await begin(function () { return controller.loadDemo(); });
    latest.deferred.resolve(result('latest')); await latest.promise;
    var count = renders;
    lateFailure.deferred.reject(new Error('late')); await lateFailure.promise;
    harness.assert(controller.state.fileName === 'latest' && renders === count, 'late error cannot update UI');
    var destroyed = await begin(function () { return controller.loadDemo(); });
    controller.destroy(); count = renders;
    destroyed.deferred.resolve(result('disposed')); await destroyed.promise;
    harness.assert(renders === count && controller.state.fileName === 'latest', 'disposed source cannot render or commit');
    passed.push('W03 real classic-script modules/controller, file/demo inversion, late failure and dispose');
    var apiApp = { core: { storageKeys: { deviceToken: 'test-token' } }, device: {}, i18n: { t: function (x) { return x; } } };
    var transport = harness.controlledFetch();
    var apiWindow = { DitherApp: apiApp, setTimeout: function () { return 1; }, clearTimeout: function () {} };
    harness.execute('device/device-api.js', { window: apiWindow, localStorage: { getItem: function () { return ''; }, setItem: function () {}, removeItem: function () {} }, AbortController: undefined, fetch: transport });
    harness.execute('device/device-auth.js', { window: apiWindow });
    var api = apiApp.device.api, auth = apiApp.device.auth, notifications = 0;
    auth.subscribe(function () { notifications++; });
    function reply(index, status) { transport.requests[index].response.resolve({ ok: status === 200, status: status, json: function () { return Promise.resolve({ success: status === 200, data: {} }); } }); }
    api.setToken('first');
    var old = auth.ensureSession(); api.setToken('second'); reply(0, 401); await old;
    harness.assert(api.hasToken() && notifications === 0, 'old 401 preserves new session');
    var validOld = auth.ensureSession(); api.setToken('second'); reply(1, 200);
    harness.assert(await validOld === false, 'same token new epoch rejects old session success');
    var logout = auth.logout(); api.setToken('third'); reply(2, 200); await logout;
    harness.assert(api.hasToken() && notifications === 0, 'old logout preserves new session');
    var login = api.resources.login('test', 'test').catch(function () {}); reply(3, 401); await login;
    harness.assert(api.hasToken(), 'public login 401 preserves token');
    var current401 = auth.ensureSession(); reply(4, 401); await current401;
    harness.assert(!api.hasToken() && notifications === 1, 'current 401 clears and notifies once');
    api.setToken('transport');
    var offline = auth.ensureSession(); transport.requests[5].response.reject(new Error('offline')); await offline;
    harness.assert(api.hasToken(), 'transport failure preserves token');
    passed.push('W04 session epochs, stale/current 401, same-token rotation, logout and transport');
    apiApp.utils = realApp.utils; apiApp.ui = realApp.ui;
    function replyData(index, data) { transport.requests[index].response.resolve({ ok: true, status: 200, json: function () { return Promise.resolve({ success: true, data: data }); } }); }
    var successCallbacks = 0, loginFinished = harness.deferred();
    function submitDialog() {
        var form = document.querySelector('.device-dialog-form');
        form.querySelector('input[type="password"]').value = 'fixture';
        form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    }
    auth.openLoginDialog({ onSuccess: function () { successCallbacks++; loginFinished.resolve(); } });
    replyData(6, { username: 'admin' }); await Promise.resolve();
    submitDialog();
    realApp.ui.modal.close();
    auth.openLoginDialog({ onSuccess: function () { successCallbacks++; loginFinished.resolve(); } });
    replyData(8, { username: 'admin' }); await Promise.resolve();
    submitDialog();
    var epochBeforeOldLogin = api.sessionEpoch();
    replyData(7, { token: 'obsolete-login' });
    replyData(9, { token: 'current-login' });
    await loginFinished.promise;
    harness.assert(api.sessionEpoch() === epochBeforeOldLogin + 1, 'closed dialog cannot submit a late token');
    harness.assert(api.hasToken() && successCallbacks === 1 && !document.querySelector('.device-dialog-form'), 'current login owns dialog/token');
    passed.push('W04 actual modal close/reopen and out-of-order login submissions');

    function cssMeasurements(css) {
        var style = document.createElement('style'); style.textContent = css; document.head.appendChild(style);
        var host = document.createElement('div'); host.className = 'host';
        host.innerHTML = '<div class="probe active"></div><div class="sibling"></div>';
        document.body.appendChild(host);
        var probe = host.firstChild, computed = getComputedStyle(probe);
        var result = [computed.bottom, computed.padding, computed.width, computed.height,
            getComputedStyle(probe, '::before').content, getComputedStyle(host.lastChild).marginLeft, getComputedStyle(host).backgroundImage];
        host.remove(); style.remove(); return result;
    }
    var beforeCss = cssMeasurements(testCss[0]), afterCss = cssMeasurements(testCss[1]);
    harness.assert(JSON.stringify(beforeCss) === JSON.stringify(afterCss), 'CSS transformed computed styles');
    harness.assert(afterCss[0] === '108px' && afterCss[1] === '20px' && afterCss[3] === '17px', 'CSS fixture concrete math expectations');
    passed.push('W05 CSS computed styles: calc/variables/nesting/combinators/comments/strings/URL');
    if (testAssetCss) {
        function assetMeasurements(css) {
            var style = document.createElement('style'); style.textContent = css; document.head.appendChild(style);
            var tooltip = document.createElement('span'); tooltip.className = 'control-tip-icon'; tooltip.dataset.tooltip = 'text';
            var link = document.createElement('a'); link.className = 'help-nav-link'; link.style.setProperty('--help-nav-depth', '2');
            document.body.appendChild(tooltip); document.body.appendChild(link);
            var result = [getComputedStyle(tooltip, '::after').bottom, getComputedStyle(link).paddingLeft];
            tooltip.remove(); link.remove(); style.remove(); return result;
        }
        var sourceStyles = assetMeasurements(testAssetCss[0]), releaseStyles = assetMeasurements(testAssetCss[1]);
        harness.assert(JSON.stringify(sourceStyles) === JSON.stringify(releaseStyles) && releaseStyles[1] === '38px', 'actual gzip tooltip/help styles');
        passed.push('W05 actual production gzip tooltip/help computed styles, mock absent');
    }
    var memoryObservations = [];
    [ [800, 480], [2048, 2048] ].forEach(function (size) {
        cache = runner.createStageCache();
        editor.operationRegistry.get = function () { return { run: function () { return next; } }; };
        var startedAt = performance.now();
        for (var iteration = 0; iteration < 40; iteration++) { put(iteration, new ImageData(size[0], size[1])); }
        memoryObservations.push({ width: size[0], height: size[1], writes: 40, retainedBytes: cache.retainedBytes, entries: cache.entries.size, elapsedMs: performance.now() - startedAt });
        runner.clearStageCache(cache);
    });
    passed.push('W01 800x480 and 2048x2048 retained-buffer budget measurements');

    var coreOnly = { core: {} };
    harness.execute('core/color/color-utils.js', { window: { DitherApp: coreOnly } });
    harness.execute('core/color/palette-utils.js', { window: { DitherApp: coreOnly } });
    var normalize = coreOnly.core.paletteUtils.normalizeColorDistanceId;
    var truth = { bt709: 'euclidean-bt709', euclidean: 'euclidean-bt709', rgb: 'euclidean-rgb', manhattan: 'manhattan-rgb', invalid: 'euclidean-rgb' };
    coreOnly.core.paletteUtils.colorDistanceIds.forEach(function (id) { truth[id] = id; });
    Object.keys(truth).forEach(function (id) {
        harness.assert(normalize(id) === truth[id] && realApp.core.paletteUtils.normalizeColorDistanceId(id) === truth[id], 'core/page identity ' + id);
    });
    harness.assert(normalize(null) === 'euclidean-rgb' && normalize(undefined) === 'euclidean-rgb', 'invalid default');
    testColorGolden.forEach(function (golden) {
        var values = [[12, 83, 201, 244, 80, 19], [0, 0, 0, 255, 255, 255], [71, 199, 86, 80, 190, 100]];
        values.forEach(function (value, index) {
            harness.assert(coreOnly.core.paletteUtils.createRgbDistanceContext(golden.id).rgb.apply(null, value) === golden.distances[index], 'pre-refactor golden ' + golden.id);
        });
    });
    passed.push('W06 core without pages, complete ID/alias truth table, five metric golden values');
    var pngBytes = Uint8Array.from(atob(testProjectPng), function (ch) { return ch.charCodeAt(0); });
    var projectRoute = await realApp.core.projectFile.classify(new File([pngBytes], 'renamed.png', { type: 'image/png' }));
    var project = realApp.core.projectFile.read(projectRoute);
    // Actual decoder cleanup and original PNG chunks are exercised as well.
    var workingResult = await realApp.core.imageLoader.loadWorkingImage(project.workingBlob, 800);
    var restored = realEditor.projectWorkspace.restore(project, workingResult);
    var restoredPixels = await realEditor.pipelineRunner.runAsync(restored.sourceImageData, restored);
    harness.assert(JSON.stringify(Array.from(restoredPixels.data)) === JSON.stringify(testProjectGolden.pixels), 'old v1 PNG pixels');
    var exported = realEditor.projectWorkspace.createManifest(restored, restoredPixels);
    harness.assert(JSON.stringify(exported.editor.features) === JSON.stringify(testProjectGolden.manifest.editor.features), 'old v1 feature settings');
    var copied = realEditor.projectWorkspace.snapshot(restored);
    copied.settings.adjust.brightness = 99;
    harness.assert(restored.settings.adjust.brightness === 7, 'settings snapshots do not alias');
    function rejects(fn, label) { var threw = false; try { fn(); } catch (error) { threw = true; } harness.assert(threw, label); }
    Object.keys(project.manifest.editor.features).forEach(function (id) {
        var invalid = JSON.parse(JSON.stringify(project.manifest)); invalid.editor.features[id].settings = [];
        rejects(function () { realEditor.projectWorkspace.restore(Object.assign({}, project, { manifest: invalid }), workingResult); }, 'reject invalid feature ' + id);
        var contract = realEditor.featureRegistry.get(id).persistence;
        rejects(function () { contract.restoreSettings({}, 99); }, 'reject unsupported version ' + id);
    });
    var invalidManifest = JSON.parse(JSON.stringify(project.manifest));
    invalidManifest.editor.features.adjust.settings.brightness = Infinity;
    rejects(function () { realEditor.projectWorkspace.restore(Object.assign({}, project, { manifest: invalidManifest }), workingResult); }, 'reject nonfinite');
    var invalidSettings = [
        ['crop', 'zoom', 0], ['crop', 'rotation', 181], ['crop', 'width', NaN], ['crop', 'flipX', 'true'],
        ['resize', 'width', 4097], ['resize', 'height', 0], ['resize', 'aspectRatio', Infinity],
        ['adjust', 'brightness', -101], ['adjust', 'contrast', NaN], ['adjust', 'saturation', '0'],
        ['palette', 'palette', [{ r: 0, g: 0, b: 0 }]], ['palette', 'originalPaletteSize', 33],
        ['dither', 'algorithm', 'unknown'], ['dither', 'paletteMapping', 'unknown'], ['dither', 'colorDistance', 'rgb'],
        ['dither', 'errorStrength', 151], ['dither', 'serpentine', 1], ['export', 'format', 'jpeg']
    ];
    invalidSettings.forEach(function (entry) {
        var manifest = JSON.parse(JSON.stringify(project.manifest));
        manifest.editor.features[entry[0]].settings[entry[1]] = entry[2];
        rejects(function () { realEditor.projectWorkspace.restore(Object.assign({}, project, { manifest: manifest }), workingResult); }, 'invalid settings boundary ' + entry[0] + '.' + entry[1]);
    });
    [function (manifest) { manifest.editor.features.unknown = { version: 1, settings: {} }; },
        function (manifest) { delete manifest.editor.features.crop; },
        function (manifest) { manifest.editor.pipeline.fixedBefore[1] = manifest.editor.pipeline.fixedBefore[0]; },
        function (manifest) { manifest.editor.pipeline.enabled.unknown = true; },
        function (manifest) { manifest.editor.features.crop.settings.extra = new Array(65).fill(0); },
        function (manifest) { manifest.editor.features.crop.settings.extra = 'x'.repeat(4097); },
        function (manifest) { Object.defineProperty(manifest.editor.features.crop.settings, '__proto__', { value: {}, enumerable: true }); }
    ].forEach(function (mutate) {
        var manifest = JSON.parse(JSON.stringify(project.manifest)); mutate(manifest);
        rejects(function () { realEditor.projectWorkspace.restore(Object.assign({}, project, { manifest: manifest }), workingResult); }, 'common feature/pipeline/data protection');
    });
    passed.push('W07 every-feature type/enum/boundary/nonfinite and common manifest protections');

    // Project restore remains atomic and shares the source generation with image/demo loads.
    var originalWorkingLoader = realApp.core.imageLoader.loadWorkingImage;
    realApp.core.imageLoader.loadWorkingImage = decode;
    var importController = new realEditor.Controller({ initialState: realEditor.projectWorkspace.snapshot(restored), render: function () { renders++; } });
    var preservedState = importController.state, preservedSettings = JSON.stringify(preservedState.settings);
    var invalidLoad = await begin(function () { return importController.loadProjectRoute(projectRoute); });
    invalidLoad.deferred.resolve(result('wrong dimensions')); // correct 2x2, override below for invalid dimensions
    // Decode fixture dimensions must fail before replacing the workspace.
    await invalidLoad.promise;
    var failedLoad = await begin(function () { return importController.loadProjectRoute(projectRoute); });
    var wrong = result('wrong'); wrong.imageData = new ImageData(1, 1);
    preservedState = importController.state; preservedSettings = JSON.stringify(preservedState.settings);
    failedLoad.deferred.resolve(wrong); await failedLoad.promise;
    harness.assert(importController.state === preservedState && JSON.stringify(importController.state.settings) === preservedSettings, 'failed restore preserves entire workspace');
    var oldProject = await begin(function () { return importController.loadProjectRoute(projectRoute); });
    var newerDemo = await begin(function () { return importController.loadDemo(); });
    newerDemo.deferred.resolve(result('new demo')); await newerDemo.promise;
    oldProject.deferred.resolve(workingResult); await oldProject.promise;
    harness.assert(importController.state.fileName === 'new demo', 'project/demo share source generation');
    importController.destroy();
    realApp.core.imageLoader.loadWorkingImage = originalWorkingLoader;
    passed.push('W03 real project restore failure atomicity and project/demo inversion');

    // Owner snapshots are captured before later UI mutations; destroy cannot commit a worker result.
    harness.execute('pages/dither-editor/worker/dither-worker-client.js', { window: { DitherApp: realApp }, Worker: harness.FakeWorker });
    var exportState = realEditor.projectWorkspace.snapshot(restored);
    exportState.settings.dither.algorithm = 'floyd-steinberg';
    exportState.settings.palette.presetId = 'custom';
    exportState.settings.palette.palette = [{ r: 0, g: 0, b: 0 }, { r: 255, g: 255, b: 255 }];
    var downloads = 0;
    realApp.core.imageExporter.exportPng = function () { downloads++; return Promise.resolve(); };
    var exportController = new realEditor.Controller({ initialState: exportState, render: function () { renders++; } });
    var workerEntered = harness.deferred(), posted;
    var oldPost = harness.FakeWorker.prototype.postMessage;
    harness.FakeWorker.prototype.postMessage = function (message, transfer) { oldPost.call(this, message, transfer); posted = message; workerEntered.resolve(this); };
    var exporting = exportController.exportPng();
    var exportWorker = await workerEntered.promise;
    var capturedStrength = posted.options.errorStrength;
    exportController.state.settings.dither.errorStrength = 1;
    harness.assert(posted.options.errorStrength === capturedStrength && capturedStrength === restored.settings.dither.errorStrength, 'export worker snapshot');
    var beforeDestroyRenders = renders;
    exportController.destroy(); await exporting;
    harness.assert(downloads === 0 && renders === beforeDestroyRenders && exportWorker.terminated && exportController.workerClient.pendingCount() === 0, 'real controller destroy no fallback/commit');
    var remounted = new realEditor.Controller({ initialState: realEditor.projectWorkspace.snapshot(restored), render: function () {} });
    var remountWork = remounted.workerClient.run(input, {}, {});
    exportController.destroy();
    harness.assert(remounted.workerClient.pendingCount() === 1, 'old controller cleanup cannot terminate remount');
    var remountCancelled = remountWork.catch(function (error) { return error.code; }); remounted.destroy(); await remountCancelled;
    harness.FakeWorker.prototype.postMessage = oldPost;
    passed.push('W02 real controller export snapshot, destroy, remount ownership');

    realEditor.featureRegistry.register({ id: 'test-persist', dock: false, panelGroup: 'none', defaultSettings: function () { return { count: 3 }; },
        persistence: { version: 2, serializeSettings: function (settings) { return { count: settings.count }; }, restoreSettings: function (settings, version) {
            if (version !== 2 || !Number.isInteger(settings.count)) { throw new Error('invalid test setting'); } return { count: settings.count };
        } } });
    restored.settings['test-persist'] = { count: 9 };
    var extendedManifest = realEditor.projectWorkspace.createManifest(restored, restoredPixels);
    var extended = realEditor.projectWorkspace.restore(Object.assign({}, project, { manifest: extendedManifest }), workingResult);
    harness.assert(extended.settings['test-persist'].count === 9 && extendedManifest.editor.features['test-persist'].version === 2, 'extension has no workspace branch');
    rejects(function () { realEditor.featureRegistry.register({ id: 'missing-contract', dock: false, defaultSettings: function () { return {}; } }); }, 'missing persistence fails registration');
    realEditor.featureRegistry.register({ id: 'action-only', dock: false });
    passed.push('W07 old v1 PNG/settings/pixels, invalid settings/version, snapshot, extensible persistence');
    var snapshotPalette = realEditor.targetPolicy.displayColors();
    var calibrated = new ImageData(1, 1);
    calibrated.data.set([snapshotPalette[0].r, snapshotPalette[0].g, snapshotPalette[0].b, 255]);
    var originalColors = realApp.device.epaperCalibration.colors;
    realApp.device.epaperCalibration.colors = function () { return snapshotPalette.map(function () { return { r: 201, g: 202, b: 203 }; }); };
    var protocolPixel = realEditor.targetPolicy.outputImageData(calibrated, snapshotPalette);
    harness.assert(JSON.stringify(Array.from(protocolPixel.data)) === '[0,0,0,255]', 'output encoding uses admitted palette snapshot');
    realApp.device.epaperCalibration.colors = originalColors;
    var epdImage = new ImageData(800, 480);
    for (var epdOffset = 3; epdOffset < epdImage.data.length; epdOffset += 4) { epdImage.data[epdOffset] = 255; }
    var encoded = realApp.core.epdimgEncoder.encode(epdImage);
    var header = new DataView(encoded.payload.buffer);
    harness.assert(encoded.payload.length === 192040 && header.getUint32(12, true) === 40 && header.getUint32(16, true) === 800 && header.getUint32(20, true) === 480, 'EPDIMG v1 dimensions/length');
    harness.assert(header.getUint32(28, true) === 0x91875ccc && (header.getUint32(32, true) !== 0 || header.getUint32(36, true) !== 0), 'EPDIMG independent CRC and generation');
    epdImage.data[0] = 42;
    rejects(function () { realApp.core.epdimgEncoder.encode(epdImage); }, 'EPDIMG palette rejects unsupported RGB');
    epdImage.data[0] = 0; epdImage.data[3] = 0;
    rejects(function () { realApp.core.epdimgEncoder.encode(epdImage); }, 'EPDIMG rejects alpha');
    passed.push('W02 palette snapshot; W07 EPDIMG header/CRC/palette/alpha boundary');
    realApp.core.imageLoader.loadDemoImage = originalDemoLoader;
    var fileDemo = await originalDemoLoader(800);
    harness.assert(fileDemo.imageData.width > 0 && fileDemo.sourceFile.blob.size > 0, 'source file demo and original Blob');
    var filePng = await realApp.core.canvasUtils.imageDataToBlob(fileDemo.imageData);
    harness.assert(filePng.type === 'image/png' && filePng.size > 0, 'file mode local PNG');
    passed.push('W08 source file demo decode and local PNG export bytes');
    document.getElementById('result').textContent = JSON.stringify({ passed: passed, memoryObservations: memoryObservations });
})().catch(function (error) { document.getElementById('result').textContent = JSON.stringify({ error: error.stack }); });
