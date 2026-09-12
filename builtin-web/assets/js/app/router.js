(function defineRouter(app) {
  let mountedPage = null;
  let routeVersion = 0;

  function routeFromHash() {
    const parts = window.location.hash.replace(/^#\/?/, '').split('/');
    if (parts[0] === 'settings') {
      const child = ['wifi', 'admin', 'system'].includes(parts[1]) ? parts[1] : 'wifi';
      return { page: 'settings', child, route: `settings/${child}` };
    }
    if (parts[0] === 'hardware') return { page: 'hardware', route: 'hardware' };
    return { page: 'network', route: 'network' };
  }

  function routeHash(pageId, child = state.settingsPage) {
    return pageId === 'settings' ? `#/settings/${child || 'wifi'}` : `#/${pageId}`;
  }

  function mountRoute(route) {
    const page = app.pages[route.page];
    if (!page) throw new Error(`Unknown page: ${route.page}`);

    if (mountedPage && mountedPage.id === page.id) {
      if (page.onRouteChange) page.onRouteChange(route.route);
    } else {
      if (mountedPage && mountedPage.unmount) mountedPage.unmount();
      const host = $('pageHost');
      host.replaceChildren();
      mountedPage = page;
      page.mount(host, { route: route.route });
    }

    state.activePage = page.id;
    app.app.shell.setHeading(page);
    app.app.shell.setActivePage(page.id);
    app.app.shell.renderDevice();
    closePublicMenu();
    window.scrollTo(0, 0);
  }

  async function handleRoute(sessionKnown = false) {
    app.utils.requests.rotate();
    wifiStatusPollGeneration += 1;
    wifiFormBusy = false;
    systemFormBusy = false;
    const version = routeVersion + 1;
    routeVersion = version;
    const route = routeFromHash();
    if (route.page === 'settings' && !sessionKnown && !(await verifySession())) {
      if (version !== routeVersion) return;
      state.pendingHash = window.location.hash || '#/settings/wifi';
      await openLoginDialog();
      if (version !== routeVersion) app.ui.dialog.close();
      return;
    }
    if (version !== routeVersion) return;
    if (route.page !== 'settings') app.ui.dialog.close();
    mountRoute(route);
    state.lastAcceptedHash = window.location.hash || '#/network';
    state.pendingHash = '';
  }

  function requestRoute(pageId, child) {
    const hash = routeHash(pageId, child);
    if (window.location.hash === hash) handleRoute();
    else window.location.hash = hash;
  }

  function acceptPendingRoute() {
    handleRoute(true);
  }

  function cancelPendingRoute() {
    app.ui.dialog.close();
    if (!state.pendingHash) return;
    history.replaceState(null, '', state.lastAcceptedHash || '#/network');
    state.pendingHash = '';
    handleRoute(true);
  }

  function togglePublicMenu() {
    const willOpen = $('menuDropdown').hidden;
    $('menuDropdown').hidden = !willOpen;
    $('menuButton').setAttribute('aria-expanded', String(willOpen));
    if (!willOpen) closeLanguageSubmenu();
  }

  function openLanguageSubmenu() {
    $('languageSubmenu').hidden = false;
    $('languageMenuButton').setAttribute('aria-expanded', 'true');
  }

  function closeLanguageSubmenu() {
    $('languageSubmenu').hidden = true;
    $('languageMenuButton').setAttribute('aria-expanded', 'false');
  }

  function closePublicMenu() {
    $('menuDropdown').hidden = true;
    $('menuButton').setAttribute('aria-expanded', 'false');
    closeLanguageSubmenu();
  }

  function bindNavigation() {
    $$('[data-page]').forEach((button) => {
      button.addEventListener('click', (event) => {
        event.preventDefault();
        const child = button.dataset.page === 'settings' ? state.settingsPage : undefined;
        requestRoute(button.dataset.page, child);
      });
    });
    window.addEventListener('hashchange', () => handleRoute());
    $('menuButton').addEventListener('click', (event) => {
      event.stopPropagation();
      togglePublicMenu();
    });
    $('menuDropdown').addEventListener('click', (event) => event.stopPropagation());
    $('languageMenuItem').addEventListener('mouseenter', openLanguageSubmenu);
    $('languageMenuItem').addEventListener('mouseleave', closeLanguageSubmenu);
    $('languageMenuButton').addEventListener('click', () => {
      if ($('languageSubmenu').hidden) openLanguageSubmenu();
      else closeLanguageSubmenu();
    });
    $$('[data-language]').forEach((button) => {
      button.addEventListener('click', () => {
        setLanguage(button.dataset.language);
        applyLanguage();
        closePublicMenu();
      });
    });
    $('logoutButton').addEventListener('click', logout);
    document.addEventListener('click', closePublicMenu);
    document.addEventListener('keydown', (event) => {
      if (event.key !== 'Escape') return;
      closePublicMenu();
      if (!$('loginDialog').hidden) cancelPendingRoute();
      else app.ui.dialog.close();
    });
    $('dialogBackdrop').addEventListener('click', () => {
      if ($('loginDialog').hidden) app.ui.dialog.close();
      else cancelPendingRoute();
    });
  }

  function startRouting() {
    const initialHash = window.location.hash;
    if (!initialHash) history.replaceState(null, '', '#/network');
    if (routeFromHash().page === 'settings') mountRoute({ page: 'network', route: 'network' });
    handleRoute();
  }

  app.app.router = {
    acceptPending: acceptPendingRoute,
    bind: bindNavigation,
    cancelPending: cancelPendingRoute,
    currentPage: () => mountedPage,
    handle: handleRoute,
    navigate: requestRoute,
    refreshUi() {
      if (mountedPage) app.app.shell.setHeading(mountedPage);
      this.renderCurrent();
    },
    renderCurrent() {
      if (mountedPage && mountedPage.render) mountedPage.render();
      app.app.shell.renderDevice();
    },
    start: startRouting
  };

})(window.DeviceConsole);
