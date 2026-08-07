(function bootstrap(app) {
  function syncResourceViews(names, formSync = 'if-clean') {
    const updated = new Set(names);
    app.app.router.renderCurrent();

    if (updated.has('wifi') && $('wifiForm')) {
      const shouldFill = formSync === 'force'
        || (formSync === 'if-clean' && !wifiFormHasChanges());
      if (shouldFill) fillWifiForm(state.wifi);
      else syncWifiFormState();
    }

    if (updated.has('device') && $('systemForm')) {
      const shouldFill = formSync === 'force'
        || (formSync === 'if-clean' && !systemFormHasChanges());
      if (shouldFill) fillSystemForm(state.device);
      else updateSystemSaveAvailability();
    }

    if (updated.has('auth')) {
      const username = state.auth && state.auth.username ? state.auth.username : '';
      if ($('loginUsername')) $('loginUsername').value = username;
      syncAdminUsername();
    }
  }

  async function refreshResources(names, options = {}) {
    const showFeedback = options.showFeedback !== false;
    const formSync = options.formSync || 'if-clean';
    if (showFeedback) setNotice(t('loading'));

    const results = await Promise.allSettled(names.map(loadResourceSnapshot));
    const committedNames = results
      .filter((result) => result.status === 'fulfilled' && result.value.committed)
      .map((result) => result.value.name);
    syncResourceViews(committedNames, formSync);

    const failure = results.find((result) => result.status === 'rejected');
    if (failure) {
      if (showFeedback) setNotice(failure.reason.message, true);
      throw failure.reason;
    }
    if (showFeedback) setTransientNotice(t('ready'));
    return results.map((result) => result.value.data);
  }

  function resourcesForCurrentPage() {
    const page = app.app.router.currentPage();
    if (!page) return [];
    if (page.id !== 'settings') return page.resources || [];
    if (state.settingsPage === 'admin') return ['auth'];
    if (state.settingsPage === 'system') return ['device'];
    return ['wifi'];
  }

  function refreshCurrentPage() {
    // refreshResources already presents a user-facing error for this explicit
    // action, so the terminal rejection is intentionally consumed here.
    return refreshResources(resourcesForCurrentPage()).catch(() => {});
  }

  async function refreshWifiSnapshot(options = {}) {
    const result = await loadResourceSnapshot('wifi');
    if (!result.committed) throw new Error('Stale Wi-Fi snapshot');
    syncResourceViews(['wifi'], options.formSync || 'none');
    return result.data;
  }

  function start() {
    app.ui.icons.mount();
    app.app.shell.mount();
    app.ui.dialog.mount();
    bindAuth();
    mountWifiDialogs();
    mountSystemDialogs();
    app.app.router.bind();
    $('refreshButton').addEventListener('click', refreshCurrentPage);
    applyLanguage();
    app.app.router.start();
    refreshResources(['device', 'wifi', 'storage', 'auth'], {
      formSync: 'force'
    }).catch((error) => reportBackgroundError('initial resource refresh', error));
  }

  window.refreshResources = refreshResources;
  window.refreshWifiSnapshot = refreshWifiSnapshot;
  start();
})(window.DeviceConsole);
