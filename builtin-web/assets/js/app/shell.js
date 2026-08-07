(function defineAppShell(app) {
  const { el, icon, translated } = app.utils.dom;

  function navigationButton(pageId, iconId, labelKey, fallback) {
    return el('button', {
      className: pageId === 'network' ? 'active' : '',
      attrs: { type: 'button' },
      dataset: { page: pageId },
      children: [icon(iconId), translated('span', labelKey, fallback)]
    });
  }

  function createBrand() {
    return el('a', {
      className: 'brand',
      attrs: { href: '#/network', 'aria-label': 'Network' },
      dataset: { page: 'network' },
      children: [
        el('span', { className: 'brand-mark', children: [icon('globe')] }),
        el('span', {
          children: [
            translated('small', 'deviceName', 'Device name'),
            el('strong', { id: 'sidebarHostname', text: 'ESP32 Device' }),
            el('span', {
              className: 'brand-presence',
              children: [
                el('i', { className: 'online-dot' }),
                translated('span', 'deviceOnline', 'Device online')
              ]
            })
          ]
        })
      ]
    });
  }

  function languageOption(language, label) {
    return el('button', {
      attrs: { type: 'button' },
      dataset: { language },
      children: [
        el('span', { text: label }),
        el('b', { className: 'language-check', attrs: { 'aria-hidden': 'true' }, text: '✓' })
      ]
    });
  }

  function createPublicMenu() {
    const languageSubmenu = el('div', {
      id: 'languageSubmenu',
      className: 'language-submenu',
      props: { hidden: true },
      children: [languageOption('en', 'English'), languageOption('zh-Hant', '繁體中文')]
    });
    const languageItem = el('div', {
      id: 'languageMenuItem',
      className: 'submenu-item',
      children: [
        el('button', {
          id: 'languageMenuButton',
          attrs: { type: 'button', 'aria-expanded': 'false' },
          children: [translated('span', 'language', 'Language')]
        }),
        languageSubmenu
      ]
    });
    const dropdown = el('div', {
      id: 'menuDropdown',
      className: 'menu-dropdown',
      props: { hidden: true },
      children: [
        languageItem,
        el('hr', { id: 'logoutSeparator', props: { hidden: true } }),
        el('button', {
          id: 'logoutButton',
          attrs: { type: 'button' },
          props: { hidden: true },
          children: [translated('span', 'logout', 'Log out')]
        })
      ]
    });
    return el('div', {
      className: 'public-menu',
      children: [
        el('button', {
          id: 'menuButton',
          className: 'button icon-button',
          attrs: {
            type: 'button',
            title: 'Menu',
            'data-i18n-title': 'menu',
            'aria-expanded': 'false',
            'aria-controls': 'menuDropdown'
          },
          children: [icon('menu'), translated('span', 'menu', 'Menu')]
        }),
        dropdown
      ]
    });
  }

  function createTopbar() {
    const heading = el('div', {
      className: 'page-heading',
      children: [
        el('h1', { id: 'pageTitle', text: 'Network' }),
        el('p', { id: 'pageDescription', text: 'Connection status and addresses' })
      ]
    });
    const refresh = el('button', {
      id: 'refreshButton',
      className: 'button icon-button',
      attrs: { type: 'button', title: 'Refresh', 'data-i18n-title': 'refresh' },
      children: [icon('refresh'), translated('span', 'refresh', 'Refresh')]
    });
    return el('header', {
      className: 'topbar',
      children: [heading, el('div', { className: 'top-actions', children: [refresh, createPublicMenu()] })]
    });
  }

  function createPrimaryNavigation() {
    return el('nav', {
      className: 'primary-nav',
      attrs: { 'aria-label': 'Main navigation' },
      children: [
        navigationButton('network', 'wifi', 'network', 'Network'),
        navigationButton('hardware', 'cpu-chip', 'hardware', 'Hardware'),
        navigationButton('settings', 'settings', 'settings', 'Settings')
      ]
    });
  }

  function createMobileNavigation() {
    return el('nav', {
      className: 'mobile-nav',
      attrs: { 'aria-label': 'Mobile navigation' },
      children: [
        navigationButton('network', 'wifi', 'network', 'Network'),
        navigationButton('hardware', 'cpu-chip', 'hardware', 'Hardware'),
        navigationButton('settings', 'settings', 'settings', 'Settings')
      ]
    });
  }

  function createShell() {
    const sidebar = el('aside', {
      className: 'sidebar',
      children: [createBrand(), createPrimaryNavigation()]
    });
    const main = el('div', {
      className: 'app-main',
      children: [
        createTopbar(),
        el('div', {
          id: 'notice',
          className: 'notice',
          attrs: { role: 'status', 'aria-live': 'polite' },
          props: { hidden: true }
        }),
        el('main', { id: 'pageHost', className: 'page-area' })
      ]
    });
    return el('div', {
      id: 'appView',
      className: 'app-shell',
      children: [sidebar, main, createMobileNavigation()]
    });
  }

  app.app.shell = {
    mount() {
      const root = $('app');
      root.replaceChildren(createShell());
      root.setAttribute('aria-busy', 'false');
    },

    setHeading(page) {
      setText('pageTitle', t(page.titleKey));
      setText('pageDescription', t(page.descriptionKey));
    },

    setActivePage(pageId) {
      $$('[data-page]').forEach((button) => {
        button.classList.toggle('active', button.dataset.page === pageId);
      });
      const hideLogout = pageId !== 'settings' || !state.token;
      $('logoutButton').hidden = hideLogout;
      $('logoutSeparator').hidden = hideLogout;
    },

    renderDevice() {
      const hostname = state.device && state.device.hostname
        ? state.device.hostname
        : 'ESP32 Device';
      setText('sidebarHostname', hostname);
      const activePage = app.app.router && app.app.router.currentPage();
      document.title = `${hostname} · ${t(activePage ? activePage.id : 'network')}`;
    }
  };
})(window.DeviceConsole);
