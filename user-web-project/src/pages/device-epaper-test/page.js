(function (app) {
    var unsubscribe = null;
    var refs = {};

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
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

    function render(snapshot) {
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
            : '—';
        refs.state.textContent = status.state || '—';
        var capabilities = snapshot.capabilities && snapshot.capabilities.capabilities || {};
        var enabled = app.device.epaper.canDraw();
        refs.buttons.white.disabled = !enabled || capabilities.white === false;
        refs.buttons.palette.disabled = !enabled || capabilities.palette === false;
        refs.buttons.refresh.disabled = !enabled || capabilities.refresh === false || !stored.available || !stored.valid;
        Object.keys(refs.buttons).forEach(function (key) {
            refs.buttons[key].setAttribute('aria-disabled', refs.buttons[key].disabled ? 'true' : 'false');
        });
        if (snapshot.cooldownRemainingSeconds) {
            refs.cooldown.textContent = t('epaperCooldownButton', { seconds: snapshot.cooldownRemainingSeconds });
        } else {
            refs.cooldown.textContent = t('epaperCooldownNone');
        }
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
            var actions = app.utils.dom.el('div', {
                className: 'device-actions',
                children: [
                    actionButton('epaperActionWhite', 'white'),
                    actionButton('epaperActionPalette', 'palette'),
                    actionButton('epaperActionRefresh', 'refresh')
                ]
            });
            container.appendChild(app.utils.dom.el('section', {
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
                                    children: [
                                        field('epaperPanelLabel', refs.panel),
                                        field('epaperStateLabel', refs.state),
                                        field('epaperCooldownLabel', refs.cooldown)
                                    ]
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
                                    actions
                                ]
                            })
                        ]
                    })
                ]
            }));
            unsubscribe = app.device.epaper.subscribe(render);
            render(app.device.epaper.snapshot());
            app.device.epaper.refreshStatus().catch(function () {});
        },
        unmount: function unmount() {
            if (unsubscribe) {
                unsubscribe();
                unsubscribe = null;
            }
            refs = {};
        }
    };
})(window.DitherApp);
