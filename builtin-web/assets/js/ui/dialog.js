(function defineDialogLifecycle(app) {
  const { el } = app.utils.dom;
  let host = null;
  let activeDialog = null;
  let returnFocus = null;
  let inertBackground = [];

  function focusableElements(dialog) {
    return $$(
      'a[href], button:not([disabled]), input:not([disabled]), '
        + 'select:not([disabled]), textarea:not([disabled]), '
        + '[tabindex]:not([tabindex="-1"])',
      dialog
    ).filter((element) => !element.closest('[hidden]'));
  }

  function isolateBackground() {
    inertBackground = Array.from(document.body.children)
      .filter((element) => element !== host && element.id !== 'dialogBackdrop')
      .map((element) => ({
        element,
        wasInert: element.hasAttribute('inert')
      }));
    inertBackground.forEach(({ element }) => element.setAttribute('inert', ''));
  }

  function restoreBackground() {
    inertBackground.forEach(({ element, wasInert }) => {
      if (!wasInert) element.removeAttribute('inert');
    });
    inertBackground = [];
  }

  function hideDialogs(restoreFocus) {
    $$('.dialog', host).forEach((dialog) => { dialog.hidden = true; });
    $('dialogBackdrop').hidden = true;
    document.body.style.overflow = '';
    restoreBackground();
    activeDialog = null;
    if (restoreFocus && returnFocus && returnFocus.isConnected) {
      returnFocus.focus();
    }
    if (restoreFocus) returnFocus = null;
  }

  function close() {
    hideDialogs(true);
  }

  function trapFocus(event) {
    if (!activeDialog || event.key !== 'Tab') return;
    const focusable = focusableElements(activeDialog);
    if (!focusable.length) {
      event.preventDefault();
      activeDialog.focus();
      return;
    }
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!event.shiftKey
      && (document.activeElement === last || !activeDialog.contains(document.activeElement))) {
      event.preventDefault();
      first.focus();
    }
  }

  app.ui.dialog = {
    mount() {
      host = el('div', { id: 'dialogHost' });
      document.body.append(
        el('div', { id: 'dialogBackdrop', className: 'dialog-backdrop', props: { hidden: true } }),
        host
      );
      document.addEventListener('keydown', trapFocus);
    },

    add(dialog) {
      host.append(dialog);
      return dialog;
    },

    open(id) {
      const trigger = document.activeElement;
      hideDialogs(false);
      const dialog = $(id);
      if (!dialog) return;
      if (!trigger || !trigger.closest('.dialog')) returnFocus = trigger;
      $('dialogBackdrop').hidden = false;
      dialog.hidden = false;
      activeDialog = dialog;
      isolateBackground();
      document.body.style.overflow = 'hidden';
      const target = dialog.dataset.initialFocus
        ? $(dialog.dataset.initialFocus)
        : dialog.querySelector('input, button');
      if (target) {
        window.setTimeout(() => {
          if (activeDialog === dialog) target.focus();
        }, 0);
      }
    },

    close
  };
})(window.DeviceConsole);
