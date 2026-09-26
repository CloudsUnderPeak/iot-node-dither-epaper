(function (app) {
    function t(key, values) { return app.i18n.t(key, values); }

    function formatEpoch(epoch) {
        return typeof epoch === 'number' && Number.isFinite(epoch)
            ? new Date(epoch * 1000).toLocaleTimeString(app.i18n.currentLanguage(), {
                hour: '2-digit', minute: '2-digit', hour12: false
            }) : t('sleepUnknownTime');
    }

    function infoField(labelKey, value, fullWidth) {
        return app.utils.dom.el('div', {
            className: 'device-field' + (fullWidth ? ' device-sleep-blockers' : ''),
            children: [app.utils.dom.el('span', { className: 'device-field-label', text: t(labelKey) }), value]
        });
    }

    function createPage(host, page) {
        var service = app.device.sleep;
        var notice = app.ui.createNotice();
        var validation = app.ui.createNotice();
        var statusNotice = app.ui.createNotice();
        var summary = app.utils.dom.el('span', {
            className: 'device-field-value', text: t('deviceLoading'), attrs: { 'aria-live': 'polite' }
        });
        var deviceTime = app.utils.dom.el('span', { className: 'device-field-value', text: t('sleepUnknownTime') });
        var browserTime = app.utils.dom.el('span', { className: 'device-field-value', text: t('sleepUnknownTime') });
        var next = app.utils.dom.el('span', { className: 'device-field-value', text: t('sleepUnknownTime') });
        var blockers = app.utils.dom.el('span', { className: 'device-field-value', text: t('sleepUnknownTime') });
        var enabled = app.utils.dom.el('input', {
            attrs: { type: 'checkbox', 'aria-labelledby': 'sleep-enable-label' }
        });
        var period = app.utils.dom.el('select', { className: 'device-input', attrs: { id: 'sleep-period' } });
        [12, 24, 48].forEach(function (hours) {
            var option = app.utils.dom.el('option', { text: t('sleepPeriodOptionHours', { hours: hours }), attrs: { value: String(hours * 60) } });
            period.appendChild(option);
        });
        function timeSelect(labelKey, name, count) {
            var select = app.utils.dom.el('select', {
                className: 'device-input', attrs: {
                    name: name, 'aria-label': t('sleepLocalTime') + ' (' + t(labelKey) + ')'
                }
            });
            for (var value = 0; value < count; value += 1) {
                select.appendChild(app.utils.dom.el('option', {
                    text: String(value).padStart(2, '0'), attrs: { value: String(value) }
                }));
            }
            return select;
        }
        var hour = timeSelect('sleepHour', 'sleep-hour', 24);
        var minute = timeSelect('sleepMinute', 'sleep-minute', 60);
        var timeControls = app.utils.dom.el('div', {
            className: 'device-sleep-time',
            children: [app.ui.selectField.create(hour),
                app.utils.dom.el('span', { text: ':', attrs: { 'aria-hidden': 'true' } }),
                app.ui.selectField.create(minute)]
        });
        var save = app.utils.dom.el('button', {
            className: 'primary-button', text: t('deviceSave'), attrs: { type: 'button' }
        });
        var draftInitialized = false;
        var busy = false;

        function selectedDelay(currentTime) {
            var target = new Date(currentTime);
            target.setHours(Number(hour.value), Number(minute.value), 0, 0);
            if (target.getTime() <= currentTime) { target.setDate(target.getDate() + 1); }
            return Math.ceil((target.getTime() - currentTime) / 60000);
        }

        function invalidTimeText() {
            var minutes = Number(period.value);
            return [720, 1440, 2880].indexOf(minutes) === -1
                ? t('sleepInvalidTimeMinutes', { minutes: minutes })
                : t('sleepInvalidTime', { hours: minutes / 60 });
        }

        function updateDraft() {
            var minutes = selectedDelay(Date.now());
            var max = Number(period.value);
            var valid = Number.isInteger(minutes) && minutes >= 1 && minutes <= max;
            enabled.disabled = busy;
            period.disabled = busy || !enabled.checked;
            hour.disabled = busy || !enabled.checked;
            minute.disabled = busy || !enabled.checked;
            validation.set(enabled.checked && !valid ? invalidTimeText() : '', { error: true });
            save.disabled = busy || !draftInitialized || (enabled.checked && !valid);
        }

        [enabled, period, hour, minute].forEach(function (control) {
            control.addEventListener('input', updateDraft);
            control.addEventListener('change', updateDraft);
        });

        function render(snapshot) {
            if (!page.mounted) { return; }
            var status = snapshot.status;
            if (!snapshot.supported) {
                summary.textContent = snapshot.error ? app.device.errorText(snapshot.error) : t('sleepUnsupported');
                save.disabled = true;
                return;
            }
            if (!status) {
                summary.textContent = snapshot.error ? app.device.errorText(snapshot.error) : t('deviceLoading');
                return;
            }
            if (!draftInitialized) {
                enabled.checked = Boolean(status.enabled);
                var savedPeriod = status.schedule && status.schedule.period_minutes || 1440;
                if ([720, 1440, 2880].indexOf(savedPeriod) === -1) {
                    period.appendChild(app.utils.dom.el('option', {
                        text: t('sleepCustomMinutes', { minutes: savedPeriod }),
                        attrs: { value: String(savedPeriod) }
                    }));
                }
                period.value = String(savedPeriod);
                var wakeEpoch = status.schedule && status.schedule.next_wake_epoch;
                var wakeTime = new Date(typeof wakeEpoch === 'number' && Number.isFinite(wakeEpoch)
                    ? wakeEpoch * 1000 : Date.now() + Math.min(savedPeriod, 60) * 60000);
                hour.value = String(wakeTime.getHours());
                minute.value = String(wakeTime.getMinutes());
                draftInitialized = true;
                updateDraft();
            }
            summary.textContent = status.enabled ? t('sleepEnabled') : t('sleepDisabled');
            deviceTime.textContent = status.time && status.time.synced ? formatEpoch(status.time.epoch) : t('sleepUnsynced');
            browserTime.textContent = formatEpoch(Date.now() / 1000);
            var schedule = status.schedule || {};
            next.textContent = schedule.clock_basis === 'relative'
                ? t('sleepRelativeNext', { seconds: schedule.next_wake_in_seconds || 0 })
                : formatEpoch(schedule.next_wake_epoch);
            blockers.textContent = Array.isArray(status.blockers) && status.blockers.length
                ? status.blockers.join(', ') : t('sleepNoBlockers');
            if (status.sleep_request && status.sleep_request.state === 'failed') {
                statusNotice.set(t('sleepRequestFailed', { code: status.sleep_request.error_code || '' }), { error: true });
            } else {
                statusNotice.clear();
            }
        }

        save.addEventListener('click', function () {
            if (busy) { return; }
            var payload = { enabled: enabled.checked };
            if (enabled.checked) {
                var currentTime = Date.now();
                var minutes = selectedDelay(currentTime);
                var max = Number(period.value);
                if (!Number.isInteger(minutes) || minutes < 1 || minutes > max) {
                    notice.set(invalidTimeText(), { error: true });
                    updateDraft();
                    return;
                }
                payload.period_minutes = Number(period.value);
                payload.first_wake_delay_minutes = minutes;
                payload.client_time = Math.floor(currentTime / 1000);
            }
            busy = true;
            updateDraft();
            service.update(payload).then(function () {
                notice.set(t('sleepSaved'), { sticky: true });
            }, function (error) {
                notice.set(app.device.errorText(error), { error: true });
            }).finally(function () { busy = false; updateDraft(); });
        });

        host.appendChild(app.utils.dom.el('section', {
            className: 'panel-section device-gate device-sleep-info',
            children: [
                app.utils.dom.el('h2', { text: t('sleepInfoTitle') }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [app.utils.dom.el('div', {
                        className: 'device-grid',
                        children: [infoField('sleepScheduleStatus', summary), infoField('sleepNextWake', next),
                            infoField('sleepDeviceTime', deviceTime), infoField('sleepBrowserTime', browserTime),
                            infoField('sleepBlockers', blockers, true)]
                    }), statusNotice.node]
                })]
        }));
        host.appendChild(app.utils.dom.el('section', {
            className: 'panel-section device-gate device-sleep-card',
            children: [
                app.utils.dom.el('h2', { text: t('sleepSettingsTitle') }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [
                        app.utils.dom.el('fieldset', {
                            className: 'device-fieldset device-sleep-form',
                            children: [
                                app.utils.dom.el('span', {
                                    className: 'device-form-label', text: t('sleepEnable'),
                                    attrs: { id: 'sleep-enable-label' }
                                }),
                                app.utils.dom.el('label', {
                                    className: 'toggle-switch',
                                    children: [enabled, app.utils.dom.el('span', {
                                        className: 'toggle-switch-track', attrs: { 'aria-hidden': 'true' }
                                    })]
                                }),
                                app.utils.dom.el('label', {
                                    className: 'device-form-label', text: t('sleepPeriodHours'),
                                    attrs: { for: 'sleep-period' }
                                }),
                                app.ui.selectField.create(period),
                                app.utils.dom.el('span', { className: 'device-form-label', text: t('sleepLocalTime') }),
                                timeControls, validation.node, notice.node,
                                app.utils.dom.el('div', { className: 'device-actions', children: [save] })]
                        })]
                })]
        }));
        page.sleepUnsubscribe = service.subscribe(render);
        updateDraft();
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
                if (!app.device.auth.canManage()) {
                    content.appendChild(app.device.auth.createLockedCard({ onUnlocked: render }));
                    return;
                }
                if (page.sleepUnsubscribe) { page.sleepUnsubscribe(); page.sleepUnsubscribe = null; }
                createPage(content, page);
                app.device.auth.ensureSession().then(function (valid) {
                    if (page.mounted && !valid && !app.device.auth.canManage()) { render(); }
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
