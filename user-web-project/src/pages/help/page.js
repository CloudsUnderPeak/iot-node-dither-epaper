(function (app) {
    app.pages.help = app.pages.help || {};

    var dom = app.utils.dom;
    var manifest = app.pages.help.documentManifest;
    var mounted = null;

    function bundle() {
        return app.i18n.t('helpBundle');
    }

    function contentFor(document, currentBundle) {
        return currentBundle.documents[document.id] || currentBundle.documents.home;
    }

    function createRouteLink(document, text, context, className) {
        var link = dom.el('a', {
            className: className || '',
            text: text,
            attrs: { href: '#/' + document.route }
        });
        link.addEventListener('click', function (event) {
            if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) {
                return;
            }
            event.preventDefault();
            context.navigate(document.route);
        });
        return link;
    }

    function navItem(document, activeId, context, currentBundle, depth) {
        var content = contentFor(document, currentBundle);
        var item = dom.el('li', { className: 'help-nav-item' });
        var link = createRouteLink(document, content.title, context, 'help-nav-link');
        link.style.setProperty('--help-nav-depth', depth || 0);
        if (document.id === activeId) {
            link.classList.add('is-active');
            link.setAttribute('aria-current', 'page');
        }
        item.appendChild(link);
        var children = document.id === 'home' ? [] : manifest.childrenOf(document.id);
        if (children.length) {
            var list = dom.el('ul', { className: 'help-nav-list' });
            children.forEach(function (child) {
                list.appendChild(navItem(child, activeId, context, currentBundle, (depth || 0) + 1));
            });
            item.appendChild(list);
        }
        return item;
    }

    function buildNavigation(document, context, currentBundle) {
        var nav = dom.el('nav', {
            className: 'help-document-nav',
            attrs: { 'aria-label': currentBundle.ui.documents }
        });
        var home = manifest.get('home');
        var startList = dom.el('ul', { className: 'help-nav-list' });
        var algorithmList = dom.el('ul', { className: 'help-nav-list' });

        nav.appendChild(dom.el('div', { className: 'help-nav-group-label', text: currentBundle.ui.overviewGroup }));
        nav.appendChild(dom.el('ul', {
            className: 'help-nav-list',
            children: [navItem(home, document.id, context, currentBundle, 0)]
        }));

        manifest.childrenOf('home').forEach(function (child) {
            if (child.group === 'start') {
                startList.appendChild(navItem(child, document.id, context, currentBundle, 0));
            } else if (child.group === 'algorithms') {
                algorithmList.appendChild(navItem(child, document.id, context, currentBundle, 0));
            }
        });
        nav.appendChild(dom.el('div', { className: 'help-nav-group-label', text: currentBundle.ui.startGroup }));
        nav.appendChild(startList);
        nav.appendChild(dom.el('div', { className: 'help-nav-group-label', text: currentBundle.ui.algorithmsGroup }));
        nav.appendChild(algorithmList);
        return nav;
    }

    function renderTable(table, currentBundle) {
        var node = dom.el('div', { className: 'help-table-wrap' });
        var tableNode = dom.el('table', { className: 'help-table' });
        var headRow = dom.el('tr');
        table.headers.forEach(function (header) {
            headRow.appendChild(dom.el('th', { text: header, attrs: { scope: 'col' } }));
        });
        tableNode.appendChild(dom.el('thead', { children: [headRow] }));
        var body = dom.el('tbody');
        table.rows.forEach(function (row) {
            var rowNode = dom.el('tr');
            row.forEach(function (cell) {
                rowNode.appendChild(dom.el('td', {
                    text: app.pages.help.contentModel.tableCellText(cell, currentBundle)
                }));
            });
            body.appendChild(rowNode);
        });
        tableNode.appendChild(body);
        node.appendChild(tableNode);
        return node;
    }

    function renderCards(cards, context) {
        var grid = dom.el('div', { className: 'help-link-cards' });
        cards.forEach(function (card) {
            var article = dom.el('article', { className: 'help-link-card' });
            var target = manifest.get(card.linkId);
            article.appendChild(target
                ? createRouteLink(target, card.title, context, 'help-card-title')
                : dom.el('h3', { className: 'help-card-title', text: card.title }));
            article.appendChild(dom.el('p', { text: card.body }));
            grid.appendChild(article);
        });
        return grid;
    }

    function definition(label, value) {
        return dom.el('div', {
            children: [dom.el('dt', { text: label }), dom.el('dd', { text: value })]
        });
    }

    function renderAlgorithms(section, currentBundle) {
        var grid = dom.el('div', { className: 'help-algorithm-list' });
        app.pages.help.contentModel.algorithmsForSection(section, currentBundle)
            .forEach(function (algorithm) {
                var article = dom.el('article', { className: 'help-algorithm-card' });
                article.appendChild(dom.el('h3', { text: algorithm.name }));
                article.appendChild(dom.el('p', {
                    className: 'help-algorithm-summary',
                    text: algorithm.summary
                }));
                article.appendChild(dom.el('dl', {
                    className: 'help-algorithm-facts',
                    children: [
                        definition(currentBundle.ui.characteristics, algorithm.characteristics),
                        definition(currentBundle.ui.bestFor, algorithm.bestFor),
                        definition(currentBundle.ui.avoidWhen, algorithm.avoid),
                        definition(currentBundle.ui.controls, algorithm.controls),
                        definition(currentBundle.ui.structure, algorithm.structure)
                    ]
                }));
                grid.appendChild(article);
            });
        return grid;
    }

    function renderSteps(steps) {
        var list = dom.el('ol', { className: 'help-steps' });
        steps.forEach(function (step) {
            list.appendChild(dom.el('li', {
                children: [dom.el('strong', { text: step.title }), dom.el('p', { text: step.body })]
            }));
        });
        return list;
    }

    function renderSection(section, context, currentBundle) {
        var node = dom.el('section', { className: 'help-article-section', attrs: { id: section.id } });
        node.appendChild(dom.el('h2', { text: section.title }));
        (section.paragraphs || []).forEach(function (paragraph) {
            node.appendChild(dom.el('p', { text: paragraph }));
        });
        if (section.visual) {
            node.appendChild(app.pages.help.visuals.build(section.visual, currentBundle));
        }
        if (section.steps) {
            node.appendChild(renderSteps(section.steps));
        }
        if (section.bullets) {
            var list = dom.el('ul');
            section.bullets.forEach(function (item) {
                list.appendChild(dom.el('li', { text: item }));
            });
            node.appendChild(list);
        }
        if (section.table) {
            node.appendChild(renderTable(section.table, currentBundle));
        }
        if (section.cards) {
            node.appendChild(renderCards(section.cards, context));
        }
        if (section.algorithms) {
            node.appendChild(renderAlgorithms(section, currentBundle));
        }
        if (section.note) {
            node.appendChild(dom.el('aside', { className: 'help-note', text: section.note }));
        }
        return node;
    }

    function buildBreadcrumbs(document, context, currentBundle) {
        var breadcrumbs = dom.el('nav', {
            className: 'help-breadcrumbs',
            attrs: { 'aria-label': currentBundle.ui.breadcrumbs }
        });
        manifest.ancestorsOf(document.id).concat([document]).forEach(function (item, index, all) {
            var content = contentFor(item, currentBundle);
            if (index === all.length - 1) {
                breadcrumbs.appendChild(dom.el('span', { text: content.title, attrs: { 'aria-current': 'page' } }));
            } else {
                breadcrumbs.appendChild(createRouteLink(item, content.title, context));
                breadcrumbs.appendChild(dom.el('span', { text: '/', attrs: { 'aria-hidden': 'true' } }));
            }
        });
        return breadcrumbs;
    }

    function buildToc(content, currentBundle) {
        var aside = dom.el('aside', { className: 'help-toc' });
        aside.appendChild(dom.el('div', { className: 'help-toc-title', text: currentBundle.ui.onThisPage }));
        var list = dom.el('ul');
        content.sections.forEach(function (section) {
            var button = dom.el('button', { text: section.title, attrs: { type: 'button' } });
            button.addEventListener('click', function () {
                var target = document.getElementById(section.id);
                if (target) {
                    target.scrollIntoView({ behavior: 'smooth', block: 'start' });
                }
            });
            list.appendChild(dom.el('li', { children: [button] }));
        });
        aside.appendChild(list);
        return aside;
    }

    function buildPager(document, context, currentBundle) {
        var documents = manifest.all();
        var index = documents.findIndex(function (item) { return item.id === document.id; });
        var pager = dom.el('nav', {
            className: 'help-pager',
            attrs: { 'aria-label': currentBundle.ui.documentPagination }
        });
        var previous = documents[index - 1];
        var next = documents[index + 1];
        if (previous) {
            pager.appendChild(createRouteLink(
                previous,
                '← ' + currentBundle.ui.previous + ': ' + contentFor(previous, currentBundle).title,
                context,
                'help-pager-link'
            ));
        }
        if (next) {
            pager.appendChild(createRouteLink(
                next,
                currentBundle.ui.next + ': ' + contentFor(next, currentBundle).title + ' →',
                context,
                'help-pager-link is-next'
            ));
        }
        return pager;
    }

    function renderDocument(route) {
        if (!mounted) {
            return;
        }
        var currentBundle = bundle();
        var document = manifest.fromRoute(route) || manifest.get('home');
        var content = contentFor(document, currentBundle);
        var root = mounted.root;
        var context = mounted.context;
        dom.clear(root);

        var sidebar = dom.el('aside', { className: 'help-sidebar' });
        var toggle = dom.el('button', {
            className: 'help-nav-toggle',
            text: currentBundle.ui.openDocuments,
            attrs: { type: 'button', 'aria-expanded': 'false' }
        });
        toggle.addEventListener('click', function () {
            var open = root.classList.toggle('is-nav-open');
            toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
            toggle.textContent = open ? currentBundle.ui.closeDocuments : currentBundle.ui.openDocuments;
        });
        sidebar.appendChild(dom.el('div', { className: 'help-sidebar-title', text: currentBundle.ui.documents }));
        sidebar.appendChild(toggle);
        sidebar.appendChild(buildNavigation(document, context, currentBundle));

        var article = dom.el('article', { className: 'help-article' });
        article.appendChild(buildBreadcrumbs(document, context, currentBundle));
        article.appendChild(dom.el('div', { className: 'help-eyebrow', text: content.eyebrow }));
        article.appendChild(dom.el('h1', { text: content.title }));
        article.appendChild(dom.el('p', { className: 'help-lead', text: content.lead }));
        content.sections.forEach(function (section) {
            article.appendChild(renderSection(section, context, currentBundle));
        });
        article.appendChild(buildPager(document, context, currentBundle));

        var main = dom.el('div', {
            className: 'help-main',
            children: [article, buildToc(content, currentBundle)]
        });
        root.appendChild(sidebar);
        root.appendChild(main);
        root.scrollTop = 0;
        main.scrollTop = 0;
    }

    app.pages.helpPage = {
        id: 'help',
        titleKey: 'helpTitle',
        mount: function mount(container, context) {
            var root = dom.el('div', { className: 'help-page' });
            mounted = { root: root, context: context };
            container.appendChild(root);
            renderDocument(context.route || 'help');
        },
        onRouteChange: function onRouteChange(route) {
            renderDocument(route);
        },
        unmount: function unmount() {
            mounted = null;
        }
    };
})(window.DitherApp);
