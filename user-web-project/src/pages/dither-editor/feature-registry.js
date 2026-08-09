(function (app) {
    // Dither Editor 的 plug-and-play 核心。
    // feature 透過 register() 提供 dock、panel、operation 與 lifecycle hooks；
    // page/controller 只查 registry，不硬寫 crop、palette 等單一 feature。
    var features = [];
    var featureIds = {};
    var VALID_PIPELINE_STAGES = {
        fixedBefore: true,
        effectsOrder: true,
        fixedAfter: true
    };
    var PANEL_GROUP_NONE = 'none';
    var VALID_PANEL_GROUPS = {
        source: true,
        prepare: true,
        edit: true,
        none: true
    };

    // 註冊單一 feature，並把它的 operation 交給 operationRegistry。
    function register(feature) {
        validateFeature(feature);
        features.push(feature);
        featureIds[feature.id] = true;
        if (feature.operation) {
            // operation id 預設等於 feature id，讓 pipeline settings 可以用同一把 key。
            app.pages.ditherEditor.operationRegistry.register(
                Object.assign({ id: feature.id }, feature.operation)
            );
        }
    }

    // 依 id 取得 feature 定義。
    function get(id) {
        return features.find(function (feature) {
            return feature.id === id;
        });
    }

    // 跨 feature 查詢的唯一管道：回傳 feature 宣告的 publicApi。
    // feature 未註冊（停用）時回 null，呼叫端必須有明確的降級路徑；
    // 禁止 feature 直接讀寫 state.settings.<其他 feature>。
    function api(id) {
        var feature = get(id);
        return feature && feature.api ? feature.api : null;
    }

    // 從 manifest 解析出啟用且依賴順序正確的 script 路徑。
    // entry 可用 paths 宣告多支 script（依序載入），feature 本體放最後一支。
    function featureScripts() {
        return resolveManifest(app.pages.ditherEditor.featureManifest || []).reduce(function (paths, entry) {
            return paths.concat(entry.paths || [entry.path]);
        }, []);
    }

    // 檢查 manifest 宣告啟用的 feature 是否真的完成 register。
    function assertRegistered() {
        resolveManifest(app.pages.ditherEditor.featureManifest || []).forEach(function (entry) {
            if (!get(entry.id)) {
                throw new Error('Feature script did not register feature: ' + entry.id);
            }
        });
    }

    // 過濾停用項目並做 dependency topological sort。
    function resolveManifest(manifest) {
        var enabled = manifest.filter(function (entry) {
            return entry.enabled !== false;
        });
        var byId = {};
        var resolved = [];
        var visiting = {};
        var visited = {};

        enabled.forEach(function (entry) {
            validateManifestEntry(entry, byId);
            byId[entry.id] = entry;
        });

        enabled
            .slice()
            .sort(function (a, b) {
                // loadOrder 只決定同層初步順序；dependsOn 仍會在 visit 時被放到前面。
                return (a.loadOrder || 0) - (b.loadOrder || 0);
            })
            .forEach(function (entry) {
                visitManifestEntry(entry, byId, visiting, visited, resolved);
            });

        return resolved;
    }

    // DFS 解析依賴順序，同時偵測循環依賴。
    function visitManifestEntry(entry, byId, visiting, visited, resolved) {
        if (visited[entry.id]) {
            return;
        }
        if (visiting[entry.id]) {
            throw new Error('Circular feature dependency: ' + entry.id);
        }

        visiting[entry.id] = true;
        (entry.dependsOn || []).forEach(function (dependencyId) {
            var dependency = byId[dependencyId];
            if (!dependency) {
                throw new Error('Missing feature dependency: ' + entry.id + ' depends on ' + dependencyId);
            }
            visitManifestEntry(dependency, byId, visiting, visited, resolved);
        });
        visiting[entry.id] = false;
        visited[entry.id] = true;
        resolved.push(entry);
    }

    // 驗證 manifest item 的基本欄位與 dependsOn 目標。
    function validateManifestEntry(entry, byId) {
        if (!entry || !entry.id || (!entry.path && !(entry.paths && entry.paths.length))) {
            throw new Error('Feature manifest entries require id and path (or paths).');
        }
        if (byId[entry.id]) {
            throw new Error('Duplicate feature manifest id: ' + entry.id);
        }
    }

    // 驗證 feature contract，避免缺少 id/labelKey 等必要欄位時靜默失敗。
    function validateFeature(feature) {
        if (!feature || !feature.id) {
            throw new Error('Feature registration requires id.');
        }
        if (featureIds[feature.id]) {
            throw new Error('Duplicate feature id: ' + feature.id);
        }
        if (feature.pipelineStage && !VALID_PIPELINE_STAGES[feature.pipelineStage]) {
            throw new Error('Invalid pipeline stage for feature ' + feature.id + ': ' + feature.pipelineStage);
        }
        if (feature.panelGroup && !VALID_PANEL_GROUPS[feature.panelGroup]) {
            throw new Error('Invalid panel group for feature ' + feature.id + ': ' + feature.panelGroup);
        }
        if (feature.dock !== false && usesPanelGroup(feature) && (!feature.icon || !feature.labelKey)) {
            throw new Error('Dock feature requires icon and labelKey: ' + feature.id);
        }
        if (feature.pipelineStage && feature.id !== 'export' && !feature.operation) {
            throw new Error('Pipeline feature requires operation: ' + feature.id);
        }
        if (feature.operation && typeof feature.operation.run !== 'function') {
            throw new Error('Feature operation requires run(): ' + feature.id);
        }
        if (feature.api && typeof feature.api !== 'object') {
            throw new Error('Feature api must be an object: ' + feature.id);
        }
    }

    // 取得某個 pipeline stage 的 feature id 順序；沒有使用者順序時用 feature metadata。
    function pipelineIds(pipeline, stage) {
        var configured = pipeline && pipeline[stage];
        if (!configured || configured.length === 0) {
            // 若 preset 沒指定順序，就由 feature 自己的 pipelineOrder 產生預設 pipeline。
            return features
                .filter(function (feature) {
                    return feature.pipelineStage === stage && isPipelineFeatureAvailable(feature);
                })
                .sort(function (a, b) {
                    return (a.pipelineOrder || 0) - (b.pipelineOrder || 0);
                })
                .map(function (feature) {
                    return feature.id;
                });
        }

        return configured
            .map(get)
            .filter(function (feature) {
                return feature && feature.pipelineStage === stage && isPipelineFeatureAvailable(feature);
            })
            .map(function (feature) {
                return feature.id;
            });
    }

    // 產生工具列顯示順序，拖曳型工具依目前 effectsOrder 排列。
    function dockTools(pipeline) {
        // Tool dock = 固定非 pipeline 工具 + fixedBefore + 可拖曳 effects。
        // Export 由 feature action 外露，不放進 accordion tool list。
        var fixedTools = features
            .filter(function (feature) {
                return feature.dock !== false && !feature.pipelineStage && usesPanelGroup(feature);
            })
            .sort(function (a, b) {
                return (a.dockOrder || 0) - (b.dockOrder || 0);
            });

        return fixedTools
            .concat(pipelineIds(pipeline, 'fixedBefore').map(get))
            .concat(pipelineIds(pipeline, 'effectsOrder').map(get))
            .filter(function (feature) {
                return feature && usesPanelGroup(feature);
            });
    }

    function panelGroup(feature) {
        if (!feature) {
            return PANEL_GROUP_NONE;
        }
        return feature.panelGroup || PANEL_GROUP_NONE;
    }

    function usesPanelGroup(feature) {
        return panelGroup(feature) !== PANEL_GROUP_NONE;
    }

    function panelGroupFor(id) {
        return panelGroup(get(id));
    }

    function panelGroupIds(pipeline, group) {
        return dockTools(pipeline)
            .filter(function (feature) {
                return panelGroup(feature) === group;
            })
            .map(function (feature) {
                return feature.id;
            });
    }

    // 判斷 feature 是否有可放進 pipeline 的 operation。
    function isPipelineFeatureAvailable(feature) {
        return feature.id === 'export' || app.pages.ditherEditor.operationRegistry.get(feature.id);
    }

    // 呼叫所有 feature 的指定 lifecycle hook。
    function dispatch(name, context, options) {
        var targetId = options && options.id;
        features.forEach(function (feature) {
            if (!feature[name]) {
                return;
            }
            if (targetId && targetId !== feature.id) {
                return;
            }
            feature[name](Object.assign({ feature: feature }, context || {}));
        });
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.featureRegistry = {
        register: register,
        get: get,
        api: api,
        all: function all() {
            return features.slice();
        },
        featureScripts: featureScripts,
        assertRegistered: assertRegistered,
        pipelineIds: pipelineIds,
        dockTools: dockTools,
        panelGroupFor: panelGroupFor,
        panelGroupIds: panelGroupIds,
        dispatch: dispatch
    };
})(window.DitherApp);
