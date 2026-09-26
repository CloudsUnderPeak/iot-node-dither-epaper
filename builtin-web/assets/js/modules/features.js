function featureDisabled(name) {
  return Boolean(state.features && state.features[name] === false);
}

let featureRequest = null;
async function refreshFeatures() {
  if (featureRequest) return featureRequest;
  featureRequest = resources.features.get().then((data) => {
    const names = ['sleep', 'epaper', 'storage', 'auth', 'user_files', 'mdns', 'battery', 'console'];
    if (!data.features || !names.every((name) => typeof data.features[name] === 'boolean')) {
      throw new Error('Invalid feature response');
    }
    state.features = data.features;
    if (featureDisabled('auth')) setToken('');
    DeviceConsole.app.router.renderCurrent();
    DeviceConsole.app.shell.setActivePage(state.activePage);
    return state.features;
  }).finally(() => { featureRequest = null; });
  return featureRequest;
}
