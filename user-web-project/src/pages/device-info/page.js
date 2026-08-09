(function (app) {
    // 裝置資訊頁：公開唯讀，呈現 /api/device 與 /api/storage。
    // 進入頁面抓一次，之後每 10 秒背景更新，不提供手動重新整理；
    // 更新失敗時沿用最後成功值，離線提示統一由 gate banner 呈現。
    var POLL_MS = 10000;

    function t(key, replacements) {
        return app.i18n.t(key, replacements);
    }

    function byteValue(value) {
        return typeof value === 'number' && isFinite(value) && value > 0 ? value : 0;
    }

    function formatBytes(value) {
        var bytes = byteValue(value);
        if (bytes < 1024) {
            return bytes + ' B';
        }
        if (bytes < 1048576) {
            return (bytes / 1024).toFixed(1) + ' KB';
        }
        return (bytes / 1048576).toFixed(2) + ' MB';
    }

    function partitionById(partitions, id) {
        return (partitions || []).filter(function (partition) {
            return partition.id === id;
        })[0] || {};
    }

    function textValue(value) {
        return value === undefined || value === null || value === '' ? '—' : String(value);
    }

    function field(labelKey, valueText) {
        return app.utils.dom.el('div', {
            className: 'device-field',
            children: [
                app.utils.dom.el('div', { className: 'device-field-label', text: t(labelKey) }),
                app.utils.dom.el('div', { className: 'device-field-value', text: valueText })
            ]
        });
    }

    // 分段容量長條圖 + 分欄 stat；0 值分段隱藏，總和作為 100% 分母，也就是卡片標題旁的總空間。
    function capacityCard(titleKey) {
        var bar = app.utils.dom.el('div', { className: 'capacity-bar' });
        var legend = app.utils.dom.el('dl', { className: 'capacity-stats' });
        var totalValue = app.utils.dom.el('span', { className: 'capacity-total-value', text: '—' });
        var node = app.utils.dom.el('section', {
            className: 'panel-section device-gate',
            children: [
                app.utils.dom.el('div', {
                    className: 'device-card-header',
                    children: [
                        app.utils.dom.el('h2', { text: t(titleKey) }),
                        app.utils.dom.el('span', {
                            className: 'capacity-total',
                            children: [
                                app.utils.dom.el('span', {
                                    className: 'capacity-total-label',
                                    text: t('deviceCapacityTotal')
                                }),
                                totalValue
                            ]
                        })
                    ]
                }),
                app.utils.dom.el('div', {
                    className: 'panel-body device-card-body',
                    children: [bar, legend]
                })
            ]
        });

        function render(segments) {
            app.utils.dom.clear(bar);
            app.utils.dom.clear(legend);
            // 比照 builtin-web capacity-stats：每個分段一欄，長標籤在欄內換行不擠壓數值。
            legend.className = 'capacity-stats segments-' + segments.length;
            var total = segments.reduce(function (sum, segment) {
                return sum + byteValue(segment.bytes);
            }, 0);
            totalValue.textContent = formatBytes(total);
            segments.forEach(function (segment) {
                var bytes = byteValue(segment.bytes);
                var title = t(segment.labelKey) + ': ' + formatBytes(bytes);
                if (total > 0 && bytes > 0) {
                    var span = app.utils.dom.el('span', {
                        className: 'capacity-seg capacity-color-' + segment.color,
                        attrs: { title: title, 'aria-label': title }
                    });
                    span.style.flexBasis = ((bytes / total) * 100) + '%';
                    bar.appendChild(span);
                }
                legend.appendChild(app.utils.dom.el('div', {
                    className: 'capacity-stat',
                    children: [
                        app.utils.dom.el('dt', {
                            className: 'capacity-stat-label',
                            children: [
                                app.utils.dom.el('i', {
                                    className: 'capacity-chip capacity-color-' + segment.color,
                                    attrs: { 'aria-hidden': 'true' }
                                }),
                                app.utils.dom.el('span', { text: t(segment.labelKey) })
                            ]
                        }),
                        app.utils.dom.el('dd', {
                            className: 'capacity-stat-value',
                            text: formatBytes(bytes)
                        })
                    ]
                }));
            });
        }

        return { node: node, render: render };
    }

    app.pages.deviceInfoPage = {
        id: 'device-info',
        titleKey: 'deviceInfoTitle',
        mount: function mount(container) {
            var self = this;
            this.mounted = true;
            this.generation = 0;
            this.deviceData = null;
            this.storageData = null;

            var deviceGrid = app.utils.dom.el('div', { className: 'device-grid' });
            var imageCard = capacityCard('deviceCardImageSpace');
            var fileCard = capacityCard('deviceCardFileStorage');
            var section = app.utils.dom.el('section', {
                className: 'device-page',
                children: [
                    app.utils.dom.el('section', {
                        className: 'panel-section device-gate',
                        children: [
                            app.utils.dom.el('h2', { text: t('deviceCardDevice') }),
                            app.utils.dom.el('div', {
                                className: 'panel-body device-card-body',
                                children: [deviceGrid]
                            })
                        ]
                    }),
                    imageCard.node,
                    fileCard.node
                ]
            });

            function renderDevice(device) {
                app.utils.dom.clear(deviceGrid);
                var chip = device.chip_model
                    ? device.chip_model + ' (rev ' + textValue(device.chip_revision) + ')'
                    : '—';
                deviceGrid.appendChild(field('deviceFieldChip', chip));
                deviceGrid.appendChild(field('deviceFieldCpuCores', textValue(device.cpu_cores)));
                deviceGrid.appendChild(field('deviceFieldFlash', device.flash_mb ? device.flash_mb + ' MB' : '—'));
                deviceGrid.appendChild(field(
                    'deviceFieldHeap',
                    typeof device.heap_used_percent === 'number' ? device.heap_used_percent + '%' : '—'
                ));
                deviceGrid.appendChild(field('deviceFieldMac', textValue(device.mac_address)));
                deviceGrid.appendChild(field('deviceFieldHostname', textValue(device.hostname)));
            }

            // 依 builtin-web Hardware 頁的推導拆出各分段。
            function renderStorage(storage) {
                var flash = storage.flash || {};
                var fixed = flash.fixed_regions || {};
                var partitions = flash.partitions || [];
                var appInfo = storage.app || {};
                var appCapacity = appInfo.capacity || {};
                var user = storage.user || {};
                var userCapacity = user.capacity || {};
                var limits = user.limits || {};
                var frontendBytes = byteValue(appCapacity.frontend_payload_bytes);
                imageCard.render([
                    {
                        labelKey: 'deviceSegSystem',
                        color: 1,
                        bytes: byteValue(fixed.bootloader_reserved_bytes)
                            + byteValue(fixed.partition_table_bytes)
                            + byteValue(partitionById(partitions, 'nvs').size_bytes)
                            + byteValue(partitionById(partitions, 'otadata').size_bytes)
                    },
                    {
                        labelKey: 'deviceSegFirmware',
                        color: 2,
                        bytes: Math.max(0, byteValue(appCapacity.firmware_image_bytes) - frontendBytes)
                    },
                    { labelKey: 'deviceSegFrontend', color: 3, bytes: frontendBytes },
                    { labelKey: 'deviceSegFlashAvailable', color: 5, bytes: byteValue(appCapacity.available_bytes) }
                ]);
                fileCard.render([
                    {
                        labelKey: 'deviceSegUserSettings',
                        color: 1,
                        bytes: byteValue(partitionById(partitions, 'user_nvs').size_bytes)
                    },
                    { labelKey: 'deviceSegUserFiles', color: 2, bytes: byteValue(userCapacity.used_bytes) },
                    { labelKey: 'deviceSegFileBuffer', color: 3, bytes: byteValue(limits.reserved_bytes) },
                    {
                        labelKey: 'deviceSegSystemLog',
                        color: 4,
                        bytes: byteValue(partitionById(partitions, 'coredump').size_bytes)
                    },
                    // Available 直接使用 max_upload_bytes：即當下單一新檔的上限。
                    { labelKey: 'deviceSegAvailable', color: 5, bytes: byteValue(limits.max_upload_bytes) }
                ]);
            }

            function load() {
                self.generation += 1;
                var current = self.generation;
                Promise.all([
                    app.device.api.resources.device(),
                    app.device.api.resources.storage()
                ]).then(
                    function (results) {
                        if (!self.mounted || current !== self.generation) {
                            return;
                        }
                        self.deviceData = results[0];
                        self.storageData = results[1];
                        renderDevice(self.deviceData);
                        renderStorage(self.storageData);
                    },
                    // 失敗時沿用最後成功值；離線提示由 gate banner 負責。
                    function () {}
                );
            }

            this.gate = app.device.bindLiveGate(section, { onOnline: load });
            section.insertBefore(this.gate.banner, section.children[0]);
            container.appendChild(section);
            this.timerId = window.setInterval(load, POLL_MS);
            load();
        },
        unmount: function unmount() {
            this.mounted = false;
            if (this.timerId) {
                window.clearInterval(this.timerId);
                this.timerId = null;
            }
            if (this.gate) {
                this.gate.unbind();
                this.gate = null;
            }
        }
    };
})(window.DitherApp);
