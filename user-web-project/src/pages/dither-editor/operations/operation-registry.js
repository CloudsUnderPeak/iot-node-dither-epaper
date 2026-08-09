(function (app) {
    // Operation registry 保存可被 pipeline 執行的圖片處理步驟。
    // feature 可以有 panel/action，但只有 register operation 後才會參與 pipeline。
    var operations = {};

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.operationRegistry = {
        // 註冊 pipeline operation；operation.id 會和 feature id 對齊。
        register: function register(operation) {
            operations[operation.id] = operation;
        },
        // 依 id 取得 operation 執行定義。
        get: function get(id) {
            return operations[id];
        },
        // 回傳所有 operation，主要給除錯或後續擴充使用。
        all: function all() {
            return Object.keys(operations).map(function (id) {
                return operations[id];
            });
        },
        // 只回傳可拖曳的 effects operation，固定前後置處理不會出現在這裡。
        pipelineEffects: function pipelineEffects() {
            // 只有標記 draggable 的 operation 會進入使用者可排序的 effects stack。
            return Object.keys(operations)
                .map(function (id) {
                    return operations[id];
                })
                .filter(function (operation) {
                    return operation.pipeline && operation.pipeline.draggable;
                });
        }
    };
})(window.DitherApp);
