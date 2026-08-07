(function defineHardwarePage(app) {
  const { detailRow, el, translated } = app.utils.dom;

  function createDevicePanel() {
    return el('article', {
      className: 'panel detail-panel',
      children: [
        el('header', {
          className: 'panel-head',
          children: [translated('h2', 'hardware', 'Hardware')]
        }),
        el('dl', {
          className: 'detail-list two-column',
          children: [
            detailRow('chipModel', 'Chip model', 'hardwareChipModel'),
            detailRow('chipRevision', 'Chip revision', 'hardwareChipRevision'),
            detailRow('cpuCores', 'CPU cores', 'hardwareCpuCores'),
            detailRow('flash', 'Flash', 'hardwareFlash'),
            detailRow('heapUsed', 'Heap used', 'hardwareHeap'),
            detailRow('macAddress', 'MAC address', 'hardwareMac')
          ]
        })
      ]
    });
  }

  function capacitySegment(segment) {
    return el('span', {
      id: segment.barId,
      className: `capacity-segment capacity-color-${segment.paletteSlot}`
    });
  }

  function capacityStat(segment) {
    const label = el('dt', {
      className: 'capacity-stat-label',
      children: [
        el('i', {
          className: `capacity-key capacity-color-${segment.paletteSlot}`,
          attrs: { 'aria-hidden': 'true' }
        }),
        translated('span', segment.labelKey, segment.label)
      ]
    });
    return el('div', {
      className: 'capacity-stat',
      children: [label, el('dd', { id: segment.valueId, text: '—' })]
    });
  }

  function createCapacityPanel(options) {
    const title = el('div', {
      className: 'capacity-title',
      children: [translated('h2', options.titleKey, options.title)]
    });
    const headMetaChildren = [el('span', {
      className: 'capacity-total',
      children: [
        translated('small', 'total', 'Total'),
        el('strong', { id: options.totalId, text: '—' })
      ]
    })];

    return el('article', {
      className: 'panel detail-panel capacity-panel',
      children: [
        el('header', {
          className: 'panel-head capacity-head',
          children: [
            title,
            el('div', { className: 'capacity-head-meta', children: headMetaChildren })
          ]
        }),
        el('div', {
          className: 'capacity-content',
          children: [
            el('div', {
              className: 'capacity-bar',
              attrs: { role: 'img', 'aria-label': options.title },
              dataset: { i18nAriaLabel: options.titleKey },
              children: options.segments.map(capacitySegment)
            }),
            el('dl', {
              className: `capacity-stats segments-${options.segments.length}`,
              children: options.segments.map(capacityStat)
            })
          ]
        })
      ]
    });
  }

  const imageSegments = [
    {
      paletteSlot: 1, barId: 'hardwareSystemBar', valueId: 'hardwareSystemUsed',
      labelKey: 'systemUsedSpace', label: 'System space'
    },
    {
      paletteSlot: 2, barId: 'hardwareFirmwareBar', valueId: 'hardwareFirmwareProgram',
      labelKey: 'firmwareProgram', label: 'Firmware program'
    },
    {
      paletteSlot: 3, barId: 'hardwareFrontendBar', valueId: 'hardwareFrontendSize',
      labelKey: 'frontendSize', label: 'Frontend size'
    },
    {
      paletteSlot: 5, barId: 'hardwareAppAvailableBar', valueId: 'hardwareAppAvailable',
      labelKey: 'firmwareAvailableSpace', label: 'Firmware available space'
    }
  ];

  const storageSegments = [
    {
      paletteSlot: 1, barId: 'hardwareUserNvsBar', valueId: 'hardwareUserNvs',
      labelKey: 'userSettingsSpace', label: 'User settings'
    },
    {
      paletteSlot: 2, barId: 'hardwareUserFilesBar', valueId: 'hardwareStorageUsed',
      labelKey: 'userFiles', label: 'User files'
    },
    {
      paletteSlot: 3, barId: 'hardwareStorageBufferBar', valueId: 'hardwareStorageBuffer',
      labelKey: 'fileBufferSpace', label: 'File buffer space'
    },
    {
      paletteSlot: 4, barId: 'hardwareCoreDumpBar', valueId: 'hardwareCoreDump',
      labelKey: 'systemLogSpace', label: 'System log space'
    },
    {
      paletteSlot: 5, barId: 'hardwareStorageAvailableBar', valueId: 'hardwareStorageAvailable',
      labelKey: 'availableSpace', label: 'Available space'
    }
  ];

  function createHardwarePage() {
    return el('section', {
      id: 'page-hardware',
      className: 'page active',
      dataset: { pageView: 'hardware' },
      children: [
        el('div', {
          className: 'detail-grid',
          children: [
            createDevicePanel(),
            createCapacityPanel({
              titleKey: 'imageSpace', title: 'Image space',
              totalId: 'hardwareImageTotal',
              segments: imageSegments
            }),
            createCapacityPanel({
              titleKey: 'fileStorage', title: 'File storage',
              totalId: 'hardwareStorageTotal',
              segments: storageSegments
            })
          ]
        })
      ]
    });
  }

  function byteValue(value) {
    const number = Number(value);
    return Number.isFinite(number) && number > 0 ? number : 0;
  }

  function partitionById(partitions, id) {
    return partitions.find((partition) => partition.id === id) || {};
  }

  function renderSegmentWidths(segments, values) {
    const total = values.reduce((sum, value) => sum + byteValue(value), 0);
    segments.forEach((segment, index) => {
      const element = $(segment.barId);
      const value = byteValue(values[index]);
      const percent = total > 0 ? value * 100 / total : 0;
      const description = `${t(segment.labelKey)}: ${formatBytes(value)}`;
      element.style.flexBasis = `${percent}%`;
      element.hidden = value === 0;
      element.title = description;
      element.setAttribute('aria-label', description);
    });
  }

  function render() {
    if (!$('page-hardware') || !state.device || !state.storage) return;
    const device = state.device;
    const flash = state.storage.flash || {};
    const fixedRegions = flash.fixed_regions || {};
    const partitions = Array.isArray(flash.partitions) ? flash.partitions : [];
    const appCapacity = state.storage.app && state.storage.app.capacity || {};
    const userStorage = state.storage.user || {};
    const userCapacity = userStorage.capacity || {};
    const userLimits = userStorage.limits || {};
    const nvs = partitionById(partitions, 'nvs');
    const otadata = partitionById(partitions, 'otadata');
    const userNvs = partitionById(partitions, 'user_nvs');
    const coredump = partitionById(partitions, 'coredump');
    const frontendBytes = byteValue(appCapacity.frontend_payload_bytes);
    const firmwareImageBytes = byteValue(appCapacity.firmware_image_bytes);
    const firmwareProgramBytes = Math.max(0, firmwareImageBytes - frontendBytes);
    const systemBytes = byteValue(fixedRegions.bootloader_reserved_bytes)
      + byteValue(fixedRegions.partition_table_bytes)
      + byteValue(nvs.size_bytes)
      + byteValue(otadata.size_bytes);
    const appAvailableBytes = byteValue(appCapacity.available_bytes);
    const userUsedBytes = byteValue(userCapacity.used_bytes);
    const userAvailableBytes = byteValue(userLimits.max_upload_bytes);
    const imageValues = [systemBytes, firmwareProgramBytes, frontendBytes, appAvailableBytes];
    const storageValues = [
      byteValue(userNvs.size_bytes), userUsedBytes,
      byteValue(userLimits.reserved_bytes), byteValue(coredump.size_bytes),
      userAvailableBytes
    ];

    setText('hardwareChipModel', device.chip_model);
    setText('hardwareChipRevision', device.chip_revision);
    setText('hardwareCpuCores', device.cpu_cores);
    setText('hardwareFlash', device.flash_mb == null ? '—' : `${device.flash_mb} MB`);
    setText('hardwareHeap', device.heap_used_percent == null ? '—' : `${device.heap_used_percent}%`);
    setText('hardwareMac', device.mac_address);
    setText('hardwareImageTotal', formatBytes(imageValues.reduce((sum, value) => sum + value, 0)));
    setText('hardwareSystemUsed', formatBytes(systemBytes));
    setText('hardwareFirmwareProgram', formatBytes(firmwareProgramBytes));
    setText('hardwareFrontendSize', formatBytes(frontendBytes));
    setText('hardwareAppAvailable', formatBytes(appAvailableBytes));
    setText('hardwareStorageTotal', formatBytes(storageValues.reduce((sum, value) => sum + value, 0)));
    setText('hardwareUserNvs', formatBytes(userNvs.size_bytes));
    setText('hardwareStorageUsed', formatBytes(userUsedBytes));
    setText('hardwareCoreDump', formatBytes(coredump.size_bytes));
    setText('hardwareStorageBuffer', formatBytes(userLimits.reserved_bytes));
    setText('hardwareStorageAvailable', formatBytes(userAvailableBytes));
    renderSegmentWidths(imageSegments, imageValues);
    renderSegmentWidths(storageSegments, storageValues);
  }

  const page = {
    id: 'hardware',
    titleKey: 'hardware',
    descriptionKey: 'hardwareOverview',
    resources: ['device', 'storage'],

    mount(host) {
      host.replaceChildren(createHardwarePage());
      applyLanguage();
      render();
    },

    render,
    unmount() {}
  };

  app.pages.hardware = page;
})(window.DeviceConsole);
