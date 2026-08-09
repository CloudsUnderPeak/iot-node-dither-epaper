(function (app) {
    function iconUrl(path) {
        var url = String(path || '').split('#')[0].trim();
        return url ? new URL(url, document.baseURI).href : '';
    }

    function iconImage(path) {
        var url = iconUrl(path);
        if (!url) {
            return null;
        }
        var image = document.createElement('img');
        image.className = 'svg-icon-img';
        image.src = url;
        image.alt = '';
        image.decoding = 'async';
        image.draggable = false;
        image.setAttribute('aria-hidden', 'true');
        return image;
    }

    function fallbackIcon(options, className) {
        var fallback = document.createElement('span');
        fallback.className = className + ' svg-icon-fallback';
        fallback.setAttribute('aria-hidden', 'true');
        fallback.textContent = options.fallbackText || '';
        return fallback;
    }

    function create(path, options) {
        options = options || {};
        var className = options.className ? 'svg-icon ' + options.className : 'svg-icon';
        var icon = document.createElement('span');
        icon.className = className;
        icon.setAttribute('aria-hidden', 'true');
        var image = iconImage(path);
        if (!image) {
            return fallbackIcon(options, className);
        }
        icon.appendChild(image);
        return icon;
    }

    function hydrate(root) {
        (root || document).querySelectorAll('[data-svg-icon]').forEach(function (node) {
            var image = iconImage(node.getAttribute('data-svg-icon'));
            node.classList.add('svg-icon');
            node.setAttribute('aria-hidden', 'true');
            if (image) {
                node.textContent = '';
                node.appendChild(image);
            }
        });
    }

    app.ui.svgIcons = {
        create: create,
        hydrate: hydrate,
        iconUrl: iconUrl
    };

    hydrate(document);
})(window.DitherApp);
