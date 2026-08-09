(function (app) {
    // Expand（pixel）檢視的拖曳平移：結果溢出 preview stage 時，以 pointer 拖曳捲動。
    // 只操作 previewStage 的 scroll 與 CSS class，不碰 editor state。
    function createPixelPreviewDrag(previewStage) {
        var drag = null;

        function canDrag() {
            return Boolean(
                previewStage.classList.contains('is-pixel-preview')
                && (
                    previewStage.scrollWidth > previewStage.clientWidth
                    || previewStage.scrollHeight > previewStage.clientHeight
                )
            );
        }

        function onPointerDown(event) {
            if (event.button !== 0 || !canDrag()) {
                return;
            }
            drag = {
                pointerId: event.pointerId,
                x: event.clientX,
                y: event.clientY,
                scrollLeft: previewStage.scrollLeft,
                scrollTop: previewStage.scrollTop
            };
            previewStage.classList.add('is-pixel-dragging');
            if (previewStage.setPointerCapture) {
                previewStage.setPointerCapture(event.pointerId);
            }
            event.preventDefault();
        }

        function onPointerMove(event) {
            if (!drag || drag.pointerId !== event.pointerId) {
                return;
            }
            if (!canDrag()) {
                drag = null;
                previewStage.classList.remove('is-pixel-dragging');
                return;
            }
            previewStage.scrollLeft = drag.scrollLeft - (event.clientX - drag.x);
            previewStage.scrollTop = drag.scrollTop - (event.clientY - drag.y);
            event.preventDefault();
        }

        function onPointerEnd(event) {
            if (!drag || drag.pointerId !== event.pointerId) {
                return;
            }
            drag = null;
            previewStage.classList.remove('is-pixel-dragging');
        }

        var endEvents = ['pointerup', 'pointercancel', 'lostpointercapture'];
        previewStage.addEventListener('pointerdown', onPointerDown);
        previewStage.addEventListener('pointermove', onPointerMove);
        endEvents.forEach(function (name) {
            previewStage.addEventListener(name, onPointerEnd);
        });

        return {
            destroy: function destroy() {
                previewStage.removeEventListener('pointerdown', onPointerDown);
                previewStage.removeEventListener('pointermove', onPointerMove);
                endEvents.forEach(function (name) {
                    previewStage.removeEventListener(name, onPointerEnd);
                });
                drag = null;
            }
        };
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.createPixelPreviewDrag = createPixelPreviewDrag;
})(window.DitherApp);
