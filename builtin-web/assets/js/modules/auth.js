const ADMIN_PASSWORD_PATTERN = /^[A-Za-z0-9!@#$%^&*()_+=.,:?-]{8,63}$/;

function createLoginDialog() {
  const { el, icon, translated } = DeviceConsole.utils.dom;
  const form = el('form', {
    id: 'loginForm',
    attrs: { autocomplete: 'off' },
    children: [
      el('label', {
        children: [
          translated('span', 'username', 'Username'),
          el('input', {
            id: 'loginUsername',
            attrs: { autocomplete: 'off', required: true },
            props: { readOnly: true }
          })
        ]
      }),
      el('label', {
        children: [
          translated('span', 'password', 'Password'),
          el('input', {
            id: 'loginPassword',
            attrs: { type: 'password', autocomplete: 'off', required: true }
          })
        ]
      }),
      el('footer', {
        children: [
          translated('button', 'signIn', 'Sign in', {
            className: 'button primary',
            attrs: { type: 'submit' }
          }),
          translated('button', 'cancel', 'Cancel', {
            id: 'cancelLoginButton',
            className: 'button',
            attrs: { type: 'button' }
          })
        ]
      })
    ]
  });
  return el('section', {
    id: 'loginDialog',
    className: 'dialog login-dialog',
    attrs: {
      role: 'dialog',
      'aria-modal': 'true',
      'aria-labelledby': 'loginTitle'
    },
    dataset: { initialFocus: 'loginPassword' },
    props: { hidden: true },
    children: [
      el('span', { className: 'feature-icon large', children: [icon('user')] }),
      translated('h2', 'unlockSettings', 'Unlock settings', { id: 'loginTitle' }),
      translated('p', 'loginBody', 'Sign in to change Wi-Fi and device settings.'),
      translated(
        'div',
        'loginNoticePrompt',
        'Enter the administrator password to unlock these settings.',
        {
          id: 'loginNotice',
          className: 'inline-notice',
          attrs: { role: 'status', 'aria-live': 'polite' }
        }
      ),
      form
    ]
  });
}

async function verifySession() {
  if (!state.token) return false;
  try {
    await resources.auth.session();
    return true;
  } catch (_) {
    setToken('');
    return false;
  }
}

function loginErrorMessage(error) {
  if (error && (error.code === 'unauthorized' || error.status === 401)) {
    return t('invalidAdminPassword');
  }
  return error && error.message ? error.message : t('signInFailed');
}

async function openLoginDialog() {
  $('loginPassword').value = '';
  setNotice(t('loginNoticePrompt'), false, $('loginNotice'));
  if (!state.auth || !state.auth.username) {
    try {
      state.auth = await resources.auth.get();
    } catch (error) {
      setNotice(error.message, true, $('loginNotice'));
    }
  }
  $('loginUsername').value = state.auth && state.auth.username ? state.auth.username : '';
  syncAdminUsername();
  DeviceConsole.ui.dialog.open('loginDialog');
}

async function login(event) {
  event.preventDefault();
  setNotice(t('signingIn'), false, $('loginNotice'));
  try {
    const data = await resources.auth.login({ username: $('loginUsername').value.trim(), password: $('loginPassword').value });
    setToken(data.token);
    DeviceConsole.ui.dialog.close();
    DeviceConsole.app.router.acceptPending();
  } catch (error) {
    setNotice(loginErrorMessage(error), true, $('loginNotice'));
  }
}

async function changePassword(event) {
  event.preventDefault();
  const password = $('newAdminPassword').value;
  const apProtectionEnabled = Boolean(
    state.wifi
    && state.wifi.interfaces
    && state.wifi.interfaces.ap
    && state.wifi.interfaces.ap.password_enabled
  );
  if (!ADMIN_PASSWORD_PATTERN.test(password)) {
    setNotice(t('adminPasswordRulesError'), true);
    $('newAdminPassword').focus();
    return;
  }
  if (password !== $('confirmAdminPassword').value) {
    setNotice(t('passwordsDoNotMatch'), true);
    $('confirmAdminPassword').focus();
    return;
  }
  try {
    await resources.auth.changePassword(password);
    setToken('');
    $('passwordForm').reset();
    DeviceConsole.app.router.navigate('network');
    setTransientNotice(t(apProtectionEnabled ? 'passwordChangedRestarting' : 'passwordChanged'), 4000);
  } catch (error) {
    setNotice(error.message, true);
  }
}

async function logout() {
  try {
    if (state.token) await resources.auth.logout();
  } catch (_) {
    // Local cleanup is still required if the token was already invalid.
  } finally {
    setToken('');
    DeviceConsole.app.router.navigate('network');
  }
}

function togglePasswordVisibility(button) {
  const input = $(button.dataset.togglePassword);
  const visible = input.type === 'text';
  input.type = visible ? 'password' : 'text';
  button.querySelector('use').setAttribute('href', visible ? '#i-eye' : '#i-eye-off');
  button.dataset.i18nTitle = visible ? 'showPassword' : 'hidePassword';
  button.dataset.i18nAriaLabel = button.dataset.i18nTitle;
  button.title = t(button.dataset.i18nTitle);
  button.setAttribute('aria-label', button.title);
  button.classList.toggle('active', !visible);
}

function syncAdminUsername() {
  const input = $('adminUsername');
  if (input) input.value = state.auth && state.auth.username ? state.auth.username : '';
}

function bindLogin() {
  $('loginForm').addEventListener('submit', login);
  $('cancelLoginButton').addEventListener('click', DeviceConsole.app.router.cancelPending);
}

function bindAdmin() {
  $('passwordForm').addEventListener('submit', changePassword);
  $$('[data-toggle-password]').forEach((button) => button.addEventListener('click', () => togglePasswordVisibility(button)));
}

function bindAuth() {
  DeviceConsole.ui.dialog.add(createLoginDialog());
  bindLogin();
  if ($('passwordForm')) bindAdmin();
}
