(function (app) {
    function t(key, values) { return app.i18n.t(key, values); }

    function row(label, input) {
        return app.utils.dom.el('label', {
            className: 'device-form-row',
            children: [app.utils.dom.el('span', { className: 'device-form-label', text: label }), input]
        });
    }

    function formatEpoch(epoch) {
        return typeof epoch === 'number' && Number.isFinite(epoch)
            ? new Date(epoch * 1000).toLocaleString() : t('sleepUnknownTime');
    }

    function createPage(host, page) {
        var service = app.device.sleep;
        var notice = app.ui.createNotice();
        var summary = app.utils.dom.el('p', { className: 'device-hint', attrs: { 'aria-live': 'polite' } });
        var time = app.utils.dom.el('p', { className: 'device-hint' });
        var next = app.utils.dom.el('p', { className: 'device-hint' });
        var blockers = app.utils.dom.el('p', { className: 'device-hint' });
        var enabled = app.utils.dom.el('input', { attrs: { type: 'checkbox' } });
        var period = app.utils.dom.el('select', { className: 'device-input' });
        [12, 24, 48].forEach(function (hours) {
            var option = app.utils.dom.el('option', { text: String(hours), attrs: { value: String(hours) } });
            period.appendChild(option);
        });
        var method = app.utils.dom.el('select', { className: 'device-input' });
        method.appendChild(app.utils.dom.el('option', { text: t('sleepDelayMode'), attrs: { value: 'delay' } }));
        method.appendChild(app.utils.dom.el('option', { text: t('sleepLocalMode'), attrs: { value: 'local' } }));
        var delay = app.utils.dom.el('input', {
            className: 'device-input', attrs: { type: 'number', min: '1', step: '1', value: '60' }
        });
        var local = app.utils.dom.el('input', { className: 'device-input', attrs: { type: 'datetime-local' } });
        var delayRow = row(t('sleepDelayMinutes'), delay);
        var localRow = row(t('sleepLocalTime'), local);
        var preview = app.utils.dom.el('p', { className: 'device-hint' });
        var save = app.utils.dom.el('button', {
            className: 'primary-button', text: t('deviceSave'), attrs: { type: 'button' }
        });
        var now = app.utils.dom.el('button', {
            className: 'secondary-button', text: t('sleepNow'), attrs: { type: 'button' }
        });
        var draftInitialized = false;
        var busy = false;

        function selectedDelay() {
            if (method.value === 'delay') { return Number(delay.value); }
            var target = new Date(local.value).getTime();
            return Number.isFinite(target) ? Math.ceil((target - Date.now()) / 60000) : NaN;
        }

        function updateDraft() {
            var minutes = selectedDelay();
            var max = Number(period.value) * 60;
            var valid = Number.isInteger(minutes) && minutes >= 1 && minutes <= max;
            delayRow.hidden = method.value !== 'delay';
            localRow.hidden = method.value !== 'local';
            preview.textContent = !enabled.checked ? t('sleepDisabledHint') :
                valid ? t('sleepFirstWakePreview', { minutes: minutes }) : t('sleepInvalidDelay');
            save.disabled = busy || (enabled.checked && !valid);
        }

        [enabled, period, method, delay, local].forEach(function (control) {
            control.addEventListener('input', updateDraft);
            control.addEventListener('change', updateDraft);
        });

        function render(snapshot) {
            if (!page.mounted) { return; }
            var status = snapshot.status;
            if (!snapshot.supported) {
                summary.textContent = snapshot.error ? app.device.errorText(snapshot.error) : t('sleepUnsupported');
                save.disabled = true;
                now.disabled = true;
                return;
            }
            if (!status) {
                summary.textContent = snapshot.error ? app.device.errorText(snapshot.error) : t('deviceLoading');
                return;
            }
            if (!draftInitialized) {
                enabled.checked = Boolean(status.enabled);
                period.value = String(status.schedule && status.schedule.period_hours || 24);
                delay.value = '60';
                draftInitialized = true;
                updateDraft();
            }
            summary.textContent = status.enabled ? t('sleepEnabled') : t('sleepDisabled');
            var deviceTime = status.time && status.time.synced ? formatEpoch(status.time.epoch) : t('sleepUnsynced');
            time.textContent = t('sleepTimeSummary', {
                device: deviceTime, browser: new Date().toLocaleString()
            });
            var schedule = status.schedule || {};
            next.textContent = schedule.clock_basis === 'relative'
                ? t('sleepRelativeNext', { seconds: schedule.next_wake_in_seconds || 0 })
                : t('sleepNextWake', { time: formatEpoch(schedule.next_wake_epoch) });
            blockers.textContent = Array.isArray(status.blockers) && status.blockers.length
                ? t('sleepBlockers', { codes: status.blockers.join(', ') }) : '';
            now.disabled = busy || !status.enabled || app.device.live.state() !== 'online';
            if (status.sleep_request && status.sleep_request.state === 'failed') {
                notice.set(t('sleepRequestFailed', { code: status.sleep_request.error_code || '' }), { error: true });
            }
        }

        save.addEventListener('click', function () {
            if (busy) { return; }
            var payload = { enabled: enabled.checked };
            if (enabled.checked) {
                var minutes = selectedDelay();
                var max = Number(period.value) * 60;
                if (!Number.isInteger(minutes) || minutes < 1 || minutes > max) {
                    notice.set(t('sleepInvalidDelay'), { error: true });
                    updateDraft();
                    return;
                }
                payload.period_hours = Number(period.value);
                payload.first_wake_delay_minutes = minutes;
                payload.client_time = Math.floor(Date.now() / 1000);
            }
            busy = true;
            updateDraft();
            service.update(payload).then(function () {
                notice.set(t('sleepSaved'), { sticky: true });
            }, function (error) {
                notice.set(app.device.errorText(error), { error: true });
            }).finally(function () { busy = false; updateDraft(); });
        });

        now.addEventListener('click', function () {
            var confirm = app.utils.dom.el('button', {
                className: 'primary-button', text: t('sleepNow'), attrs: { type: 'button' }
            });
            var cancel = app.utils.dom.el('button', {
                className: 'secondary-button', text: t('deviceCancel'), attrs: { type: 'button' }
            });
            var dialog = app.utils.dom.el('section', {
                className: 'device-dialog',
                children: [
                    app.utils.dom.el('h2', { text: t('sleepNowConfirmTitle') }),
                    app.utils.dom.el('p', { text: t('sleepNowConfirmBody') }),
                    app.utils.dom.el('div', { className: 'modal-actions', children: [confirm, cancel] })
                ]
            });
            cancel.addEventListener('click', function () { app.ui.modal.close(); });
            confirm.addEventListener('click', function () {
                confirm.disabled = true;
                service.now().then(function () {
                    app.ui.modal.close();
                    notice.set(t('sleepNowAccepted'), { sticky: true });
                }, function (error) {
                    app.ui.modal.close();
                    var codes = error && error.data && error.data.blockers;
                    notice.set(Array.isArray(codes) && codes.length ?
                        t('sleepBlockers', { codes: codes.join(', ') }) : app.device.errorText(error),
                        { error: true });
                });
            });
            app.ui.modal.open(dialog, { initialFocus: cancel });
        });

        host.appendChild(app.utils.dom.el('section', {
            className: 'panel-section device-gate',
            children: [
                app.utils.dom.el('h2', { text: t('deviceSleepTitle') }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [summary, time, next, blockers, notice.node,
                        app.utils.dom.el('fieldset', {
                            className: 'device-fieldset',
                            children: [row(t('sleepEnable'), enabled), row(t('sleepPeriodHours'), period),
                                row(t('sleepFirstWakeMode'), method), delayRow, localRow,
                                preview, app.utils.dom.el('div', {
                                    className: 'device-actions', children: [save, now]
                                })]
                        })]
                })]
        }));
        page.sleepUnsubscribe = service.subscribe(render);
        render(service.snapshot());
        service.refresh().catch(function () {});
    }

    app.pages.deviceSleepPage = {
        id: 'device-sleep',
        titleKey: 'deviceSleepTitle',
        mount: function (container) {
            var page = this;
            page.mounted = true;
            var content = app.utils.dom.el('div', { className: 'device-content' });
            var section = app.utils.dom.el('section', { className: 'device-page', children: [content] });
            function render() {
                if (!page.mounted) { return; }
                app.utils.dom.clear(content);
                if (!app.device.auth.hasToken()) {
                    content.appendChild(app.device.auth.createLockedCard({ onUnlocked: render }));
                    return;
                }
                if (page.sleepUnsubscribe) { page.sleepUnsubscribe(); page.sleepUnsubscribe = null; }
                createPage(content, page);
                app.device.auth.ensureSession().then(function (valid) {
                    if (page.mounted && !valid && !app.device.auth.hasToken()) { render(); }
                });
            }
            page.authUnsubscribe = app.device.auth.subscribe(render);
            page.gate = app.device.bindLiveGate(section, { onOnline: render });
            section.insertBefore(page.gate.banner, content);
            container.appendChild(section);
            render();
        },
        unmount: function () {
            this.mounted = false;
            if (this.sleepUnsubscribe) { this.sleepUnsubscribe(); this.sleepUnsubscribe = null; }
            if (this.authUnsubscribe) { this.authUnsubscribe(); this.authUnsubscribe = null; }
            if (this.gate) { this.gate.unbind(); this.gate = null; }
        }
    };
})(window.DitherApp);
