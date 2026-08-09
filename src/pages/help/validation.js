(function (app) {
    app.pages.help = app.pages.help || {};

    function algorithmDetails(bundle) {
        var details = {};
        var documents = bundle && bundle.documents || {};
        Object.keys(documents).forEach(function (documentId) {
            (documents[documentId].sections || []).forEach(function (section) {
                (section.algorithms || []).forEach(function (algorithm) {
                    if (algorithm.id) {
                        details[algorithm.id] = {
                            detail: algorithm,
                            family: section.capabilityFamily || null
                        };
                    }
                });
            });
        });
        return details;
    }

    function algorithmSection(bundle, family) {
        var documents = bundle && bundle.documents || {};
        var found = null;
        Object.keys(documents).some(function (documentId) {
            return (documents[documentId].sections || []).some(function (section) {
                if (section.capabilityFamily === family) {
                    found = section;
                    return true;
                }
                return false;
            });
        });
        return found;
    }

    function validate() {
        var errors = [];
        var warnings = [];
        var algorithms = app.app.projectCapabilities.list('dither-algorithms');
        var algorithmIds = {};
        var locales = ['en', 'zh-TW'];

        algorithms.forEach(function (algorithm) {
            algorithmIds[algorithm.id] = true;
            if (!algorithm.helpFamily) {
                errors.push('Algorithm "' + algorithm.id + '" is missing helpFamily metadata.');
            }
        });

        locales.forEach(function (locale) {
            var dictionary = app.i18n[locale] || {};
            var details = algorithmDetails(dictionary.helpBundle);
            algorithms.forEach(function (algorithm) {
                if (!details[algorithm.id]) {
                    errors.push(locale + ' Help is missing algorithm details for "' + algorithm.id + '".');
                } else if (details[algorithm.id].family !== algorithm.helpFamily) {
                    errors.push(
                        locale + ' Help places "' + algorithm.id + '" in "'
                        + details[algorithm.id].family + '" instead of "' + algorithm.helpFamily + '".'
                    );
                }
            });
            Object.keys(details).forEach(function (id) {
                if (!algorithmIds[id]) {
                    warnings.push(locale + ' Help contains unused algorithm details for "' + id + '".');
                }
            });
            ['helpMaxInputLongEdge', 'helpMaxResizeOutputSize'].forEach(function (key) {
                var rendered = app.i18n.interpolate(dictionary[key], { value: 123 });
                if (typeof dictionary[key] !== 'string' || rendered.indexOf('{value}') !== -1) {
                    errors.push(locale + ' i18n template "' + key + '" must replace {value}.');
                }
            });
        });

        ['maxInputLongEdge', 'maxResizeOutputSize'].forEach(function (factId) {
            if (app.app.projectCapabilities.fact(factId) === undefined) {
                errors.push('Project capability fact "' + factId + '" is missing.');
            }
        });

        if (app.i18n.interpolate('{value}/{value}/{other}', { value: 7, other: 9 }) !== '7/7/9') {
            errors.push('Named i18n placeholders do not replace repeated values correctly.');
        }
        if (app.i18n.interpolate('{0}/{0}/{1}', [7, 9]) !== '7/7/9') {
            errors.push('Positional i18n placeholders do not replace repeated values correctly.');
        }

        var englishBundle = app.i18n.en.helpBundle;
        var orderedSection = algorithmSection(englishBundle, 'ordered');
        var simulatedAlgorithm = {
            id: 'help-validation-fallback',
            labelKey: 'helpValidationFallbackLabel',
            processorId: 'ordered',
            helpFamily: 'ordered',
            supportsThresholdStrength: true
        };
        if (!orderedSection) {
            errors.push('English Help is missing an ordered algorithm section.');
        } else {
            var simulatedCards = app.pages.help.contentModel.algorithmsForSection(
                orderedSection,
                englishBundle,
                algorithms.concat([simulatedAlgorithm])
            );
            var fallbackCard = simulatedCards.find(function (card) {
                return card.id === simulatedAlgorithm.id;
            });
            if (!fallbackCard || fallbackCard.summary !== englishBundle.ui.detailsPending) {
                errors.push('New algorithms without authored details do not receive a Help fallback card.');
            }

            var firstOrdered = algorithms.find(function (algorithm) {
                return algorithm.helpFamily === 'ordered';
            });
            if (firstOrdered) {
                var cardsAfterRemoval = app.pages.help.contentModel.algorithmsForSection(
                    orderedSection,
                    englishBundle,
                    algorithms.filter(function (algorithm) {
                        return algorithm.id !== firstOrdered.id;
                    })
                );
                if (cardsAfterRemoval.some(function (card) { return card.id === firstOrdered.id; })) {
                    errors.push('Removed algorithms remain visible in capability-driven Help content.');
                }
            }
        }

        return { errors: errors, warnings: warnings };
    }

    app.pages.help.validation = { validate: validate };
})(window.DitherApp);
