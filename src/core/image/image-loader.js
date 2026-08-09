(function (app) {
    // 圖片載入入口：所有使用者圖片都必須先通過格式 gate，再轉成 origin-clean ImageData。
    // 只允許 PNG / JPEG / WebP，是為了讓後續 canvas 演算法流程穩定可預期。
    var DEMO_IMAGE_MANIFEST_SCRIPT = 'assets/demo/demo-manifest.js';
    var DEMO_IMAGE_DATA_SCRIPT = 'assets/demo/demo-data.js';
    var ACCEPTED_IMAGE_TYPES = 'image/png,image/jpeg,image/webp';
    var SUPPORTED_IMAGE_TYPES = {
        'image/png': true,
        'image/jpeg': true,
        'image/webp': true
    };
    var SUPPORTED_IMAGE_EXTENSIONS = ['.png', '.jpg', '.jpeg', '.webp'];

    // core 不碰 i18n：錯誤帶 code 讓頁面層對應翻譯，message 僅作 fallback。
    function loaderError(code, message) {
        var error = new Error(message);
        error.code = code;
        return error;
    }

    // 以 HTMLImageElement 載入 URL，保留給本機 demo 或 blob URL fallback 使用。
    function loadImageFromUrl(url) {
        return new Promise(function (resolve, reject) {
            var image = new Image();
            image.onload = function () {
                resolve(image);
            };
            image.onerror = function () {
                reject(loaderError('image-load-failed', 'Image could not be loaded.'));
            };
            image.src = url;
        });
    }

    // 檢查匯入檔案是否為允許的 PNG/JPEG/WebP。
    function isSupportedImageFile(file) {
        var type = String(file && file.type || '').toLowerCase();
        var name = String(file && file.name || '').toLowerCase();
        if (SUPPORTED_IMAGE_TYPES[type]) {
            return true;
        }
        return SUPPORTED_IMAGE_EXTENSIONS.some(function (extension) {
            return name.endsWith(extension);
        });
    }

    // 統一產生格式錯誤，讓 controller 顯示一致訊息。
    function rejectUnsupportedImage(file) {
        if (!isSupportedImageFile(file)) {
            throw loaderError('unsupported-format', 'Only PNG, JPEG, and WebP images are supported.');
        }
    }

    // 將 createImageBitmap 結果轉為 ImageData，並負責關閉 bitmap 資源。
    function imageBitmapToImageData(bitmap, maxLongEdge) {
        try {
            return app.core.canvasUtils.imageToImageData(bitmap, maxLongEdge);
        } finally {
            if (bitmap.close) {
                bitmap.close();
            }
        }
    }

    // createImageBitmap 不可用時，改用 blob URL + ImageElement fallback。
    function blobUrlToImageData(blob, maxLongEdge) {
        var url = URL.createObjectURL(blob);
        return loadImageFromUrl(url)
            .then(function (image) {
                return app.core.canvasUtils.imageToImageData(image, maxLongEdge);
            })
            .finally(function () {
                URL.revokeObjectURL(url);
            });
    }

    // 優先使用 createImageBitmap，因為它通常不會造成 file:// demo 的 canvas taint。
    function blobToImageData(blob, maxLongEdge) {
        if (window.createImageBitmap) {
            // createImageBitmap 先由瀏覽器解碼 Blob，通常比直接 <img src=objectURL> 更不容易遇到 canvas taint。
            return createImageBitmap(blob)
                .then(function (bitmap) {
                    return imageBitmapToImageData(bitmap, maxLongEdge);
                })
                .catch(function () {
                    return blobUrlToImageData(blob, maxLongEdge);
                });
        }
        return blobUrlToImageData(blob, maxLongEdge);
    }

    function imageUrlToImageData(url, maxLongEdge) {
        return loadImageFromUrl(url)
            .then(function (image) {
                return app.core.canvasUtils.imageToImageData(image, maxLongEdge);
            });
    }

    function loadDataUrlImage(dataUrl, maxLongEdge) {
        if (!window.fetch) {
            return imageUrlToImageData(dataUrl, maxLongEdge);
        }
        return fetch(dataUrl)
            .then(function (response) {
                return response.blob();
            })
            .then(function (blob) {
                return blobToImageData(blob, maxLongEdge);
            })
            .catch(function () {
                return imageUrlToImageData(dataUrl, maxLongEdge);
            });
    }

    // 從使用者選取或拖放的 File 載入 ImageData。
    function loadImageFromFile(file, maxLongEdge) {
        try {
            // dropzone 會繞過 <input accept>，所以 core loader 必須再次驗證格式。
            rejectUnsupportedImage(file);
        } catch (error) {
            return Promise.reject(error);
        }
        return blobToImageData(file, maxLongEdge);
    }

    function loadDemoManifest() {
        if (!app.app || !app.app.scriptLoader || !app.app.scriptLoader.load) {
            return Promise.reject(loaderError('demo-load-failed', 'Demo image could not be loaded.'));
        }
        return app.app.scriptLoader.load(DEMO_IMAGE_MANIFEST_SCRIPT)
            .then(function () {
                var demoImage = app.assets && app.assets.demoImage;
                if (!demoImage || !demoImage.url || !demoImage.fileName) {
                    throw loaderError('demo-manifest-missing', 'Demo image manifest is missing.');
                }
                return demoImage;
            });
    }

    function loadGeneratedDemoImage(demoImage, maxLongEdge) {
        var dataScript = demoImage.dataScript || DEMO_IMAGE_DATA_SCRIPT;
        if (!app.app || !app.app.scriptLoader || !app.app.scriptLoader.load) {
            return Promise.reject(loaderError('demo-load-failed', 'Demo image could not be loaded.'));
        }
        return app.app.scriptLoader.load(dataScript)
            .then(function () {
                var embeddedDemo = app.assets && app.assets.demoImageData;
                if (!embeddedDemo || !embeddedDemo.dataUrl) {
                    throw loaderError('demo-data-missing', 'Demo image data asset is missing.');
                }
                return loadDataUrlImage(embeddedDemo.dataUrl, maxLongEdge);
            });
    }

    function loadDemoImageFile(demoImage, maxLongEdge) {
        // Server/GitHub Pages 情境直接讀 source image；file:// 若被擋，外層會 fallback 到 data asset。
        if (!window.fetch) {
            return imageUrlToImageData(demoImage.url, maxLongEdge);
        }
        return fetch(demoImage.url)
            .then(function (response) {
                if (!response.ok) {
                    throw loaderError('demo-load-failed', 'Demo image could not be loaded.');
                }
                return response.blob();
            })
            .then(function (blob) {
                return blobToImageData(blob, maxLongEdge);
            })
            .catch(function () {
                return imageUrlToImageData(demoImage.url, maxLongEdge);
            });
    }

    // 載入專案內建 demo 圖。
    function loadDemoImage(maxLongEdge) {
        return loadDemoManifest()
            .then(function (demoImage) {
                return loadDemoImageFile(demoImage, maxLongEdge)
                    .catch(function () {
                        return loadGeneratedDemoImage(demoImage, maxLongEdge);
                    })
                    .then(function (result) {
                        result.fileName = demoImage.fileName;
                        return result;
                    });
            });
    }

    // 建立白底空白 ImageData；目前 UI 不直接暴露 blank canvas 建立入口。
    function createBlankImage(width, height) {
        var canvas = app.core.canvasUtils.createCanvas(width, height);
        var ctx = canvas.getContext('2d', { willReadFrequently: true });
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        return {
            imageData: ctx.getImageData(0, 0, canvas.width, canvas.height),
            originalSize: { width: canvas.width, height: canvas.height },
            workingSize: { width: canvas.width, height: canvas.height },
            wasResized: false
        };
    }

    app.core.imageLoader = {
        acceptedImageTypes: ACCEPTED_IMAGE_TYPES,
        isSupportedImageFile: isSupportedImageFile,
        loadImageFromFile: loadImageFromFile,
        loadDemoImage: loadDemoImage,
        createBlankImage: createBlankImage
    };
})(window.DitherApp);
