(function (app) {
    var MODE_GROUP_SOURCE = 'source';
    var MODE_GROUP_PREPARE = 'prepare';
    var MODE_GROUP_EDIT = 'edit';
    var MODE_GROUP_NONE = 'none';

    function hasImage(state) {
        return Boolean(state && state.sourceImageData);
    }

    function openPanels(state, ids) {
        state.openToolPanels = {};
        ids.forEach(function (id) {
            state.openToolPanels[id] = true;
        });
    }

    function availablePanelIds(state) {
        var registry = app.pages.ditherEditor.featureRegistry;
        var tools = registry && registry.dockTools ? registry.dockTools(state.pipeline) : [];
        return tools.map(function (tool) {
            return tool.id;
        });
    }

    function pruneUnavailablePanels(state) {
        var availableIds = availablePanelIds(state);
        state.openToolPanels = state.openToolPanels || {};
        Object.keys(state.openToolPanels).forEach(function (id) {
            if (availableIds.indexOf(id) === -1) {
                delete state.openToolPanels[id];
            }
        });
    }

    function panelGroupFor(id) {
        var registry = app.pages.ditherEditor.featureRegistry;
        return registry && registry.panelGroupFor ? registry.panelGroupFor(id) : null;
    }

    function panelIdsForGroup(state, group) {
        var registry = app.pages.ditherEditor.featureRegistry;
        return registry && registry.panelGroupIds ? registry.panelGroupIds(state.pipeline, group) : [];
    }

    function isToolInGroup(id, group) {
        return panelGroupFor(id) === group;
    }

    function hasPanelGroup(state, group) {
        return panelIdsForGroup(state, group).length > 0;
    }

    function hasOpenPanelGroup(state, group) {
        var ids = panelIdsForGroup(state, group);
        return ids.some(function (id) {
            return state.openToolPanels && state.openToolPanels[id];
        });
    }

    function normalizeEditViewMode(viewMode) {
        return viewMode === 'original' || viewMode === 'pixel' ? viewMode : 'result';
    }

    function closePanelGroup(state, group) {
        panelIdsForGroup(state, group).forEach(function (id) {
            state.openToolPanels[id] = false;
        });
    }

    function openPanelGroup(state, group) {
        var ids = panelIdsForGroup(state, group);
        openPanels(state, ids);
        return ids;
    }

    function normalize(state) {
        if (!hasImage(state)) {
            return enterSource(state);
        }
        pruneUnavailablePanels(state);
        if (state.mode === MODE_GROUP_PREPARE || hasOpenPanelGroup(state, MODE_GROUP_PREPARE)) {
            enterPrepare(state);
            return state.mode;
        }
        if (hasOpenPanelGroup(state, MODE_GROUP_EDIT)) {
            normalizeEdit(state);
            return state.mode;
        }
        if (hasOpenPanelGroup(state, MODE_GROUP_SOURCE)) {
            normalizeSource(state);
            return state.mode;
        }
        normalizeEdit(state);
        return state.mode;
    }

    function enterSource(state) {
        state.mode = MODE_GROUP_SOURCE;
        openPanelGroup(state, MODE_GROUP_SOURCE);
        state.viewMode = hasImage(state) && state.viewMode === 'original' ? 'original' : 'result';
        return state.mode;
    }

    function normalizeSource(state) {
        state.mode = MODE_GROUP_SOURCE;
        state.openToolPanels = state.openToolPanels || {};
        pruneUnavailablePanels(state);
        closePanelGroup(state, MODE_GROUP_PREPARE);
        state.viewMode = hasImage(state) && state.viewMode === 'original' ? 'original' : 'result';
        return state.mode;
    }

    function enterPrepare(state) {
        if (!hasImage(state)) {
            return enterSource(state);
        }
        if (!hasPanelGroup(state, MODE_GROUP_PREPARE)) {
            return enterEdit(state);
        }
        state.mode = MODE_GROUP_PREPARE;
        openPanelGroup(state, MODE_GROUP_PREPARE);
        state.viewMode = 'result';
        return state.mode;
    }

    function enterEdit(state) {
        if (!hasImage(state)) {
            return enterSource(state);
        }
        normalizeEdit(state);
        openPanelGroup(state, MODE_GROUP_EDIT);
        return state.mode;
    }

    function openEditPanel(state, activeTool) {
        if (!hasImage(state)) {
            return enterSource(state);
        }
        if (!isToolInGroup(activeTool, MODE_GROUP_EDIT)) {
            return enterEdit(state);
        }
        state.mode = MODE_GROUP_EDIT;
        state.openToolPanels = state.openToolPanels || {};
        pruneUnavailablePanels(state);
        closePanelGroup(state, MODE_GROUP_PREPARE);
        closePanelGroup(state, MODE_GROUP_SOURCE);
        panelIdsForGroup(state, MODE_GROUP_EDIT).forEach(function (id) {
            state.openToolPanels[id] = id === activeTool;
        });
        state.viewMode = normalizeEditViewMode(state.viewMode);
        return state.mode;
    }

    function normalizeEdit(state) {
        state.mode = MODE_GROUP_EDIT;
        state.openToolPanels = state.openToolPanels || {};
        pruneUnavailablePanels(state);
        closePanelGroup(state, MODE_GROUP_PREPARE);
        closePanelGroup(state, MODE_GROUP_SOURCE);
        state.viewMode = normalizeEditViewMode(state.viewMode);
        return state.mode;
    }

    function openSourcePanel(state) {
        return enterSource(state);
    }

    function canUseTool(state, id) {
        if (!hasImage(state)) {
            return isToolInGroup(id, MODE_GROUP_SOURCE);
        }
        if (state.mode === MODE_GROUP_PREPARE) {
            return isToolInGroup(id, MODE_GROUP_SOURCE)
                || isToolInGroup(id, MODE_GROUP_PREPARE)
                || isToolInGroup(id, MODE_GROUP_EDIT);
        }
        return true;
    }

    function canUseAction(state, id) {
        if (!hasImage(state)) {
            return false;
        }
        return state.mode === MODE_GROUP_EDIT || isToolInGroup(id, MODE_GROUP_SOURCE);
    }

    function canUseSource(state) {
        return panelIdsForGroup(state, MODE_GROUP_SOURCE).some(function (id) {
            return canUseTool(state, id);
        });
    }

    function sourceToolIds(state) {
        return panelIdsForGroup(state, MODE_GROUP_SOURCE);
    }

    function canUseSettingGroup(state, id) {
        if (!hasImage(state)) {
            return false;
        }
        if (!state.settings || !Object.prototype.hasOwnProperty.call(state.settings, id)) {
            return false;
        }
        if (state.mode === MODE_GROUP_PREPARE) {
            return isToolInGroup(id, MODE_GROUP_PREPARE);
        }
        return state.mode === MODE_GROUP_EDIT;
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.editorModeStateMachine = {
        groups: {
            SOURCE: MODE_GROUP_SOURCE,
            PREPARE: MODE_GROUP_PREPARE,
            EDIT: MODE_GROUP_EDIT,
            NONE: MODE_GROUP_NONE
        },
        normalize: normalize,
        enterSource: enterSource,
        enterPrepare: enterPrepare,
        enterEdit: enterEdit,
        openSourcePanel: openSourcePanel,
        openEditPanel: openEditPanel,
        isSourceTool: function isSourceTool(id) {
            return isToolInGroup(id, MODE_GROUP_SOURCE);
        },
        isPrepareTool: function isPrepareTool(id) {
            return isToolInGroup(id, MODE_GROUP_PREPARE);
        },
        isEditTool: function isEditTool(id) {
            return isToolInGroup(id, MODE_GROUP_EDIT);
        },
        canUseTool: canUseTool,
        canUseAction: canUseAction,
        canUseSource: canUseSource,
        sourceToolIds: sourceToolIds,
        canUseSettingGroup: canUseSettingGroup
    };
})(window.DitherApp);
