(function () {
    var loading = document.getElementById('app-loading');
    if (loading) { return loading.dataset.state === 'ready' || loading.dataset.state === 'error'; }
    var ids = ['result', 'probe-result', 'project-test-json', 'validation-json', 'render-json', 'benchmark-json'];
    for (var i = 0; i < ids.length; i++) {
        var node = document.getElementById(ids[i]);
        if (!node) { continue; }
        try {
            var result = JSON.parse(node.textContent);
            return Boolean(result && !result.pending && (result.passed || result.error || result.errors || result.rows || result.dataUrl));
        } catch (error) { return false; }
    }
    return false;
})()
