(function (app) {
    // Crop 純幾何模組：固定比例框、pan 邊界、旋轉外接矩形與 preview layout。
    // 全部是純函式、不碰 DOM/canvas/state；由 crop-feature 與 viewport 模組共用。
    var DEFAULT_ASPECT_RATIO_ID = '16-9';
    var MIN_ZOOM = 1;
    var MAX_ZOOM = 8;

    var ASPECT_RATIOS = [
        { id: '1-1', label: '1 : 1', width: 1, height: 1 },
        { id: '4-3', label: '4 : 3', width: 4, height: 3 },
        { id: '3-4', label: '3 : 4', width: 3, height: 4 },
        { id: '5-3', label: '5 : 3', width: 5, height: 3 },
        { id: '3-5', label: '3 : 5', width: 3, height: 5 },
        { id: '16-9', label: '16 : 9', width: 16, height: 9 },
        { id: '9-16', label: '9 : 16', width: 9, height: 16 }
    ];

    // 依 id 取得固定比例設定；找不到時回到預設 16:9。
    function ratioFor(id) {
        return ASPECT_RATIOS.find(function (ratio) {
            return ratio.id === id;
        }) || ASPECT_RATIOS.find(function (ratio) {
            return ratio.id === DEFAULT_ASPECT_RATIO_ID;
        });
    }

    // 將數值限制在指定範圍。
    function clamp(value, min, max) {
        return Math.max(min, Math.min(max, Number(value) || 0));
    }

    // 水平/垂直翻轉後，旋轉角度需要反向，讓視覺方向符合鏡像後的結果。
    function mirroredRotation(rotation) {
        return clamp(-Number(rotation || 0), -180, 180);
    }

    function steppedRotation(rotation, delta) {
        var next = Number(rotation || 0) + delta;
        while (next > 180) {
            next -= 360;
        }
        while (next < -180) {
            next += 360;
        }
        return next;
    }

    // 取得目前可用的影像尺寸；尚未載入圖時使用工作尺寸。
    function imageBounds(state) {
        var image = state.sourceImageData;
        return {
            width: image ? image.width : state.workingSize.width,
            height: image ? image.height : state.workingSize.height
        };
    }

    function frameForBounds(bounds, ratioId) {
        // 裁切框永遠以來源影像 bounds 內可容納的最大固定比例計算，並置中。
        var ratio = ratioFor(ratioId);
        var ratioValue = ratio.width / ratio.height;
        var width = bounds.width;
        var height = width / ratioValue;
        if (height > bounds.height) {
            height = bounds.height;
            width = height * ratioValue;
        }
        return {
            x: (bounds.width - width) / 2,
            y: (bounds.height - height) / 2,
            width: width,
            height: height,
            ratio: ratio
        };
    }

    // 根據 zoom 後的原圖尺寸計算 pan 可移動範圍。
    function maxPan(bounds, frame, zoom) {
        return {
            x: Math.max(0, (bounds.width * zoom - frame.width) / 2),
            y: Math.max(0, (bounds.height * zoom - frame.height) / 2)
        };
    }

    // 計算旋轉後原圖外接矩形，用來確保 preview canvas 足夠容納可視區。
    function transformedBounds(bounds, settings) {
        var angle = Number(settings.rotation || 0) * Math.PI / 180;
        // sin/cos 必須取絕對值；超過 90 度時 cos 會變負，否則外接範圍會被錯算成變小。
        var sin = Math.abs(Math.sin(angle));
        var cos = Math.abs(Math.cos(angle));
        return {
            width: bounds.width * cos + bounds.height * sin,
            height: bounds.width * sin + bounds.height * cos
        };
    }

    function previewLayout(bounds, settings) {
        // 預覽 canvas 需要能容納旋轉後的完整影像外框，也要保證裁切框尺寸不被旋轉擠壓。
        var frame = frameForBounds(bounds, settings.aspectRatioId);
        var transformed = transformedBounds(bounds, settings);
        var width = Math.max(frame.width, transformed.width);
        var height = Math.max(frame.height, transformed.height);
        width = Math.ceil(width);
        height = Math.ceil(height);

        return {
            width: width,
            height: height,
            frame: {
                x: (width - frame.width) / 2,
                y: (height - frame.height) / 2,
                width: frame.width,
                height: frame.height,
                ratio: frame.ratio
            }
        };
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.cropGeometry = {
        DEFAULT_ASPECT_RATIO_ID: DEFAULT_ASPECT_RATIO_ID,
        MIN_ZOOM: MIN_ZOOM,
        MAX_ZOOM: MAX_ZOOM,
        ratios: ASPECT_RATIOS.slice(),
        ratioFor: ratioFor,
        clamp: clamp,
        mirroredRotation: mirroredRotation,
        steppedRotation: steppedRotation,
        imageBounds: imageBounds,
        frameForBounds: frameForBounds,
        maxPan: maxPan,
        transformedBounds: transformedBounds,
        previewLayout: previewLayout
    };
})(window.DitherApp);
