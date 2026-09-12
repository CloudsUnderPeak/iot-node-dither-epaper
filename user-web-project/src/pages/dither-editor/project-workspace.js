(function (app) {
    var WORKSPACE_VERSION = 1;

    function workspaceError(code, message) {
        var error = new Error(message);
        error.code = code;
        return error;
    }

    function copy(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function safeFileName(name) {
        var base = String(name || 'untitled')
            .replace(/\.dither\.png$/i, '')
            .replace(/\.(png|jpe?g|webp)$/i, '')
            .replace(/[\\/:*?"<>|\x00-\x1f]/g, '-')
            .trim();
        return (base || 'untitled').slice(0, 180) + '.dither.png';
    }

    function validateSize(size, label) {
        if (!size || !Number.isInteger(size.width) || !Number.isInteger(size.height)
            || size.width < 1 || size.height < 1 || size.width > 16384 || size.height > 16384) {
            throw workspaceError('settings-invalid', label + ' size is invalid.');
        }
    }

    function validateFeatureValue(value, depth) {
        if (depth > 12) {
            return false;
        }
        if (value === null || typeof value === 'boolean' || typeof value === 'string') {
            return typeof value !== 'string' || value.length <= 4096;
        }
        if (typeof value === 'number') {
            return Number.isFinite(value);
        }
        if (Array.isArray(value)) {
            return value.length <= 64 && value.every(function (item) {
                return validateFeatureValue(item, depth + 1);
            });
        }
        return Boolean(value && typeof value === 'object') && Object.keys(value).length <= 64
            && Object.keys(value).every(function (key) {
                return key !== '__proto__' && key !== 'prototype' && key !== 'constructor'
                    && validateFeatureValue(value[key], depth + 1);
            });
    }

    function validColor(color) {
        return Boolean(color) && ['r', 'g', 'b'].every(function (key) {
            return Number.isInteger(color[key]) && color[key] >= 0 && color[key] <= 255;
        });
    }

    function validPalette(value, allowNull) {
        if (allowNull && value === null) {
            return true;
        }
        return Array.isArray(value) && value.length >= 2 && value.length <= 32 && value.every(validColor);
    }

    function validateFeatureSettings(id, value) {
        if (!validateFeatureValue(value, 0) || !value || typeof value !== 'object' || Array.isArray(value)) {
            return false;
        }
        if (id === 'crop') {
            return ['x', 'y', 'width', 'height', 'panX', 'panY', 'zoom', 'rotation'].every(function (key) {
                return Number.isFinite(value[key]);
            }) && value.width >= 1 && value.height >= 1 && value.zoom >= 0.1 && value.zoom <= 20
                && value.rotation >= -180 && value.rotation <= 180
                && typeof value.aspectRatioId === 'string'
                && typeof value.flipX === 'boolean' && typeof value.flipY === 'boolean'
                && typeof value.backgroundPreset === 'string'
                && /^#[0-9a-f]{6}$/i.test(value.backgroundColor)
                && /^#[0-9a-f]{6}$/i.test(value.autoBackgroundColor);
        }
        if (id === 'resize') {
            return Number.isInteger(value.width) && Number.isInteger(value.height)
                && value.width >= 1 && value.height >= 1 && value.width <= 4096 && value.height <= 4096
                && Number.isFinite(value.aspectRatio) && value.aspectRatio > 0;
        }
        if (id === 'adjust') {
            return ['brightness', 'contrast', 'saturation'].every(function (key) {
                return Number.isFinite(value[key]) && value[key] >= -100 && value[key] <= 100;
            });
        }
        if (id === 'palette') {
            return typeof value.presetId === 'string'
                && validPalette(value.palette, true) && validPalette(value.originalPalette, true)
                && Number.isInteger(value.originalPaletteSize)
                && value.originalPaletteSize >= 2 && value.originalPaletteSize <= 32;
        }
        if (id === 'dither') {
            var algorithms = ['none'].concat(app.pages.ditherEditor.ditherAlgorithmRegistry.list().map(function (item) {
                return item.id;
            }));
            return algorithms.indexOf(value.algorithm) !== -1
                && ['nearest-color', 'pair-mix', 'tri-mix'].indexOf(value.paletteMapping) !== -1
                && ['euclidean-bt709', 'euclidean-rgb', 'manhattan-bt709', 'manhattan-rgb', 'ciede2000'].indexOf(value.colorDistance) !== -1
                && typeof value.serpentine === 'boolean'
                && Number.isFinite(value.errorStrength) && value.errorStrength >= 0 && value.errorStrength <= 150;
        }
        if (id === 'export') {
            return value.format === 'png';
        }
        return false;
    }

    function allowedPipelineIds(defaultState, stage) {
        return defaultState.pipeline[stage].slice();
    }

    function validatePipeline(pipeline, defaultState) {
        if (!pipeline || !pipeline.enabled || typeof pipeline.enabled !== 'object') {
            throw workspaceError('settings-invalid', 'Project pipeline is invalid.');
        }
        ['fixedBefore', 'effectsOrder', 'fixedAfter'].forEach(function (stage) {
            var allowed = allowedPipelineIds(defaultState, stage);
            var values = pipeline[stage];
            if (!Array.isArray(values) || values.length !== allowed.length) {
                throw workspaceError('feature-unsupported', 'Project pipeline is not supported.');
            }
            var seen = {};
            values.forEach(function (id) {
                if (typeof id !== 'string' || allowed.indexOf(id) === -1 || seen[id]) {
                    throw workspaceError('feature-unsupported', 'Project pipeline is not supported.');
                }
                seen[id] = true;
            });
        });
        Object.keys(pipeline.enabled).forEach(function (id) {
            var known = defaultState.pipeline.fixedBefore.concat(defaultState.pipeline.effectsOrder).indexOf(id) !== -1;
            if (!known || typeof pipeline.enabled[id] !== 'boolean') {
                throw workspaceError('settings-invalid', 'Project operation state is invalid.');
            }
        });
    }

    function manifest(state, resultImageData) {
        if (!state.sourceFile || !state.sourceFile.blob) {
            throw workspaceError('source-invalid', 'Original source image is unavailable. Reload the image before exporting.');
        }
        validateSize(state.originalSize, 'Original image');
        validateSize(state.sourceImageData, 'Working image');
        validateSize(resultImageData, 'Result image');
        var features = {};
        Object.keys(state.settings || {}).forEach(function (id) {
            features[id] = { version: 1, settings: copy(state.settings[id]) };
        });
        return {
            format: app.core.projectFile.format,
            schemaVersion: app.core.projectFile.schemaVersion,
            rendererVersion: 1,
            producer: { appId: 'embedded-web-dithering', appVersion: '0.1.0' },
            createdAt: new Date().toISOString(),
            source: {
                kind: 'file',
                fileName: state.sourceFile.fileName || state.fileName || 'Untitled',
                mimeType: state.sourceFile.mimeType || state.sourceFile.blob.type || 'image/png',
                byteLength: state.sourceFile.blob.size,
                originalSize: copy(state.originalSize)
            },
            working: {
                width: state.sourceImageData.width,
                height: state.sourceImageData.height,
                normalizationVersion: WORKSPACE_VERSION
            },
            result: { width: resultImageData.width, height: resultImageData.height },
            editor: {
                mode: state.mode,
                viewMode: state.viewMode,
                openToolPanels: copy(state.openToolPanels || {}),
                pipeline: copy(state.pipeline),
                features: features
            },
            renderContext: {
                targetMode: state.target && state.target.mode || 'standalone',
                orientation: state.target && state.target.orientation || null,
                displayPalette: state.settings.palette && copy(state.settings.palette.palette),
                calibrationRevision: state.target && state.target.calibrationRevision || 0
            }
        };
    }

    function snapshot(state) {
        var value = Object.assign({}, state);
        value.settings = copy(state.settings);
        value.pipeline = copy(state.pipeline);
        value.target = copy(state.target || {});
        value.openToolPanels = copy(state.openToolPanels || {});
        value.originalSize = copy(state.originalSize);
        value.workingSize = copy(state.workingSize);
        value.sourceFile = state.sourceFile ? Object.assign({}, state.sourceFile) : null;
        return value;
    }

    function restore(project, workingResult) {
        var data = project.manifest;
        var base = app.pages.ditherEditor.state.create();
        if (!data.source || !data.working || !data.result || !data.editor || !data.editor.features) {
            throw workspaceError('project-data-invalid', 'Project workspace data is incomplete.');
        }
        validateSize(data.source.originalSize, 'Original image');
        validateSize(data.working, 'Working image');
        validateSize(data.result, 'Result image');
        if (workingResult.imageData.width !== data.working.width
            || workingResult.imageData.height !== data.working.height) {
            throw workspaceError('source-invalid', 'Working image dimensions do not match the project.');
        }
        validatePipeline(data.editor.pipeline, base);
        var featureIds = Object.keys(base.settings);
        var documentFeatureIds = Object.keys(data.editor.features);
        if (documentFeatureIds.some(function (id) { return featureIds.indexOf(id) === -1; })
            || featureIds.some(function (id) { return documentFeatureIds.indexOf(id) === -1; })) {
            throw workspaceError('feature-unsupported', 'Project uses unsupported editor features.');
        }
        featureIds.forEach(function (id) {
            var record = data.editor.features[id];
            if (!record || record.version !== 1 || !validateFeatureSettings(id, record.settings)) {
                throw workspaceError('settings-invalid', 'Project feature settings are invalid.');
            }
            base.settings[id] = copy(record.settings);
        });
        base.fileName = data.source.fileName;
        base.sourceFile = {
            blob: project.sourceBlob,
            fileName: data.source.fileName,
            mimeType: data.source.mimeType
        };
        base.sourceImageData = workingResult.imageData;
        base.originalSize = copy(data.source.originalSize);
        base.workingSize = { width: workingResult.imageData.width, height: workingResult.imageData.height };
        base.pipeline = copy(data.editor.pipeline);
        base.mode = data.editor.mode === 'source' || data.editor.mode === 'prepare' ? data.editor.mode : 'edit';
        base.viewMode = data.editor.viewMode === 'original' || data.editor.viewMode === 'pixel'
            ? data.editor.viewMode : 'result';
        base.openToolPanels = copy(data.editor.openToolPanels || {});
        base.status = 'ready';
        base.target = {
            mode: data.renderContext && data.renderContext.targetMode === 'epaper' ? 'epaper' : 'standalone',
            orientation: data.renderContext && data.renderContext.orientation || null,
            calibrationRevision: data.renderContext && data.renderContext.calibrationRevision || 0
        };
        return base;
    }

    app.pages.ditherEditor.projectWorkspace = {
        createManifest: manifest,
        snapshot: snapshot,
        restore: restore,
        exportFileName: safeFileName
    };
})(window.DitherApp);
