(function defineSettingsPage(app) {
  const { el, icon, translated } = app.utils.dom;
  const ADMIN_PATTERN = '[A-Za-z0-9\\u0021\\u0023-\\u0026\\u0028-\\u002E'
    + '\\u003A\\u003D\\u003F-\\u0040\\u005E-\\u005F]{8,63}';
  const IPV4_PATTERN = '(?:25[0-5]|2[0-4][0-9]|1?[0-9]{1,2})'
    + '(?:\\.(?:25[0-5]|2[0-4][0-9]|1?[0-9]{1,2})){3}';

  function showSettingsPage(pageId) {
    state.settingsPage = pageId;
    $$('[data-settings-view]').forEach((view) => {
      view.hidden = view.dataset.settingsView !== pageId;
    });
    $$('[data-settings-page]').forEach((button) => {
      button.classList.toggle('active', button.dataset.settingsPage === pageId);
    });
  }

  function bindSettingsNavigation() {
    $$('[data-settings-page]').forEach((button) => {
      button.addEventListener('click', () => {
        app.app.router.navigate('settings', button.dataset.settingsPage);
      });
    });
  }

  function field(label, control) {
    const labelNode = typeof label === 'string' ? el('span', { text: label }) : label;
    return el('label', { children: [labelNode, control] });
  }

  function passwordToggle(inputId) {
    return el('button', {
      attrs: {
        type: 'button',
        title: 'Show password',
        'data-i18n-title': 'showPassword',
        'data-i18n-aria-label': 'showPassword',
        'aria-label': 'Show password'
      },
      dataset: { togglePassword: inputId },
      children: [icon('eye')]
    });
  }

  function passwordField(inputId, attrs) {
    return el('span', {
      className: 'password-field',
      children: [
        el('input', { id: inputId, attrs: Object.assign({ type: 'password' }, attrs) }),
        passwordToggle(inputId)
      ]
    });
  }

  function selectField(selectId, options) {
    return el('span', {
      className: 'select-field',
      children: [
        el('select', {
          id: selectId,
          children: options.map((option) => option.key
            ? translated('option', option.key, option.label, { attrs: { value: option.value } })
            : el('option', { attrs: { value: option.value }, text: option.label }))
        }),
        icon('chevron-down')
      ]
    });
  }

  function ipv4Field(id, labelKey, label, placeholder, className) {
    return el('label', {
      className,
      children: [
        translated('span', labelKey, label),
        el('input', {
          id,
          attrs: {
            inputmode: 'decimal',
            maxlength: 15,
            pattern: IPV4_PATTERN,
            placeholder
          }
        })
      ]
    });
  }

  function modeOption(value, label, descriptionKey, description) {
    return el('label', {
      children: [
        el('input', { attrs: { type: 'radio', name: 'wifiMode', value } }),
        el('span', {
          children: [
            el('strong', { text: label }),
            translated('small', descriptionKey, description)
          ]
        })
      ]
    });
  }

  function createModeSettings() {
    return el('article', {
      className: 'panel mode-section',
      children: [
        el('header', {
          className: 'panel-head',
          children: [
            translated('h2', 'wifiMode', 'Wi-Fi mode'),
            el('span', { id: 'selectedModeLabel', text: '—' })
          ]
        }),
        el('div', {
          className: 'mode-picker',
          children: [
            modeOption('ap', 'AP', 'apFriendly', 'Connect directly to this device'),
            modeOption('sta', 'STA', 'staFriendly', 'Join your existing Wi-Fi'),
            modeOption('ap_sta', 'AP + STA', 'apstaFriendly', 'Use both connection methods')
          ]
        }),
        el('p', { id: 'modeDescription', text: '—' })
      ]
    });
  }

  function createAdvancedPanel(id, children) {
    return el('details', {
      id,
      className: 'advanced-panel embedded',
      children: [
        el('summary', {
          children: [
            translated('strong', 'advancedSettings', 'Advanced settings'),
            icon('chevron-right', 'accordion-chevron accordion-chevron-closed'),
            icon('chevron-down', 'accordion-chevron accordion-chevron-open')
          ]
        }),
        el('div', { className: 'advanced-fields', children })
      ]
    });
  }

  function createStaSettings() {
    const fallback = el('div', {
      id: 'fallbackField',
      className: 'form-row switch-row',
      children: [
        translated('span', 'fallbackAp', 'Fallback AP', { id: 'fallbackApLabel' }),
        el('span', {
          children: [
            translated(
              'small',
              'fallback',
              'Automatically enable AP when connection fails or disconnects',
              { id: 'fallbackApDescription' }
            ),
            el('input', {
              id: 'fallbackToAp',
              className: 'switch',
              attrs: {
                type: 'checkbox',
                'aria-labelledby': 'fallbackApLabel',
                'aria-describedby': 'fallbackApDescription'
              }
            })
          ]
        })
      ]
    });
    const advanced = createAdvancedPanel('staAdvancedSettings', [
      fallback,
      field(
        translated('span', 'staIpMode', 'STA IPv4'),
        selectField('staIpMode', [
          { value: 'dhcp', key: 'automaticDhcp', label: 'Automatic (DHCP)' },
          { value: 'static', key: 'staticIp', label: 'Static' }
        ])
      ),
      ipv4Field('staIpAddress', 'ipAddress', 'IP address', 'e.g. 192.168.1.50', 'staStaticIpField'),
      ipv4Field('staIpNetmask', 'netmask', 'Netmask', 'e.g. 255.255.255.0', 'staStaticIpField'),
      ipv4Field('staIpGateway', 'gateway', 'Gateway', 'e.g. 192.168.1.1', 'staStaticIpField'),
      ipv4Field('staDns1', 'primaryDns', 'Primary DNS', 'e.g. 192.168.1.1', 'staStaticIpField'),
      ipv4Field('staDns2', 'secondaryDns', 'Secondary DNS', 'e.g. 1.1.1.1', 'staStaticIpField')
    ]);
    advanced.querySelector('#staIpMode').closest('label').id = 'staIpModeField';
    return el('article', {
      id: 'staSettings',
      className: 'panel form-panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [
            translated('h2', 'joinExistingNetwork', 'Join an existing network'),
            translated('button', 'chooseNetwork', 'Choose network', {
              id: 'chooseNetworkButton',
              className: 'text-button',
              attrs: { type: 'button' }
            })
          ]
        }),
        field(
          'SSID',
          el('input', {
            id: 'staSsid',
            attrs: {
              maxlength: 32,
              pattern: '[\\u0020-\\u007E]{1,32}',
              autocomplete: 'off'
            }
          })
        ),
        field(
          translated('span', 'security', 'Security'),
          selectField('staSecurity', [
            { value: 'wpa', label: 'WPA / WPA2' },
            { value: 'open', label: 'Open' }
          ])
        ),
        field(
          translated('span', 'password', 'Password'),
          passwordField('staPassword', {
            minlength: 8,
            maxlength: 63,
            pattern: '[\\u0020-\\u007E]{8,63}',
            autocomplete: 'off',
            placeholder: '********'
          })
        ),
        advanced
      ]
    });
  }

  function createApSettings() {
    const protectAp = el('div', {
      className: 'form-row switch-row',
      children: [
        translated('span', 'protectAp', 'Protect AP', { id: 'protectApLabel' }),
        el('span', {
          children: [
            translated('small', 'useAdminPassword', 'Use admin password'),
            el('input', {
              id: 'apPasswordEnabled',
              className: 'switch',
              attrs: { type: 'checkbox', 'aria-labelledby': 'protectApLabel' }
            })
          ]
        })
      ]
    });
    const advanced = createAdvancedPanel('apAdvancedSettings', [
      field(
        translated('span', 'apIpMode', 'AP IPv4'),
        selectField('apIpMode', [
          { value: 'default', key: 'useDefaultSettings', label: 'Use default settings' },
          { value: 'static', key: 'setManually', label: 'Set manually' }
        ])
      ),
      ipv4Field('apIpAddress', 'apIpAddress', 'AP IP address', 'e.g. 192.168.4.1', 'apStaticIpField'),
      ipv4Field('apIpNetmask', 'apNetmask', 'AP netmask', 'e.g. 255.255.255.0', 'apStaticIpField')
    ]);
    advanced.querySelector('#apIpMode').closest('label').id = 'apIpModeField';
    return el('article', {
      id: 'apSettings',
      className: 'panel form-panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [translated('h2', 'deviceAccessPoint', 'Device access point')]
        }),
        field(
          'AP SSID',
          el('input', {
            id: 'apSsid',
            attrs: {
              maxlength: 32,
              pattern: '[\\u0020-\\u007E]{1,32}',
              autocomplete: 'off'
            }
          })
        ),
        protectAp,
        field(
          translated('span', 'apAddress', 'AP address'),
          el('span', {
            className: 'read-only',
            children: [el('strong', { id: 'settingsApIp', text: '—' })]
          })
        ),
        advanced
      ]
    });
  }

  function settingsTab(pageId, iconId, label, labelKey) {
    const text = labelKey ? translated('span', labelKey, label) : el('span', { text: label });
    return el('button', {
      className: pageId === 'wifi' ? 'active' : '',
      attrs: { type: 'button' },
      dataset: { settingsPage: pageId },
      children: [icon(iconId), text]
    });
  }

  function createWifiSettings() {
    const layout = el('div', {
      id: 'wifiSettingsLayout',
      className: 'conditional-layout',
      children: [createStaSettings(), createApSettings()]
    });
    const recovery = el('div', {
      id: 'wifiRecovery',
      className: 'wifi-recovery',
      attrs: { role: 'status', 'aria-live': 'polite' },
      props: { hidden: true },
      children: [
        el('span', { id: 'wifiRecoveryMessage', text: '—' }),
        el('a', { id: 'wifiRecoveryLink', attrs: { href: '#' }, text: '—' })
      ]
    });
    const saveDock = el('div', {
      className: 'save-dock',
      children: [
        el('div', {
          className: 'save-dock-inner',
          children: [
            translated('span', 'noChanges', 'No changes', { id: 'wifiSaveState' }),
            translated('button', 'saveChanges', 'Save changes', {
              id: 'wifiSaveButton',
              className: 'button primary',
              attrs: { type: 'submit' },
              props: { disabled: true }
            })
          ]
        })
      ]
    });
    return el('section', {
      id: 'settings-wifi',
      className: 'settings-page active',
      dataset: { settingsView: 'wifi' },
      children: [
        el('form', {
          id: 'wifiForm',
          attrs: { autocomplete: 'off' },
          children: [createModeSettings(), layout, recovery, saveDock]
        })
      ]
    });
  }

  function adminPasswordField(inputId) {
    return passwordField(inputId, {
      minlength: 8,
      maxlength: 63,
      pattern: ADMIN_PATTERN,
      autocomplete: 'off',
      required: true
    });
  }

  function createAdminSettings() {
    const form = el('form', {
      id: 'passwordForm',
      className: 'panel password-form',
      attrs: { autocomplete: 'off', novalidate: true },
      children: [
        el('header', {
          className: 'panel-head',
          children: [translated('h2', 'changePassword', 'Change password')]
        }),
        field(
          translated('span', 'username', 'Username'),
          el('input', {
            id: 'adminUsername',
            attrs: { autocomplete: 'off' },
            props: { disabled: true }
          })
        ),
        field(translated('span', 'newPassword', 'New password'), adminPasswordField('newAdminPassword')),
        el('div', {
          className: 'password-hint',
          children: [
            translated('strong', 'allowedCharacters', 'Allowed characters'),
            translated(
              'span',
              'passwordRules',
              '8–63 characters. Use letters, numbers, or: ! @ # $ % ^ & * ( ) - _ = + . , : ?'
            )
          ]
        }),
        field(
          translated('span', 'confirmPassword', 'Confirm password'),
          adminPasswordField('confirmAdminPassword')
        ),
        translated('button', 'updatePassword', 'Update password', {
          className: 'button primary form-submit',
          attrs: { type: 'submit' }
        })
      ]
    });
    return el('section', {
      id: 'settings-admin',
      className: 'settings-page',
      dataset: { settingsView: 'admin' },
      props: { hidden: true },
      children: [el('div', { className: 'admin-layout single', children: [form] })]
    });
  }

  function createSystemSettings() {
    const form = el('form', {
      id: 'systemForm',
      className: 'panel form-panel hostname-panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [translated('h2', 'systemSettings', 'System settings')]
        }),
        field(
          translated('span', 'deviceName', 'Device name'),
          el('input', {
            id: 'hostname',
            attrs: {
              maxlength: 31,
              pattern: '[A-Za-z0-9](?:[A-Za-z0-9\\-]{0,29}[A-Za-z0-9])?',
              autocomplete: 'off',
              required: true
            }
          })
        ),
        el('div', {
          className: 'panel-actions',
          children: [
            translated('span', 'noChanges', 'No changes', {
              id: 'systemSaveState',
              className: 'visually-hidden',
              attrs: { role: 'status', 'aria-live': 'polite' }
            }),
            translated('button', 'saveChanges', 'Save changes', {
              id: 'systemSaveButton',
              className: 'button primary',
              attrs: { type: 'submit' },
              props: { disabled: true }
            })
          ]
        })
      ]
    });
    const dangerZone = el('article', {
      className: 'danger-zone',
      children: [
        el('span', { className: 'danger-icon', children: [icon('warning')] }),
        el('div', {
          children: [
            translated('h2', 'factoryReset', 'Factory reset'),
            translated(
              'p',
              'factoryResetBody',
              'Clear Wi-Fi and admin settings, then restart the device in AP mode.'
            )
          ]
        }),
        translated('button', 'resetDevice', 'Reset device', {
          id: 'resetButton',
          className: 'button danger',
          attrs: { type: 'button' }
        })
      ]
    });
    return el('section', {
      id: 'settings-system',
      className: 'settings-page',
      dataset: { settingsView: 'system' },
      props: { hidden: true },
      children: [el('div', { className: 'system-layout', children: [form, dangerZone] })]
    });
  }

  function createSettingsPage() {
    const tabs = el('nav', {
      className: 'settings-tabs',
      attrs: { 'aria-label': 'Settings navigation' },
      children: [
        settingsTab('wifi', 'wifi', 'Wi-Fi'),
        settingsTab('admin', 'user', 'Admin', 'admin'),
        settingsTab('system', 'settings', 'System', 'system')
      ]
    });
    return el('section', {
      id: 'page-settings',
      className: 'page active',
      dataset: { pageView: 'settings' },
      children: [tabs, createWifiSettings(), createAdminSettings(), createSystemSettings()]
    });
  }

  const page = {
    id: 'settings',
    titleKey: 'settings',
    descriptionKey: 'deviceSettings',

    mount(host, context) {
      host.replaceChildren(createSettingsPage());
      bindSettingsNavigation();
      bindWifi();
      bindSystem();
      bindAdmin();
      applyLanguage();
      if (state.wifi) fillWifiForm(state.wifi);
      if (state.device) fillSystemForm(state.device);
      syncAdminUsername();
      this.onRouteChange(context.route);
    },

    onRouteChange(route) {
      const pageId = String(route || '').split('/')[1] || 'wifi';
      showSettingsPage(['wifi', 'admin', 'system'].includes(pageId) ? pageId : 'wifi');
    },

    render() {
      if (state.wifi && !wifiFormHasChanges()) fillWifiForm(state.wifi);
      if (state.device && !systemFormHasChanges()) fillSystemForm(state.device);
      syncAdminUsername();
    },

    unmount() {
      window.clearInterval(scanCooldownTimer);
    }
  };

  app.pages.settings = page;
})(window.DeviceConsole);
