(function (app) {
    // CropPointerMapper 將 crop overlay 的 pointer/wheel 操作轉成 crop pan/zoom 設定。
    // 它只做座標換算與事件生命週期，不負責重繪或 pipeline。
    function CropPointerMapper(options) {
        options = options || {};
        this.cropOverlay = options.cropOverlay;
        this.canvas = options.canvas;
        this.controller = options.controller;
        this.prepareMode = options.prepareMode;
        this.activePointers = {};
        this.drag = null;
        this.pinch = null;

        this.handlePointerDown = this.handlePointerDown.bind(this);
        this.handlePointerMove = this.handlePointerMove.bind(this);
        this.handlePointerEnd = this.handlePointerEnd.bind(this);
        this.handleWheel = this.handleWheel.bind(this);

        this.bind();
    }

    CropPointerMapper.prototype.bind = function bind() {
        if (!this.cropOverlay) {
            return;
        }
        this.cropOverlay.addEventListener('pointerdown', this.handlePointerDown);
        this.cropOverlay.addEventListener('pointermove', this.handlePointerMove);
        this.cropOverlay.addEventListener('pointerup', this.handlePointerEnd);
        this.cropOverlay.addEventListener('pointercancel', this.handlePointerEnd);
        this.cropOverlay.addEventListener('wheel', this.handleWheel, { passive: false });
    };

    CropPointerMapper.prototype.destroy = function destroy() {
        if (!this.cropOverlay) {
            return;
        }
        this.cropOverlay.removeEventListener('pointerdown', this.handlePointerDown);
        this.cropOverlay.removeEventListener('pointermove', this.handlePointerMove);
        this.cropOverlay.removeEventListener('pointerup', this.handlePointerEnd);
        this.cropOverlay.removeEventListener('pointercancel', this.handlePointerEnd);
        this.cropOverlay.removeEventListener('wheel', this.handleWheel);
        this.activePointers = {};
        this.drag = null;
        this.pinch = null;
    };

    CropPointerMapper.prototype.isPrepareMode = function isPrepareMode() {
        return Boolean(
            this.controller
            && this.controller.state
            && this.controller.state.sourceImageData
            && this.controller.state.mode === this.prepareMode
        );
    };

    CropPointerMapper.prototype.canvasScale = function canvasScale() {
        var rect = this.canvas.getBoundingClientRect();
        return {
            x: this.canvas.width / rect.width,
            y: this.canvas.height / rect.height
        };
    };

    CropPointerMapper.prototype.pointerList = function pointerList() {
        var pointers = this.activePointers;
        return Object.keys(pointers).map(function (id) {
            return pointers[id];
        });
    };

    CropPointerMapper.prototype.pointerCount = function pointerCount() {
        return Object.keys(this.activePointers).length;
    };

    CropPointerMapper.prototype.storePointer = function storePointer(event) {
        var id = String(event.pointerId);
        this.activePointers[id] = {
            id: id,
            pointerId: event.pointerId,
            clientX: event.clientX,
            clientY: event.clientY
        };
        return this.activePointers[id];
    };

    CropPointerMapper.prototype.releasePointer = function releasePointer(event) {
        if (
            this.cropOverlay.releasePointerCapture
            && (!this.cropOverlay.hasPointerCapture || this.cropOverlay.hasPointerCapture(event.pointerId))
        ) {
            this.cropOverlay.releasePointerCapture(event.pointerId);
        }
    };

    CropPointerMapper.prototype.beginDrag = function beginDrag(pointer) {
        this.drag = {
            pointerId: pointer.id,
            startX: pointer.clientX,
            startY: pointer.clientY,
            crop: Object.assign({}, this.controller.state.settings.crop),
            scale: this.canvasScale()
        };
    };

    CropPointerMapper.prototype.beginPinch = function beginPinch() {
        var pointers = this.pointerList();
        var distance = pointers.length >= 2 ? pointerDistance(pointers[0], pointers[1]) : 0;
        this.drag = null;
        this.pinch = distance > 0
            ? {
                pointerIds: [pointers[0].id, pointers[1].id],
                startDistance: distance,
                crop: Object.assign({}, this.controller.state.settings.crop)
            }
            : null;
    };

    CropPointerMapper.prototype.updatePinchZoom = function updatePinchZoom() {
        if (!this.pinch) {
            return;
        }
        var first = this.activePointers[this.pinch.pointerIds[0]];
        var second = this.activePointers[this.pinch.pointerIds[1]];
        var distance = first && second ? pointerDistance(first, second) : 0;
        if (distance <= 0) {
            return;
        }
        this.controller.updateSetting(
            'crop',
            'zoom',
            Number(this.pinch.crop.zoom || 1) * distance / this.pinch.startDistance
        );
    };

    CropPointerMapper.prototype.resetInteraction = function resetInteraction() {
        var overlay = this.cropOverlay;
        Object.keys(this.activePointers).forEach(function (id) {
            var pointerId = this.activePointers[id].pointerId;
            if (
                overlay.releasePointerCapture
                && (!overlay.hasPointerCapture || overlay.hasPointerCapture(pointerId))
            ) {
                overlay.releasePointerCapture(pointerId);
            }
        }, this);
        this.activePointers = {};
        this.drag = null;
        this.pinch = null;
    };

    CropPointerMapper.prototype.handlePointerDown = function handlePointerDown(event) {
        if (!this.isPrepareMode()) {
            return;
        }
        event.preventDefault();
        var pointer = this.storePointer(event);
        this.cropOverlay.setPointerCapture(event.pointerId);
        if (this.pointerCount() > 1) {
            this.beginPinch();
            return;
        }
        // 拖曳 overlay 時移動的是原圖 pan，不是裁切框本身。
        this.beginDrag(pointer);
    };

    CropPointerMapper.prototype.handlePointerMove = function handlePointerMove(event) {
        var pointerId = String(event.pointerId);
        if (!this.activePointers[pointerId]) {
            return;
        }
        if (!this.isPrepareMode()) {
            this.resetInteraction();
            return;
        }
        event.preventDefault();
        this.storePointer(event);
        if (this.pinch) {
            this.updatePinchZoom();
            return;
        }
        if (!this.drag || pointerId !== this.drag.pointerId) {
            return;
        }
        this.controller.updateSettings('crop', {
            panX: Number(this.drag.crop.panX || 0) + (event.clientX - this.drag.startX) * this.drag.scale.x,
            panY: Number(this.drag.crop.panY || 0) + (event.clientY - this.drag.startY) * this.drag.scale.y
        });
    };

    CropPointerMapper.prototype.handlePointerEnd = function handlePointerEnd(event) {
        var pointerId = String(event.pointerId);
        if (!this.activePointers[pointerId]) {
            return;
        }
        this.releasePointer(event);
        delete this.activePointers[pointerId];
        if (this.pinch) {
            if (this.pointerCount() > 1) {
                this.beginPinch();
            } else {
                this.pinch = null;
                this.drag = null;
                if (this.pointerCount() === 1) {
                    this.beginDrag(this.pointerList()[0]);
                }
            }
            return;
        }
        if (this.drag && pointerId === this.drag.pointerId) {
            this.drag = null;
        }
    };

    CropPointerMapper.prototype.handleWheel = function handleWheel(event) {
        if (!this.isPrepareMode()) {
            return;
        }
        event.preventDefault();
        this.controller.updateSetting(
            'crop',
            'zoom',
            (this.controller.state.settings.crop.zoom || 1) + (event.deltaY < 0 ? 0.02 : -0.02)
        );
    };

    function pointerDistance(a, b) {
        var dx = a.clientX - b.clientX;
        var dy = a.clientY - b.clientY;
        return Math.sqrt(dx * dx + dy * dy);
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.CropPointerMapper = CropPointerMapper;
})(window.DitherApp);
