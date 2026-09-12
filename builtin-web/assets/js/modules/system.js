let systemFormBaseline = '';
let systemFormBusy = false;

function createResetDialog() {
  const { el, icon, translated } = DeviceConsole.utils.dom;
  return el('section', {
    id: 'resetDialog',
    className: 'dialog confirm-dialog',
    attrs: {
      role: 'dialog',
      'aria-modal': 'true',
      'aria-labelledby': 'resetTitle'
    },
    props: { hidden: true },
    children: [
      el('span', { className: 'danger-icon', children: [icon('warning')] }),
      translated('h2', 'confirmFactoryReset', 'Confirm factory reset', { id: 'resetTitle' }),
      translated(
        'p',
        'resetConfirmBody',
        'All Wi-Fi and admin settings and all user files will be cleared. This cannot be undone.'
      ),
      el('footer', {
        children: [
          translated('button', 'resetDevice', 'Reset device', {
            id: 'confirmResetButton',
            className: 'button danger',
            attrs: { type: 'button' }
          }),
          translated('button', 'cancel', 'Cancel', {
            id: 'cancelResetButton',
            className: 'button',
            attrs: { type: 'button' }
          })
        ]
      })
    ]
  });
}

function mountSystemDialogs() {
  DeviceConsole.ui.dialog.add(createResetDialog());
}

function updateSystemSaveAvailability() {
  if (!$('systemForm') || !systemFormBaseline) return;
  const form = $('systemForm');
  const dirty = $('hostname').value !== systemFormBaseline;
  const valid = form.checkValidity();
  form.classList.toggle('form-edited', dirty);
  $('systemSaveButton').disabled = systemFormBusy || !dirty || !valid;
  if (systemFormBusy) {
    setText('systemSaveState', t('savingSystem'));
    return;
  }
  setText('systemSaveState', !dirty ? t('noChanges') : valid ? t('readyToSave') : t('fixInvalidFields'));
}

function systemFormHasChanges() {
  return Boolean($('hostname')) && Boolean(systemFormBaseline) && $('hostname').value !== systemFormBaseline;
}

function openResetDialog() {
  DeviceConsole.ui.dialog.open('resetDialog');
}

function fillSystemForm(device) {
  if (!$('systemForm') || !device) return;
  $('hostname').value = device.hostname || 'esp32-device';
  systemFormBaseline = $('hostname').value;
  $('systemForm').classList.remove('form-edited');
  updateSystemSaveAvailability();
}

async function saveSystem(event) {
  event.preventDefault();
  const signal = DeviceConsole.utils.requests.signal();
  updateSystemSaveAvailability();
  if ($('systemSaveButton').disabled) return;
  systemFormBusy = true;
  $('systemSaveButton').disabled = true;
  setNotice(t('savingSystem'));
  setText('systemSaveState', t('savingSystem'));
  const submittedHostname = $('hostname').value.trim();
  try {
    const data = await resources.system.update(submittedHostname, { signal });
    if (signal.aborted) return;
    const savedHostname = data.hostname || submittedHostname;
    systemFormBusy = false;
    systemFormBaseline = savedHostname;
    mergeResourceSnapshot('device', { hostname: savedHostname });
    DeviceConsole.app.router.renderCurrent();
    if (!$('systemForm')) return;
    if ($('hostname').value.trim() === submittedHostname) $('hostname').value = savedHostname;
    updateSystemSaveAvailability();
    setTransientNotice(t('systemSaved'));
    setText('systemSaveState', t('systemSaved'));
  } catch (error) {
    if (signal.aborted) return;
    systemFormBusy = false;
    if (!$('systemForm')) return;
    updateSystemSaveAvailability();
    setNotice(DeviceConsole.utils.requests.message(error), true);
    setText('systemSaveState', DeviceConsole.utils.requests.message(error));
  }
}

async function resetSystem() {
  const signal = DeviceConsole.utils.requests.signal();
  const confirmButton = $('confirmResetButton');
  confirmButton.disabled = true;
  try {
    await resources.system.reset({ signal });
    if (signal.aborted) return;
    DeviceConsole.ui.dialog.close();
    setToken('');
    DeviceConsole.app.router.navigate('network');
    setNotice(t('restarting'));
  } catch (error) {
    if (signal.aborted) return;
    DeviceConsole.ui.dialog.close();
    setNotice(DeviceConsole.utils.requests.message(error), true);
  } finally {
    if (!signal.aborted && confirmButton.isConnected) confirmButton.disabled = false;
  }
}

function bindSystem() {
  $('systemForm').addEventListener('submit', saveSystem);
  $('systemForm').addEventListener('input', updateSystemSaveAvailability);
  $('resetButton').addEventListener('click', openResetDialog);
  $('confirmResetButton').addEventListener('click', resetSystem);
  $('cancelResetButton').addEventListener('click', DeviceConsole.ui.dialog.close);
}
