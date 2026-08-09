(function (app) {
    app.pages.help = app.pages.help || {};

    var documents = [
        { id: 'home', route: 'help', parentId: null, group: 'overview' },
        { id: 'introduction', route: 'help/introduction', parentId: 'home', group: 'start' },
        { id: 'quick-start', route: 'help/quick-start', parentId: 'home', group: 'start' },
        { id: 'dithering', route: 'help/dithering', parentId: 'home', group: 'algorithms' },
        {
            id: 'error-diffusion',
            route: 'help/dithering/error-diffusion',
            parentId: 'dithering',
            group: 'algorithms'
        },
        {
            id: 'ordered',
            route: 'help/dithering/ordered',
            parentId: 'dithering',
            group: 'algorithms'
        },
        {
            id: 'dot',
            route: 'help/dithering/dot',
            parentId: 'dithering',
            group: 'algorithms'
        },
        {
            id: 'palette-mapping',
            route: 'help/palette-mapping',
            parentId: 'home',
            group: 'algorithms'
        },
        {
            id: 'color-distance',
            route: 'help/color-distance',
            parentId: 'home',
            group: 'algorithms'
        }
    ];

    function findBy(field, value) {
        return documents.find(function (document) {
            return document[field] === value;
        }) || null;
    }

    app.pages.help.documentManifest = {
        all: function all() {
            return documents.slice();
        },
        get: function get(id) {
            return findBy('id', id);
        },
        fromRoute: function fromRoute(route) {
            return findBy('route', String(route || '').replace(/^#\/?/, '').replace(/\/+$/, ''));
        },
        childrenOf: function childrenOf(id) {
            return documents.filter(function (document) {
                return document.parentId === id;
            });
        },
        ancestorsOf: function ancestorsOf(id) {
            var ancestors = [];
            var current = findBy('id', id);
            while (current && current.parentId) {
                current = findBy('id', current.parentId);
                if (current) {
                    ancestors.unshift(current);
                }
            }
            return ancestors;
        }
    };
})(window.DitherApp);
