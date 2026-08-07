(function defineNetworkPage(app) {
  const { detailRow, el, icon, translated } = app.utils.dom;

  function runtimeModeLabel(mode) {
    return ({ ap: 'AP', sta: 'STA', ap_sta: 'AP + STA', off: 'Off' })[mode]
      || mode
      || '—';
  }

  function interfaceData(name) {
    const interfaces = state.wifi && state.wifi.interfaces;
    return interfaces && interfaces[name] ? interfaces[name] : {};
  }

  function interfaceState(value, fallback = 'inactive') {
    const knownStates = ['connecting', 'connected', 'starting', 'active', 'failed', 'disabled'];
    return t(knownStates.includes(value) ? value : fallback);
  }

  function setRuntimeBadge(id, runtimeState) {
    const badge = $(id);
    badge.textContent = interfaceState(runtimeState);
    badge.classList.toggle('status-warning', ['connecting', 'starting'].includes(runtimeState));
    badge.classList.toggle('status-error', runtimeState === 'failed');
    badge.classList.toggle('status-inactive', runtimeState === 'disabled');
  }

  function endpointColumn(name, primaryId, ipId, className = '') {
    return el('div', {
      className: `endpoint-column${className ? ` ${className}` : ''}`,
      children: [
        el('dt', { text: name }),
        el('dd', { id: primaryId, className: 'endpoint-primary', text: '—' }),
        el('dd', {
          className: 'endpoint-secondary',
          children: [el('span', { text: 'IP' }), el('span', { id: ipId, text: '—' })]
        })
      ]
    });
  }

  function interfaceCard(kind, iconId) {
    const prefix = kind === 'STA' ? 'networkSta' : 'networkAp';
    const rows = kind === 'STA'
      ? [
        detailRow('ipAddress', 'IP address', `${prefix}DetailIp`),
        detailRow('security', 'Security', `${prefix}Security`),
        detailRow('stateLabel', 'State', `${prefix}State`)
      ]
      : [
        detailRow('ipAddress', 'IP address', `${prefix}DetailIp`),
        detailRow('password', 'Password', `${prefix}Password`),
        detailRow('stateLabel', 'State', `${prefix}State`)
      ];
    return el('section', {
      className: 'interface-card',
      children: [
        el('header', {
          children: [
            el('span', { className: 'feature-icon', children: [icon(iconId)] }),
            el('span', {
              children: [
                el('small', { id: `${prefix}Label`, text: kind }),
                el('strong', { id: `${prefix}Ssid`, text: '—' })
              ]
            }),
            el('em', { id: `${prefix}Badge`, text: '—' })
          ]
        }),
        el('dl', { className: 'detail-list', children: rows })
      ]
    });
  }

  function createNetworkPage() {
    const modeStatus = el('span', {
      className: 'mode-status',
      children: [
        icon('wifi'),
        translated('span', 'wifiMode', 'Wi-Fi mode'),
        el('strong', { id: 'networkMode', text: '—' })
      ]
    });
    const addressPanel = el('article', {
      className: 'panel endpoint-panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [translated('h2', 'networkAddresses', 'Network addresses'), modeStatus]
        }),
        el('dl', {
          className: 'endpoint-columns',
          children: [
            endpointColumn('STA', 'networkStaName', 'networkStaIp'),
            endpointColumn('AP', 'networkApName', 'networkApIp'),
            el('div', {
              className: 'endpoint-column endpoint-mdns',
              children: [
                el('dt', { text: 'mDNS URL' }),
                el('dd', { children: [el('a', { id: 'networkMdns', attrs: { href: '#' }, text: '—' })] }),
                el('dd', { id: 'networkMdnsState', className: 'endpoint-helper', text: '—' })
              ]
            })
          ]
        })
      ]
    });
    const interfacePanel = el('article', {
      className: 'panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [
            translated('h2', 'interfaceStatus', 'Interface status'),
            el('span', { id: 'activeInterfaceCount', text: '—' })
          ]
        }),
        el('div', {
          className: 'interface-grid',
          children: [interfaceCard('STA', 'wifi'), interfaceCard('AP', 'signal')]
        })
      ]
    });
    return el('section', {
      id: 'page-network',
      className: 'page active',
      dataset: { pageView: 'network' },
      children: [addressPanel, interfacePanel]
    });
  }

  function render() {
    if (!$('page-network') || !state.device || !state.wifi) return;
    const wifi = state.wifi;
    const sta = interfaceData('sta');
    const ap = interfaceData('ap');
    const hostname = state.device.hostname || 'esp32-device';
    const mdns = `http://${hostname}.local/`;
    const activeCount = Number(sta.state === 'connected') + Number(ap.state === 'active');
    const mdnsAvailable = sta.state === 'connected';

    setText('networkMode', runtimeModeLabel(wifi.mode));
    setText('networkStaName', sta.ssid || t('unavailable'));
    setText('networkStaIp', sta.ip);
    setText('networkApName', ap.ssid || t('unavailable'));
    setText('networkApIp', ap.ip);
    $('networkMdns').textContent = mdns;
    if (mdnsAvailable) $('networkMdns').href = mdns;
    else $('networkMdns').removeAttribute('href');
    $('networkMdns').setAttribute('aria-disabled', String(!mdnsAvailable));
    let mdnsStateKey = 'mdnsNeedsSta';
    if (mdnsAvailable) mdnsStateKey = 'mdnsStaLan';
    else if (sta.state === 'connecting') mdnsStateKey = 'mdnsConnecting';
    else if (sta.state === 'failed') mdnsStateKey = 'mdnsStaFailed';
    setText('networkMdnsState', t(mdnsStateKey));
    setText('activeInterfaceCount', tf('activeInterfaces', { count: activeCount }));
    setText('networkStaLabel', 'STA');
    setText('networkStaSsid', sta.ssid || t('unavailable'));
    setRuntimeBadge('networkStaBadge', sta.state);
    setText('networkStaDetailIp', sta.ip);
    setText('networkStaSecurity', String(sta.security || '—').toUpperCase());
    setText('networkStaState', interfaceState(sta.state));
    setText('networkApLabel', 'AP');
    setText('networkApSsid', ap.ssid || t('unavailable'));
    setRuntimeBadge('networkApBadge', ap.state);
    setText('networkApDetailIp', ap.ip);
    setText('networkApPassword', t(ap.password_enabled ? 'enabled' : 'disabled'));
    setText('networkApState', interfaceState(ap.state));
  }

  const page = {
    id: 'network',
    titleKey: 'network',
    descriptionKey: 'networkOverview',
    resources: ['device', 'wifi'],

    mount(host) {
      host.replaceChildren(createNetworkPage());
      applyLanguage();
      render();
    },

    render,
    unmount() {}
  };

  app.pages.network = page;
})(window.DeviceConsole);
