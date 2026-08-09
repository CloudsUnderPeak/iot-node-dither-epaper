(function (app) {
    var unsubscribe = null;
    var refs = {};

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
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
        refs.state.textContent = t('epaperStateLabel') + ': ' + (status.state || '—');
        refs.stored.textContent = stored.available && stored.valid
            ? t('epaperStoredImageAvailable')
            : t('epaperStoredImageMissing');
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
            refs.cooldown.textContent = '';
        }
    }

    app.pages.deviceEpaperTestPage = {
        id: 'device-epaper-test',
        titleKey: 'deviceEpaperTestTitle',
        mount: function mount(container) {
            refs = { buttons: {} };
            refs.notice = app.ui.createNotice();
            refs.state = app.utils.dom.el('div', { className: 'device-field-value' });
            refs.stored = app.utils.dom.el('div', { className: 'device-hint' });
            refs.cooldown = app.utils.dom.el('div', { className: 'device-hint' });
            var actions = app.utils.dom.el('div', {
                className: 'device-actions epaper-test-actions',
                children: [
                    actionButton('epaperActionWhite', 'white'),
                    actionButton('epaperActionPalette', 'palette'),
                    actionButton('epaperActionRefresh', 'refresh')
                ]
            });
            container.appendChild(app.utils.dom.el('section', {
                className: 'device-page',
                children: [
                    app.utils.dom.el('h1', { text: t('deviceEpaperTestTitle') }),
                    app.utils.dom.el('p', { className: 'device-hint', text: t('epaperTestHint') }),
                    refs.notice.node,
                    app.utils.dom.el('section', {
                        className: 'panel-section',
                        children: [app.utils.dom.el('div', {
                            className: 'panel-body device-card-body',
                            children: [refs.state, refs.stored, refs.cooldown, actions]
                        })]
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
