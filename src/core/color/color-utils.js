(function (app) {
    // 色彩通用工具：只處理數值轉換，不知道 palette preset 或 Dither algorithm。
    app.core.colorUtils = {
        // 將任意數值限制成 0-255 的整數色彩通道。
        clampByte: function clampByte(value) {
            return Math.min(255, Math.max(0, Math.round(value)));
        },
        // 將數值限制在 0-255 但保留小數；給誤差擴散工作緩衝與距離計算使用，
        // 不可與 clampByte 混用：round 時機不同會改變輸出。
        clampChannel: function clampChannel(value) {
            return value < 0 ? 0 : (value > 255 ? 255 : value);
        },
        // 依 Rec. 709 權重計算亮度，給圖案抖色等灰階判斷使用。
        luminance: function luminance(r, g, b) {
            return 0.2126 * r + 0.7152 * g + 0.0722 * b;
        },
        // 將 #rrggbb 轉成 RGB 物件；非法輸入回黑色，給色票 input 使用。
        hexToRgb: function hexToRgb(hex) {
            var match = /^#?([0-9a-f]{6})$/i.exec(String(hex || ''));
            var normalized = match ? match[1] : '000000';
            return {
                r: parseInt(normalized.slice(0, 2), 16),
                g: parseInt(normalized.slice(2, 4), 16),
                b: parseInt(normalized.slice(4, 6), 16)
            };
        },
        // 將 RGB 物件轉成 #rrggbb 字串；通道經 clampByte round 到整數。
        rgbToHex: function rgbToHex(color) {
            var clampByte = app.core.colorUtils.clampByte;
            function part(value) {
                return clampByte(Number(value) || 0).toString(16).padStart(2, '0');
            }
            return '#' + part(color && color.r) + part(color && color.g) + part(color && color.b);
        }
    };
})(window.DitherApp);
