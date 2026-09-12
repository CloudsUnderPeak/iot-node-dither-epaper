var harness = (function () {
    function deferred() {
        var resolve, reject;
        var promise = new Promise(function (yes, no) { resolve = yes; reject = no; });
        return { promise: promise, resolve: resolve, reject: reject };
    }
    function execute(path, bindings) {
        var keys = Object.keys(bindings);
        Function.apply(null, keys.concat(testSources[path])).apply(null, keys.map(function (key) { return bindings[key]; }));
    }
    function assert(value, message) {
        if (!value) { throw new Error(message); }
    }
    function FakeWorker() {
        this.messages = [];
        this.terminated = false;
        FakeWorker.instances.push(this);
    }
    FakeWorker.instances = [];
    FakeWorker.prototype.postMessage = function (message, transfer) { this.messages.push({ message: message, transfer: transfer }); };
    FakeWorker.prototype.terminate = function () { this.terminated = true; };
    function controlledFetch() {
        var requests = [];
        function fetch(url, options) {
            var request = deferred();
            requests.push({ url: url, options: options, response: request });
            return request.promise;
        }
        fetch.requests = requests;
        return fetch;
    }
    return { deferred: deferred, execute: execute, assert: assert, FakeWorker: FakeWorker, controlledFetch: controlledFetch };
})();
