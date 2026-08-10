(function (app) {
    var epaperUnsubscribe = null;
    var calibrationUnsubscribe = null;
    var gate = null;
    var refs = {};
    var draftColors = [];
    var baselineColors = [];
    var baselineRevision = -1;
    var dirty = false;
    var externalChanged = false;
    var localMutation = false;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function copyColors(colors) {
        return colors.map(function (color) {
            return {
                id: color.id,
                code: color.code,
                r: String(color.r),
                g: String(color.g),
                b: String(color.b)
            };
        });
    }

    function field(labelKey, valueNode) {
        return app.utils.dom.el('div', {
            className: 'device-field',
            children: [
                app.utils.dom.el('div', { className: 'device-field-label', text: t(labelKey) }),
                valueNode
            ]
        });
    }

    function actionButton(key, action) {
        var button = app.utils.dom.el('button', {
            className: action === 'refresh' ? 'secondary-button' : 'primary-button',
            text: t(key),
            attrs: { type: 'button' }
        });
        button.addEventListener('click', function () {
            refs.notice.clear();
            app.device.epaper.runAction(action).catch(function (error) {
                refs.notice.set(app.device.errorText(error), { error: true, sticky: true });
            });
        });
        refs.buttons[action] = button;
        return button;
    }

    function renderEpaper(snapshot) {
        if (!refs.state) {
            return;
        }
        var status = snapshot.status || {};
        var stored = status.stored_image || {};
        var panel = snapshot.capabilities && snapshot.capabilities.panel || {};
        refs.panel.textContent = panel.model
            ? t('epaperPanelSummary', {
                model: panel.model,
                width: panel.width,
                height: panel.height,
                colors: panel.colors
            })
            : '--';
        refs.state.textContent = status.state || '--';
        var capabilities = snapshot.capabilities && snapshot.capabilities.capabilities || {};
        var enabled = app.device.epaper.canDraw();
        refs.buttons.white.disabled = !enabled || capabilities.white === false;
        refs.buttons.palette.disabled = !enabled || capabilities.palette === false;
        refs.buttons.refresh.disabled = !enabled || capabilities.refresh === false || !stored.available || !stored.valid;
        Object.keys(refs.buttons).forEach(function (key) {
            refs.buttons[key].setAttribute('aria-disabled', refs.buttons[key].disabled ? 'true' : 'false');
        });
        refs.cooldown.textContent = snapshot.cooldownRemainingSeconds
            ? t('epaperCooldownButton', { seconds: snapshot.cooldownRemainingSeconds })
            : t('epaperCooldownNone');
    }

    function channelValue(value) {
        if (!/^(?:0|[1-9][0-9]{0,2})$/.test(value)) {
            return null;
        }
        var number = Number(value);
        return number <= 255 ? number : null;
    }

    function validateDraft() {
        var seen = {};
        var colors = [];
        for (var index = 0; index < draftColors.length; index += 1) {
            var draft = draftColors[index];
            var r = channelValue(draft.r);
            var g = channelValue(draft.g);
            var b = channelValue(draft.b);
            if (r === null || g === null || b === null) {
                return { valid: false, errorKey: 'epaperCalibrationInvalidChannel' };
            }
            var rgbKey = r + ',' + g + ',' + b;
            if (seen[rgbKey]) {
                return { valid: false, errorKey: 'epaperCalibrationDuplicate' };
            }
            seen[rgbKey] = true;
            colors.push({ id: draft.id, code: draft.code, r: r, g: g, b: b });
        }
        return { valid: true, colors: colors };
    }

    function rgbHex(color) {
        function hex(value) {
            return Math.max(0, Math.min(255, value)).toString(16).padStart(2, '0');
        }
        return '#' + hex(Number(color.r)) + hex(Number(color.g)) + hex(Number(color.b));
    }

    function applyHex(index, value) {
        if (!/^#[0-9a-f]{6}$/i.test(value)) {
            return;
        }
        draftColors[index].r = String(parseInt(value.slice(1, 3), 16));
        draftColors[index].g = String(parseInt(value.slice(3, 5), 16));
        draftColors[index].b = String(parseInt(value.slice(5, 7), 16));
        ['r', 'g', 'b'].forEach(function (channel) {
            refs.colorRows[index].inputs[channel].value = draftColors[index][channel];
        });
    }

    function refreshColorPreview(index) {
        var color = draftColors[index];
        var r = channelValue(color.r);
        var g = channelValue(color.g);
        var b = channelValue(color.b);
        if (r === null || g === null || b === null) {
            refs.colorRows[index].preview.classList.add('is-invalid');
            return;
        }
        var value = rgbHex({ r: r, g: g, b: b });
        refs.colorRows[index].preview.classList.remove('is-invalid');
        refs.colorRows[index].preview.style.backgroundColor = value;
        refs.colorRows[index].picker.value = value;
        refs.strip[index].style.backgroundColor = value;
    }

    function sameDraft(left, right) {
        return left.length === right.length && left.every(function (color, index) {
            var other = right[index];
            return other && color.id === other.id && color.code === other.code
                && color.r === other.r && color.g === other.g && color.b === other.b;
        });
    }

    function updateDraftState() {
        dirty = !sameDraft(draftColors, baselineColors);
        renderCalibration(app.device.epaperCalibration.snapshot());
    }

    function rebase(snapshot) {
        draftColors = copyColors(snapshot.colors);
        baselineColors = copyColors(snapshot.colors);
        baselineRevision = snapshot.revision;
        dirty = false;
        externalChanged = false;
        refs.colorRows.forEach(function (row, index) {
            ['r', 'g', 'b'].forEach(function (channel) {
                row.inputs[channel].value = draftColors[index][channel];
            });
            refreshColorPreview(index);
        });
    }

    function renderCalibration(snapshot) {
        if (!refs.calibrationState) {
            return;
        }
        var validation = validateDraft();
        var offline = app.device.live.state() !== 'online';
        var busy = snapshot.loading || snapshot.saving;
        var stateKey = 'epaperCalibrationStateClean';
        if (snapshot.loading) {
            stateKey = 'epaperCalibrationStateLoading';
        } else if (snapshot.saving) {
            stateKey = 'epaperCalibrationStateSaving';
        } else if (externalChanged) {
            stateKey = 'epaperCalibrationStateExternalChanged';
        } else if (!validation.valid) {
            stateKey = validation.errorKey;
        } else if (dirty) {
            stateKey = 'epaperCalibrationStateDirty';
        } else if (!snapshot.synced) {
            stateKey = 'epaperCalibrationStateUnsynced';
        }
        refs.calibrationState.textContent = t(stateKey);
        refs.saveCalibration.disabled = offline || busy || !dirty || !validation.valid || externalChanged;
        refs.reloadCalibration.disabled = offline || busy;
        refs.resetCalibration.disabled = offline || busy;
        refs.calibrationFieldset.disabled = offline || busy;
    }

    function onCalibrationSnapshot(snapshot) {
        if (!localMutation && snapshot.revision !== baselineRevision) {
            if (dirty) {
                externalChanged = true;
            } else {
                rebase(snapshot);
            }
        }
        renderCalibration(snapshot);
    }

    function saveCalibration() {
        var validation = validateDraft();
        if (!validation.valid || externalChanged) {
            renderCalibration(app.device.epaperCalibration.snapshot());
            return;
        }
        localMutation = true;
        refs.notice.clear();
        app.device.epaperCalibration.save(validation.colors).then(function (snapshot) {
            rebase(snapshot);
            refs.notice.set(t('epaperCalibrationSavedNotice'), { sticky: true });
        }).catch(function (error) {
            refs.notice.set(app.device.errorText(error), { error: true, sticky: true });
        }).finally(function () {
            localMutation = false;
            renderCalibration(app.device.epaperCalibration.snapshot());
        });
    }

    function reloadCalibration() {
        if (dirty && !window.confirm(t('epaperCalibrationReloadConfirm'))) {
            return;
        }
        localMutation = true;
        refs.notice.clear();
        app.device.epaperCalibration.load(true).then(function (snapshot) {
            rebase(snapshot);
        }).catch(function (error) {
            refs.notice.set(app.device.errorText(error), { error: true, sticky: true });
        }).finally(function () {
            localMutation = false;
            renderCalibration(app.device.epaperCalibration.snapshot());
        });
    }

    function resetCalibration() {
        if (!window.confirm(t('epaperCalibrationResetConfirm'))) {
            return;
        }
        localMutation = true;
        refs.notice.clear();
        app.device.epaperCalibration.reset().then(function (snapshot) {
            rebase(snapshot);
            refs.notice.set(t('epaperCalibrationResetNotice'), { sticky: true });
        }).catch(function (error) {
            refs.notice.set(app.device.errorText(error), { error: true, sticky: true });
        }).finally(function () {
            localMutation = false;
            renderCalibration(app.device.epaperCalibration.snapshot());
        });
    }

    function channelInput(index, channel) {
        var input = app.utils.dom.el('input', {
            className: 'device-input epaper-calibration-channel',
            attrs: { type: 'number', min: '0', max: '255', step: '1', inputmode: 'numeric' }
        });
        input.value = draftColors[index][channel];
        input.addEventListener('input', function () {
            draftColors[index][channel] = input.value;
            refreshColorPreview(index);
            updateDraftState();
        });
        return app.utils.dom.el('label', {
            className: 'epaper-calibration-channel-wrap',
            children: [app.utils.dom.el('span', { text: channel.toUpperCase() }), input]
        });
    }

    function calibrationRow(color, index) {
        var picker = app.utils.dom.el('input', {
            className: 'epaper-calibration-picker',
            attrs: { type: 'color', 'aria-label': t('epaperCalibrationPickerLabel', { color: t('epaperCalibrationColor_' + color.id) }) }
        });
        var preview = app.utils.dom.el('span', { className: 'epaper-calibration-preview' });
        refs.colorRows[index] = { picker: picker, preview: preview, inputs: {} };
        var channels = ['r', 'g', 'b'].map(function (channel) {
            var wrap = channelInput(index, channel);
            refs.colorRows[index].inputs[channel] = wrap.lastChild;
            return wrap;
        });
        picker.value = rgbHex(color);
        picker.addEventListener('input', function () {
            applyHex(index, picker.value);
            refreshColorPreview(index);
            updateDraftState();
        });
        return app.utils.dom.el('div', {
            className: 'epaper-calibration-row',
            children: [
                app.utils.dom.el('div', {
                    className: 'epaper-calibration-name',
                    children: [
                        preview,
                        app.utils.dom.el('span', { text: t('epaperCalibrationColor_' + color.id) }),
                        app.utils.dom.el('span', { className: 'device-badge is-muted', text: t('epaperCalibrationCode', { code: color.code }) })
                    ]
                }),
                picker,
                app.utils.dom.el('div', { className: 'epaper-calibration-channels', children: channels })
            ]
        });
    }

    function calibrationCard() {
        var snapshot = app.device.epaperCalibration.snapshot();
        draftColors = copyColors(snapshot.colors);
        baselineColors = copyColors(snapshot.colors);
        baselineRevision = snapshot.revision;
        refs.colorRows = [];
        refs.strip = draftColors.map(function (color) {
            return app.utils.dom.el('span', { attrs: { title: t('epaperCalibrationColor_' + color.id) } });
        });
        refs.calibrationState = app.utils.dom.el('div', { className: 'device-hint epaper-calibration-state' });
        refs.saveCalibration = app.utils.dom.el('button', {
            className: 'primary-button', text: t('epaperCalibrationSave'), attrs: { type: 'button' }
        });
        refs.reloadCalibration = app.utils.dom.el('button', {
            className: 'secondary-button', text: t('epaperCalibrationReload'), attrs: { type: 'button' }
        });
        refs.resetCalibration = app.utils.dom.el('button', {
            className: 'secondary-button', text: t('epaperCalibrationReset'), attrs: { type: 'button' }
        });
        refs.saveCalibration.addEventListener('click', saveCalibration);
        refs.reloadCalibration.addEventListener('click', reloadCalibration);
        refs.resetCalibration.addEventListener('click', resetCalibration);
        refs.calibrationFieldset = app.utils.dom.el('fieldset', {
            className: 'device-fieldset',
            children: [
                app.utils.dom.el('p', { className: 'device-hint', text: t('epaperCalibrationHint') }),
                app.utils.dom.el('div', { className: 'epaper-calibration-strip', children: refs.strip }),
                app.utils.dom.el('div', {
                    className: 'epaper-calibration-grid',
                    children: draftColors.map(calibrationRow)
                }),
                app.utils.dom.el('div', {
                    className: 'save-dock',
                    children: [
                        refs.calibrationState,
                        app.utils.dom.el('div', {
                            className: 'device-actions',
                            children: [refs.reloadCalibration, refs.resetCalibration, refs.saveCalibration]
                        })
                    ]
                })
            ]
        });
        refs.colorRows.forEach(function (row, index) { refreshColorPreview(index); });
        return app.utils.dom.el('section', {
            className: 'panel-section device-gate epaper-calibration-card',
            children: [
                app.utils.dom.el('h2', { text: t('epaperCalibrationCardTitle') }),
                app.utils.dom.el('div', { className: 'panel-body device-card-body', children: [refs.calibrationFieldset] })
            ]
        });
    }

    app.pages.deviceEpaperTestPage = {
        id: 'device-epaper-test',
        titleKey: 'deviceEpaperTestTitle',
        mount: function mount(container) {
            refs = { buttons: {} };
            refs.notice = app.ui.createNotice();
            refs.panel = app.utils.dom.el('div', { className: 'device-field-value' });
            refs.state = app.utils.dom.el('div', { className: 'device-field-value' });
            refs.cooldown = app.utils.dom.el('div', { className: 'device-field-value' });
            var section = app.utils.dom.el('section', {
                className: 'device-page',
                children: [
                    refs.notice.node,
                    app.utils.dom.el('section', {
                        className: 'panel-section device-gate',
                        children: [
                            app.utils.dom.el('h2', { text: t('epaperStatusCardTitle') }),
                            app.utils.dom.el('div', {
                                className: 'panel-body device-card-body',
                                children: [app.utils.dom.el('div', {
                                    className: 'device-grid',
                                    children: [field('epaperPanelLabel', refs.panel), field('epaperStateLabel', refs.state), field('epaperCooldownLabel', refs.cooldown)]
                                })]
                            })
                        ]
                    }),
                    app.utils.dom.el('section', {
                        className: 'panel-section device-gate',
                        children: [
                            app.utils.dom.el('h2', { text: t('epaperDiagnosticsCardTitle') }),
                            app.utils.dom.el('div', {
                                className: 'panel-body device-card-body',
                                children: [
                                    app.utils.dom.el('div', { className: 'device-hint', text: t('epaperTestHint') }),
                                    app.utils.dom.el('div', {
                                        className: 'device-actions',
                                        children: [actionButton('epaperActionWhite', 'white'), actionButton('epaperActionPalette', 'palette'), actionButton('epaperActionRefresh', 'refresh')]
                                    })
                                ]
                            })
                        ]
                    }),
                    calibrationCard()
                ]
            });
            gate = app.device.bindLiveGate(section, {
                onOnline: function () {
                    app.device.epaperCalibration.load(true).catch(function () {});
                }
            });
            section.insertBefore(gate.banner, section.children[0]);
            container.appendChild(section);
            epaperUnsubscribe = app.device.epaper.subscribe(renderEpaper);
            calibrationUnsubscribe = app.device.epaperCalibration.subscribe(onCalibrationSnapshot);
            renderEpaper(app.device.epaper.snapshot());
            renderCalibration(app.device.epaperCalibration.snapshot());
            app.device.epaper.refreshStatus().catch(function () {});
            app.device.epaperCalibration.load(false).catch(function () {});
        },
        unmount: function unmount() {
            if (epaperUnsubscribe) {
                epaperUnsubscribe();
                epaperUnsubscribe = null;
            }
            if (calibrationUnsubscribe) {
                calibrationUnsubscribe();
                calibrationUnsubscribe = null;
            }
            if (gate) {
                gate.unbind();
                gate = null;
            }
            refs = {};
        }
    };
})(window.DitherApp);
