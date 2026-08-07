const resourceReaders = {
  device: resources.device.get,
  wifi: resources.wifi.get,
  storage: resources.storage.get,
  auth: resources.auth.get
};

const resourceRequestVersions = {
  device: 0,
  wifi: 0,
  storage: 0,
  auth: 0
};

function invalidateResourceRequests(name) {
  resourceRequestVersions[name] += 1;
}

function mergeResourceSnapshot(name, data) {
  invalidateResourceRequests(name);
  state[name] = Object.assign({}, state[name] || {}, data);
}

async function loadResourceSnapshot(name) {
  const version = resourceRequestVersions[name] + 1;
  resourceRequestVersions[name] = version;
  const data = await resourceReaders[name]();
  if (resourceRequestVersions[name] !== version) return { name, data, committed: false };
  state[name] = data;
  return { name, data, committed: true };
}
