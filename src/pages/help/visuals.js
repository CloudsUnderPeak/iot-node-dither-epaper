(function (app) {
    app.pages.help = app.pages.help || {};

    var dom = app.utils.dom;
    var comparisonSets = {
        family: [
            { id: 'floyd', asset: 'assets/help/family-floyd.png' },
            { id: 'bayer', asset: 'assets/help/family-bayer.png' },
            { id: 'dot', asset: 'assets/help/family-dot.png' }
        ],
        mapping: [
            { id: 'nearest', asset: 'assets/help/family-bayer.png' },
            { id: 'pair', asset: 'assets/help/mapping-pair.png' },
            { id: 'tri', asset: 'assets/help/mapping-tri.png' }
        ],
        distance: [
            { id: 'bt709', asset: 'assets/help/family-floyd.png' },
            { id: 'rgb', asset: 'assets/help/distance-rgb.png' },
            { id: 'ciede', asset: 'assets/help/distance-ciede2000.png' }
        ]
    };

    function drawSyntheticSource(canvas) {
        var width = 480;
        var height = 288;
        var context = canvas.getContext('2d');
        var imageData = context.createImageData(width, height);
        var data = imageData.data;
        canvas.width = width;
        canvas.height = height;

        for (var y = 0; y < height; y += 1) {
            for (var x = 0; x < width; x += 1) {
                var index = (y * width + x) * 4;
                var nx = x / (width - 1);
                var ny = y / (height - 1);
                var dx = nx - 0.54;
                var dy = ny - 0.48;
                var circle = dx * dx + dy * dy < 0.08 ? 85 : 0;
                data[index] = nx * 255;
                data[index + 1] = ny * 220 + circle;
                data[index + 2] = (1 - nx) * 190 + (1 - ny) * 60;
                data[index + 3] = 255;
            }
        }
        context.putImageData(imageData, 0, 0);
    }

    function buildPipeline(bundle) {
        var labels = [
            app.i18n.t('panelPalette'),
            app.i18n.t('labelColorDistance'),
            app.i18n.t('labelPaletteMapping'),
            app.i18n.t('panelDither'),
            bundle.ui.after
        ];
        var node = dom.el('div', {
            className: 'help-flow',
            attrs: { role: 'img', 'aria-label': labels.join(' → ') }
        });
        labels.forEach(function (label, index) {
            node.appendChild(dom.el('span', { className: 'help-flow-step', text: label }));
            if (index < labels.length - 1) {
                node.appendChild(dom.el('span', {
                    className: 'help-flow-arrow',
                    text: '→',
                    attrs: { 'aria-hidden': 'true' }
                }));
            }
        });
        return node;
    }

    function buildComparison(spec, bundle) {
        var localized = bundle.visuals[spec.set];
        var options = comparisonSets[spec.set];
        var selected = options[0];
        var node = dom.el('figure', { className: 'help-comparison' });
        var tabs = dom.el('div', {
            className: 'help-example-tabs',
            attrs: { role: 'group', 'aria-label': localized.title }
        });
        var frame = dom.el('div', { className: 'help-comparison-frame' });
        var source = dom.el('canvas', {
            className: 'help-comparison-source',
            attrs: { width: '480', height: '288', 'aria-label': bundle.ui.before }
        });
        var result = dom.el('img', {
            className: 'help-comparison-result',
            attrs: { src: selected.asset, alt: localized[selected.id] }
        });
        var divider = dom.el('span', { className: 'help-comparison-divider', attrs: { 'aria-hidden': 'true' } });
        var range = dom.el('input', {
            className: 'help-comparison-range',
            attrs: {
                type: 'range', min: '0', max: '100', value: '58',
                'aria-label': bundle.ui.comparisonHint
            }
        });
        var caption = dom.el('figcaption', {
            className: 'help-comparison-caption',
            text: localized[selected.id + 'Caption']
        });
        var labels = dom.el('div', {
            className: 'help-comparison-labels',
            children: [
                dom.el('span', { text: bundle.ui.after }),
                dom.el('span', { text: bundle.ui.before })
            ]
        });

        function setPosition(value) {
            result.style.clipPath = 'inset(0 ' + (100 - value) + '% 0 0)';
            divider.style.left = value + '%';
        }

        function selectOption(option, button) {
            result.src = option.asset;
            result.alt = localized[option.id];
            caption.textContent = localized[option.id + 'Caption'];
            Array.prototype.forEach.call(tabs.children, function (tab) {
                tab.setAttribute('aria-pressed', tab === button ? 'true' : 'false');
            });
        }

        options.forEach(function (option, index) {
            var button = dom.el('button', {
                className: 'help-example-tab',
                text: localized[option.id],
                attrs: { type: 'button', 'aria-pressed': index === 0 ? 'true' : 'false' }
            });
            button.addEventListener('click', function () {
                selectOption(option, button);
            });
            tabs.appendChild(button);
        });

        range.addEventListener('input', function () {
            setPosition(Number(range.value));
        });
        drawSyntheticSource(source);
        setPosition(Number(range.value));
        frame.appendChild(source);
        frame.appendChild(result);
        frame.appendChild(divider);
        frame.appendChild(range);
        node.appendChild(dom.el('h3', { className: 'help-visual-title', text: localized.title }));
        node.appendChild(tabs);
        node.appendChild(frame);
        node.appendChild(labels);
        node.appendChild(caption);
        node.appendChild(dom.el('code', { className: 'help-comparison-settings', text: localized.settings }));
        return node;
    }

    function buildGrid(spec, className) {
        var rows = spec.rows;
        var columns = Math.max.apply(null, rows.map(function (row) { return row.length; }));
        var figure = dom.el('figure', { className: 'help-grid-figure' });
        var grid = dom.el('div', {
            className: className,
            attrs: { role: 'img', 'aria-label': spec.title }
        });
        grid.style.setProperty('--help-grid-columns', columns);
        var flat = [];
        rows.forEach(function (row) {
            row.forEach(function (value) {
                if (typeof value === 'number') {
                    flat.push(value);
                }
            });
        });
        var max = flat.length ? Math.max.apply(null, flat) : 1;
        rows.forEach(function (row) {
            for (var index = 0; index < columns; index += 1) {
                var value = row[index] === undefined ? '' : row[index];
                var cell = dom.el('span', {
                    className: 'help-grid-cell' + (value === 'current' || value === '目前' ? ' is-current' : ''),
                    text: value
                });
                cell.style.setProperty('--help-grid-level', typeof value === 'number' ? (value + 1) / (max + 2) : 0);
                grid.appendChild(cell);
            }
        });
        figure.appendChild(dom.el('figcaption', { className: 'help-visual-title', text: spec.title }));
        figure.appendChild(grid);
        return figure;
    }

    function rgbFromHex(hex) {
        return {
            r: parseInt(hex.slice(1, 3), 16),
            g: parseInt(hex.slice(3, 5), 16),
            b: parseInt(hex.slice(5, 7), 16)
        };
    }

    function buildDistanceExplorer(bundle) {
        var palette = ['#000000', '#ffffff', '#008000', '#0000ff', '#ff0000', '#ffff00'];
        var metrics = [
            ['euclidean-bt709', 'Euclidean BT.709'],
            ['euclidean-rgb', 'Euclidean RGB'],
            ['manhattan-bt709', 'Manhattan BT.709'],
            ['manhattan-rgb', 'Manhattan RGB'],
            ['ciede2000', 'CIEDE2000']
        ];
        var node = dom.el('div', { className: 'help-distance-explorer' });
        var input = dom.el('input', {
            attrs: { type: 'color', value: '#68a6c8', 'aria-label': bundle.ui.targetColor }
        });
        var valueLabel = dom.el('code', { text: input.value.toUpperCase() });
        var results = dom.el('div', { className: 'help-distance-results' });
        var paletteNode = dom.el('div', { className: 'help-distance-palette' });

        palette.forEach(function (hex) {
            paletteNode.appendChild(dom.el('span', {
                className: 'help-distance-swatch',
                attrs: { title: hex, 'aria-label': hex }
            }));
            paletteNode.lastChild.style.backgroundColor = hex;
        });

        function nearest(metricId, target) {
            var measure = app.core.paletteUtils.createColorDistanceMeasurer(metricId);
            var selected = palette[0];
            var best = Infinity;
            palette.forEach(function (hex) {
                var distance = measure(target, rgbFromHex(hex));
                if (distance < best) {
                    best = distance;
                    selected = hex;
                }
            });
            return selected;
        }

        function render() {
            var target = rgbFromHex(input.value);
            valueLabel.textContent = input.value.toUpperCase();
            dom.clear(results);
            metrics.forEach(function (metric) {
                var selected = nearest(metric[0], target);
                var row = dom.el('div', { className: 'help-distance-result' });
                var swatch = dom.el('span', {
                    className: 'help-distance-result-swatch',
                    attrs: { 'aria-label': bundle.ui.nearestColor + ' ' + selected }
                });
                swatch.style.backgroundColor = selected;
                row.appendChild(dom.el('strong', { text: metric[1] }));
                row.appendChild(swatch);
                row.appendChild(dom.el('code', { text: selected.toUpperCase() }));
                results.appendChild(row);
            });
        }

        input.addEventListener('input', render);
        node.appendChild(dom.el('p', { className: 'help-visual-hint', text: bundle.ui.distanceExplorerHint }));
        node.appendChild(dom.el('div', {
            className: 'help-distance-target',
            children: [dom.el('span', { text: bundle.ui.targetColor }), input, valueLabel]
        }));
        node.appendChild(paletteNode);
        node.appendChild(results);
        render();
        return node;
    }

    app.pages.help.visuals = {
        build: function build(spec, bundle) {
            if (spec.type === 'pipeline') {
                return buildPipeline(bundle);
            }
            if (spec.type === 'comparison') {
                return buildComparison(spec, bundle);
            }
            if (spec.type === 'matrix') {
                return buildGrid(spec, 'help-matrix-grid');
            }
            if (spec.type === 'kernel') {
                return buildGrid(spec, 'help-kernel-grid');
            }
            if (spec.type === 'distance-explorer') {
                return buildDistanceExplorer(bundle);
            }
            return dom.el('div');
        }
    };
})(window.DitherApp);
