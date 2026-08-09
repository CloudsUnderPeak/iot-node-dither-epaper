(function (app) {
    app.pages.help = app.pages.help || {};

    function capabilityControls(capability, bundle) {
        var controls = [];
        if (capability.supportsErrorStrength) {
            controls.push(app.i18n.t('labelErrorStrength'));
        }
        if (capability.supportsThresholdStrength) {
            controls.push(app.i18n.t('labelDitherStrength'));
        }
        if (capability.supportsDotDensity) {
            controls.push(app.i18n.t('labelDotDensity'));
        }
        if (capability.supportsSerpentine) {
            controls.push(app.i18n.t('labelSerpentine'));
        }
        return controls.length ? controls.join(bundle.ui.listSeparator) : bundle.ui.noExtraControls;
    }

    function tableCellText(cell, bundle) {
        if (!cell || typeof cell !== 'object') {
            return cell;
        }
        var value = app.app.projectCapabilities.fact(cell.fact);
        if (value === undefined || value === null) {
            return bundle.ui.detailsPending;
        }
        return app.i18n.t(cell.key, { value: value });
    }

    function algorithmsForSection(section, bundle, capabilities) {
        if (!section.capabilityFamily) {
            return section.algorithms || [];
        }
        var details = {};
        (section.algorithms || []).forEach(function (algorithm) {
            details[algorithm.id] = algorithm;
        });
        return (capabilities || app.app.projectCapabilities.list('dither-algorithms'))
            .filter(function (algorithm) {
                return algorithm.helpFamily === section.capabilityFamily;
            })
            .map(function (algorithm) {
                var detail = details[algorithm.id] || {};
                return {
                    id: algorithm.id,
                    name: app.i18n.t(algorithm.labelKey),
                    summary: detail.summary || bundle.ui.detailsPending,
                    characteristics: detail.characteristics || algorithm.processorId,
                    bestFor: detail.bestFor || bundle.ui.detailsPending,
                    avoid: detail.avoid || bundle.ui.detailsPending,
                    controls: detail.controls || capabilityControls(algorithm, bundle),
                    structure: detail.structure || algorithm.matrixId || algorithm.processorId
                };
            });
    }

    app.pages.help.contentModel = {
        algorithmsForSection: algorithmsForSection,
        tableCellText: tableCellText
    };
})(window.DitherApp);
