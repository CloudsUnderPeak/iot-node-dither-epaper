(function (app) {
    // Classic script loader：讓各 page 的 entry.js 自己管理內部依賴。
    // Promise cache 可避免同一路徑被多個入口重複載入。
    var loadedScripts = {};

    app.app.pageEntryPromises = app.app.pageEntryPromises || [];

    // page entry 會把自己的載入 Promise 註冊進來，main.js 會等待全部完成。
    app.app.registerPageEntry = function registerPageEntry(promise) {
        app.app.pageEntryPromises.push(promise);
        return promise;
    };

    // 提供 app 啟動前的同步點，避免 router 啟動時頁面還沒註冊。
    app.app.whenPageEntriesReady = function whenPageEntriesReady() {
        return Promise.all(app.app.pageEntryPromises);
    };

    app.app.scriptLoader = {
        load: loadScript,
        // 同批 script 先全部插入，讓瀏覽器可並行下載；async=false 保持 classic script 執行順序。
        loadMany: function loadMany(paths) {
            return Promise.all(paths.map(function (path) {
                return loadScript(path);
            }));
        }
    };

    // 載入單支 script，並快取 Promise 避免重複插入相同檔案。
    function loadScript(path) {
        if (loadedScripts[path]) {
            return loadedScripts[path];
        }

        if (app.startupGate) {
            app.startupGate.registerResource();
        }

        // 使用 classic <script>，不使用 module / fetch template，
        // 是為了維持 file:// 直接開啟時也能運作。
        loadedScripts[path] = new Promise(function (resolve, reject) {
            var script = document.createElement('script');
            script.src = path;
            script.async = false;
            script.onload = function () {
                if (app.startupGate) {
                    app.startupGate.settleResource();
                }
                resolve();
            };
            script.onerror = function () {
                reject(new Error('Failed to load script: ' + path));
            };
            document.body.appendChild(script);
        });

        return loadedScripts[path];
    }
})(window.DitherApp);
