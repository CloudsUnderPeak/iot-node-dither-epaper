(function (app) {
    // Project capabilities 是 Help 與實際功能設定之間的唯讀資料橋接層。
    // 生產者以 replaceCollection / setFact 更新，消費者不可直接持有內部陣列。
    var collections = {};
    var facts = {};

    app.app.projectCapabilities = {
        replaceCollection: function replaceCollection(id, items) {
            collections[id] = (items || []).map(function (item) {
                return Object.assign({}, item);
            });
        },
        list: function list(id) {
            return (collections[id] || []).map(function (item) {
                return Object.assign({}, item);
            });
        },
        setFact: function setFact(id, value) {
            facts[id] = value;
        },
        fact: function fact(id) {
            return facts[id];
        }
    };
})(window.DitherApp);
