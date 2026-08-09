(function (app) {
    // Dither algorithm registry 讓演算法和 processor 維持共同 plug-and-play 介面。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    var algorithms = [];
    var algorithmById = {};
    var processors = {};

    function copyAlgorithm(algorithm) {
        var entry = {};
        Object.keys(algorithm).forEach(function (key) {
            entry[key] = algorithm[key];
        });
        entry.matrixId = entry.matrixId || null;
        entry.supportsPalette = entry.supportsPalette !== false;
        entry.supportsSerpentine = entry.supportsSerpentine === true;
        entry.supportsErrorStrength = entry.supportsErrorStrength === true;
        entry.supportsThresholdStrength = entry.supportsThresholdStrength === true;
        entry.supportsDotDensity = entry.supportsDotDensity === true;
        return entry;
    }

    function validateId(id, type) {
        if (!id || typeof id !== 'string') {
            throw new Error(type + ' must provide a string id.');
        }
    }

    app.pages.ditherEditor.ditherAlgorithmRegistry = {
        registerProcessor: function registerProcessor(processor) {
            validateId(processor && processor.id, 'Dither processor');
            if (typeof processor.apply !== 'function') {
                throw new Error('Dither processor "' + processor.id + '" must provide apply().');
            }
            if (processors[processor.id]) {
                throw new Error('Duplicate dither processor id "' + processor.id + '".');
            }
            processors[processor.id] = processor;
        },

        register: function register(algorithm) {
            validateId(algorithm && algorithm.id, 'Dither algorithm');
            validateId(algorithm.labelKey, 'Dither algorithm "' + algorithm.id + '" labelKey');
            validateId(algorithm.processorId, 'Dither algorithm "' + algorithm.id + '" processorId');
            if (algorithmById[algorithm.id]) {
                throw new Error('Duplicate dither algorithm id "' + algorithm.id + '".');
            }

            var entry = copyAlgorithm(algorithm);
            algorithms.push(entry);
            algorithmById[entry.id] = entry;
            return entry;
        },

        get: function get(id) {
            return algorithmById[id] || null;
        },

        first: function first() {
            return algorithms[0] || null;
        },

        list: function list() {
            return algorithms.slice();
        },

        supportsErrorStrength: function supportsErrorStrength(id) {
            var algorithm = algorithmById[id];
            return Boolean(algorithm && algorithm.supportsErrorStrength);
        },

        supportsThresholdStrength: function supportsThresholdStrength(id) {
            var algorithm = algorithmById[id];
            return Boolean(algorithm && algorithm.supportsThresholdStrength);
        },

        supportsDotDensity: function supportsDotDensity(id) {
            var algorithm = algorithmById[id];
            return Boolean(algorithm && algorithm.supportsDotDensity);
        },

        supportsSerpentine: function supportsSerpentine(id) {
            var algorithm = algorithmById[id];
            return Boolean(algorithm && algorithm.supportsSerpentine);
        },

        run: function run(imageData, algorithmOrId, options) {
            var algorithm = typeof algorithmOrId === 'string'
                ? algorithmById[algorithmOrId]
                : algorithmOrId;
            if (!algorithm) {
                return imageData;
            }
            var processor = processors[algorithm.processorId];
            if (!processor) {
                throw new Error(
                    'Dither algorithm "' + algorithm.id
                    + '" references missing processor "' + algorithm.processorId + '".'
                );
            }
            return processor.apply(imageData, options || {}, algorithm);
        },

        assertRegistered: function assertRegistered() {
            algorithms.forEach(function (algorithm) {
                if (!processors[algorithm.processorId]) {
                    throw new Error(
                        'Dither algorithm "' + algorithm.id
                        + '" references missing processor "' + algorithm.processorId + '".'
                    );
                }
            });
        }
    };
})(window.DitherApp);
