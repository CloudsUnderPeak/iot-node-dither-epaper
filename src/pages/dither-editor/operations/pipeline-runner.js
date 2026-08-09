(function (app) {
    // Pipeline runner 是純執行器：依 state.pipeline 決定順序，逐步把 ImageData 丟給 operation。
    // 它不讀 DOM，也不決定哪些 tool 顯示；那些責任在 feature/page 層。
    var MAX_STAGE_CACHE_ENTRIES = 32;
    var imageDataIds = typeof WeakMap === 'function' ? new WeakMap() : null;
    var nextImageDataId = 1;

    function orderedOperationIds(state) {
        var pipeline = state.pipeline;
        return pipeline.fixedBefore
            .concat(pipeline.effectsOrder)
            .concat(pipeline.fixedAfter)
            .filter(function (id) {
                // export 是 action，不是會改變 pixels 的 pipeline operation。
                return id !== 'export' && pipeline.enabled[id] !== false;
            });
    }

    function createStageCache() {
        return {
            entries: new Map()
        };
    }

    function clearStageCache(stageCache) {
        if (stageCache && stageCache.entries && stageCache.entries.clear) {
            stageCache.entries.clear();
        }
    }

    function normalizeStageCache(stageCache) {
        if (!stageCache) {
            return null;
        }
        if (!stageCache.entries || !stageCache.entries.get) {
            stageCache.entries = new Map();
        }
        return stageCache;
    }

    function imageDataKey(imageData) {
        if (!imageData) {
            return 'image:null';
        }
        if (imageDataIds) {
            var existingId = imageDataIds.get(imageData);
            if (!existingId) {
                existingId = nextImageDataId;
                nextImageDataId += 1;
                imageDataIds.set(imageData, existingId);
            }
            return 'image:' + existingId + ':' + imageData.width + 'x' + imageData.height;
        }
        if (!imageData.__ditherStageCacheId) {
            imageData.__ditherStageCacheId = nextImageDataId;
            nextImageDataId += 1;
        }
        return 'image:' + imageData.__ditherStageCacheId + ':' + imageData.width + 'x' + imageData.height;
    }

    function isImageDataLike(value) {
        return Boolean(
            value &&
            typeof value === 'object' &&
            value.data &&
            typeof value.width === 'number' &&
            typeof value.height === 'number'
        );
    }

    function stableValue(value) {
        if (value === null || typeof value !== 'object') {
            if (typeof value === 'number' && !Number.isFinite(value)) {
                return String(value);
            }
            return value;
        }
        if (isImageDataLike(value)) {
            return { imageDataKey: imageDataKey(value) };
        }
        if (Array.isArray(value)) {
            return value.map(stableValue);
        }

        var output = {};
        Object.keys(value).sort().forEach(function (key) {
            if (typeof value[key] === 'undefined' || typeof value[key] === 'function') {
                return;
            }
            output[key] = stableValue(value[key]);
        });
        return output;
    }

    function stableSerialize(value) {
        return JSON.stringify(stableValue(value));
    }

    function stageCacheKey(operation, id, inputKey, imageData, state) {
        var settings = state.settings[id] || {};
        var context = {
            id: id,
            state: state,
            inputImageData: imageData
        };
        var extra = typeof operation.cacheKey === 'function'
            ? operation.cacheKey(settings, context)
            : null;
        return [
            id,
            inputKey,
            stableSerialize(settings),
            stableSerialize(extra)
        ].join('\n');
    }

    function getStageCacheEntry(stageCache, key) {
        if (!stageCache) {
            return null;
        }
        var entry = stageCache.entries.get(key);
        if (!entry) {
            return null;
        }
        stageCache.entries.delete(key);
        stageCache.entries.set(key, entry);
        return entry;
    }

    function setStageCacheEntry(stageCache, key, entry) {
        if (!stageCache) {
            return;
        }
        stageCache.entries.set(key, entry);
        while (stageCache.entries.size > MAX_STAGE_CACHE_ENTRIES) {
            stageCache.entries.delete(stageCache.entries.keys().next().value);
        }
    }

    function runOperationIds(inputImageData, state, order, options) {
        var current = inputImageData;
        var stageCache = normalizeStageCache(options && options.stageCache);
        var inputKey = stageCache ? imageDataKey(current) : '';

        for (var i = 0; i < order.length; i += 1) {
            var operation = app.pages.ditherEditor.operationRegistry.get(order[i]);
            if (!operation) {
                throw new Error('Missing operation: ' + order[i]);
            }
            var cacheKey = null;
            var cached = null;
            if (stageCache && operation.cacheable !== false) {
                cacheKey = stageCacheKey(operation, order[i], inputKey, current, state);
                cached = getStageCacheEntry(stageCache, cacheKey);
                if (cached) {
                    current = cached.imageData;
                    inputKey = cached.outputKey;
                    continue;
                }
            }
            // Operation 若改變 pixels 必須回傳新的 ImageData；若設定為 no-op，可回傳原物件。
            var previous = current;
            current = operation.run(current, state.settings[order[i]] || {}, {
                id: order[i],
                state: state,
                stageCache: stageCache
            });
            if (stageCache && operation.cacheable !== false) {
                var outputKey = current === previous
                    ? inputKey
                    : 'stage:' + order[i] + ':' + cacheKey;
                setStageCacheEntry(stageCache, cacheKey, {
                    imageData: current,
                    outputKey: outputKey
                });
                inputKey = outputKey;
            } else if (stageCache) {
                inputKey = imageDataKey(current);
            }
        }
        return current;
    }

    // 非同步版執行器：operation.run 可回傳 ImageData 或 Promise<ImageData>（worker 路徑）。
    // cache 邏輯與同步版一致；同步版保留給 prepare group 等保證同步的路徑。
    function runOperationIdsAsync(inputImageData, state, order, options) {
        var stageCache = normalizeStageCache(options && options.stageCache);
        var current = inputImageData;
        var inputKey = stageCache ? imageDataKey(current) : '';
        var index = 0;

        function step() {
            if (index >= order.length) {
                return Promise.resolve(current);
            }
            var id = order[index];
            index += 1;
            var operation = app.pages.ditherEditor.operationRegistry.get(id);
            if (!operation) {
                return Promise.reject(new Error('Missing operation: ' + id));
            }
            var cacheKey = null;
            if (stageCache && operation.cacheable !== false) {
                cacheKey = stageCacheKey(operation, id, inputKey, current, state);
                var cached = getStageCacheEntry(stageCache, cacheKey);
                if (cached) {
                    current = cached.imageData;
                    inputKey = cached.outputKey;
                    return step();
                }
            }
            var previous = current;
            return Promise.resolve(operation.run(previous, state.settings[id] || {}, {
                id: id,
                state: state,
                stageCache: stageCache
            })).then(function (result) {
                current = result;
                if (stageCache && operation.cacheable !== false) {
                    var outputKey = current === previous
                        ? inputKey
                        : 'stage:' + id + ':' + cacheKey;
                    setStageCacheEntry(stageCache, cacheKey, {
                        imageData: current,
                        outputKey: outputKey
                    });
                    inputKey = outputKey;
                } else if (stageCache) {
                    inputKey = imageDataKey(current);
                }
                return step();
            });
        }

        return step();
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.pipelineRunner = {
        createStageCache: createStageCache,
        clearStageCache: clearStageCache,
        // 依 state.pipeline 順序執行 operation，並把每步結果傳給下一步。
        run: function run(inputImageData, state, options) {
            return runOperationIds(inputImageData, state, orderedOperationIds(state), options);
        },
        // 非同步版：preview/export 走這裡，容許 dither 等 stage 回傳 Promise（worker）。
        runAsync: function runAsync(inputImageData, state, options) {
            return runOperationIdsAsync(inputImageData, state, orderedOperationIds(state), options);
        },
        // 執行指定 panel group 對應的 pipeline operation，用於取得 prepare 後的 edit original。
        runPanelGroup: function runPanelGroup(inputImageData, state, group, options) {
            return runOperationIds(
                inputImageData,
                state,
                orderedOperationIds(state).filter(function (id) {
                    return app.pages.ditherEditor.featureRegistry.panelGroupFor(id) === group;
                }),
                options
            );
        }
    };
})(window.DitherApp);
