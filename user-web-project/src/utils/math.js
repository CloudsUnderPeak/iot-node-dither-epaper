(function (app) {
    // 數學工具維持純函式；演算法、UI slider、設定 normalize 都可以共用。
    // 不在這裡讀 DOM 或 editor state，避免 utility 反向依賴頁面。
    app.utils.math = {
        // 將數值限制在 min/max 範圍內。
        clamp: function clamp(value, min, max) {
            return Math.min(max, Math.max(min, value));
        },
        // 將數值四捨五入，主要給 UI 顯示與尺寸計算使用。
        round: function round(value) {
            return Math.round(Number(value) || 0);
        }
    };
})(window.DitherApp);
