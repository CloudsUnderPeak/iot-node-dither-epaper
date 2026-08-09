(function (app) {
    // 輕量排序元件，用 pointer events 實作，避免再依賴 SortableJS CDN。
    // 它只回傳排序後的 data-id 陣列，實際狀態更新由呼叫端處理。
    var DRAG_THRESHOLD = 5;

    app.ui.sortableList = {
        mount: function mount(listNode, options) {
            // handle 決定哪些區域可以啟動拖曳；immediateHandle 可讓 drag handle 立即開始拖。
            // 其他 tool row 區域則需長按，避免和「點擊展開面板」互相干擾。
            var onChange = options.onChange || function () {};
            var draggableSelector = options.draggable || '[draggable="true"]';
            var handleSelector = options.handle;
            var immediateHandleSelector = options.immediateHandle;
            var holdDelay = options.holdDelay || 260;
            var activeItem = null;
            var activeHandle = null;
            var activePointerId = null;
            var activeImmediate = false;
            var draggedItem = null;
            var orderChanged = false;
            var startX = 0;
            var startY = 0;
            var suppressClick = false;
            var holdTimer = null;
            var cleanup = [];

            // 綁定事件並記錄解除函式，destroy 時可完整清理。
            function listen(node, eventName, handler, listenerOptions) {
                node.addEventListener(eventName, handler, listenerOptions);
                cleanup.push(function () {
                    node.removeEventListener(eventName, handler, listenerOptions);
                });
            }

            listNode.querySelectorAll(draggableSelector).forEach(function (item) {
                var handles = handleSelector ? item.querySelectorAll(handleSelector) : [item];
                Array.prototype.forEach.call(handles, function (handle) {
                    handle.draggable = false;
                    listen(handle, 'pointerdown', function (event) {
                        if (event.button !== 0) {
                            return;
                        }
                        clearHoldTimer();
                        activeItem = item;
                        activeHandle = handle;
                        activePointerId = event.pointerId;
                        activeImmediate = immediateHandleSelector
                            ? Boolean(closestWithin(event.target, handle, immediateHandleSelector))
                            : true;
                        startX = event.clientX;
                        startY = event.clientY;
                        orderChanged = false;
                        if (handle.setPointerCapture) {
                            handle.setPointerCapture(event.pointerId);
                        }
                        if (!activeImmediate) {
                            holdTimer = window.setTimeout(function () {
                                if (activeItem && !draggedItem) {
                                    startDrag();
                                }
                            }, holdDelay);
                        }
                    });
                    listen(handle, 'click', function (event) {
                        if (!suppressClick) {
                            return;
                        }
                        event.preventDefault();
                        event.stopImmediatePropagation();
                        suppressClick = false;
                    }, true);
                });
            });

            listen(window, 'pointermove', function (event) {
                if (activePointerId !== event.pointerId || !activeItem) {
                    return;
                }
                if (!draggedItem && (!activeImmediate || !hasPassedThreshold(event))) {
                    return;
                }
                if (!draggedItem) {
                    startDrag();
                }
                event.preventDefault();
                if (moveItem(
                    listNode,
                    draggedItem,
                    referenceFromPoint(listNode, draggableSelector, handleSelector, draggedItem, event.clientY)
                )) {
                    orderChanged = true;
                }
            });

            listen(window, 'pointerup', function (event) {
                if (activePointerId === event.pointerId) {
                    finishDrag();
                }
            });

            listen(window, 'pointercancel', function (event) {
                if (activePointerId === event.pointerId) {
                    finishDrag();
                }
            });

            return {
                // 移除事件與動畫樣式，避免頁面切換後留下狀態。
                destroy: function destroy() {
                    finishDrag();
                    cleanupAnimationStyles(listNode);
                    cleanup.forEach(function (removeListener) {
                        removeListener();
                    });
                }
            };

            // 滑鼠移動超過門檻才啟動拖曳，避免誤觸。
            function hasPassedThreshold(event) {
                return Math.abs(event.clientX - startX) + Math.abs(event.clientY - startY) >= DRAG_THRESHOLD;
            }

            function startDrag() {
                // 拖曳時不建立 ghost 元素，而是直接移動 DOM 排序，讓使用者即時看到位置變化。
                clearHoldTimer();
                draggedItem = activeItem;
                draggedItem.classList.add('is-dragging');
                document.body.classList.add('is-sorting');
            }

            // 完成拖曳後通知新順序，並短暫抑制 click 事件。
            function finishDrag() {
                clearHoldTimer();
                if (draggedItem) {
                    draggedItem.classList.remove('is-dragging');
                    document.body.classList.remove('is-sorting');
                    if (orderChanged) {
                        onChange(readOrder(listNode));
                    }
                    suppressClick = true;
                    window.setTimeout(function () {
                        suppressClick = false;
                    }, 120);
                }
                if (activeHandle && activeHandle.releasePointerCapture && activePointerId !== null) {
                    try {
                        activeHandle.releasePointerCapture(activePointerId);
                    } catch (error) {}
                }
                activeItem = null;
                activeHandle = null;
                activePointerId = null;
                activeImmediate = false;
                draggedItem = null;
                orderChanged = false;
            }

            // 清掉長按啟動拖曳的 timer。
            function clearHoldTimer() {
                if (holdTimer) {
                    window.clearTimeout(holdTimer);
                    holdTimer = null;
                }
            }
        }
    };

    // 檢查 pointerdown 目標是否在指定 handle 內。
    function closestWithin(node, root, selector) {
        while (node && node !== root) {
            if (node.matches && node.matches(selector)) {
                return node;
            }
            node = node.parentNode;
        }
        return root && root.matches && root.matches(selector) ? root : null;
    }

    // 依目前 Y 座標找出 draggedItem 應插入到哪個項目前面。
    function referenceFromPoint(listNode, selector, handleSelector, draggedItem, clientY) {
        var items = Array.prototype.slice.call(listNode.querySelectorAll(selector));
        for (var i = 0; i < items.length; i += 1) {
            if (items[i] === draggedItem) {
                continue;
            }
            var rect = sortTargetRect(items[i], handleSelector);
            if (clientY < rect.top + rect.height / 2) {
                return items[i];
            }
        }
        return null;
    }

    // 拖曳排序以 handle 的矩形為判斷範圍；沒有 handle 時用整個 item。
    function sortTargetRect(item, handleSelector) {
        var handle = handleSelector ? item.querySelector(handleSelector) : null;
        return (handle || item).getBoundingClientRect();
    }

    function moveItem(listNode, draggedItem, reference) {
        // FLIP 動畫：先量測舊位置，再移動 DOM，最後用 transform 補間到新位置。
        if (reference === draggedItem || draggedItem.nextSibling === reference) {
            return false;
        }
        var positions = measureItems(listNode);
        listNode.insertBefore(draggedItem, reference);
        animateItems(positions, draggedItem);
        return true;
    }

    // 記錄所有項目的舊位置，給 FLIP 動畫計算位移。
    function measureItems(listNode) {
        return Array.prototype.slice.call(listNode.querySelectorAll('[data-id]')).map(function (node) {
            return {
                node: node,
                rect: node.getBoundingClientRect()
            };
        });
    }

    // 使用 transform 做短動畫，讓排序變化可見但不建立拖曳殘影。
    function animateItems(positions, draggedItem) {
        positions.forEach(function (entry) {
            if (entry.node === draggedItem) {
                return;
            }
            var rect = entry.node.getBoundingClientRect();
            var dx = entry.rect.left - rect.left;
            var dy = entry.rect.top - rect.top;
            if (!dx && !dy) {
                return;
            }
            window.clearTimeout(entry.node.__sortableAnimationTimer);
            if (entry.node.__sortableAnimationFrame) {
                window.cancelAnimationFrame(entry.node.__sortableAnimationFrame);
            }
            entry.node.style.transition = 'none';
            entry.node.style.transform = 'translate(' + dx + 'px, ' + dy + 'px)';
            entry.node.style.willChange = 'transform';
            entry.node.__sortableAnimationFrame = window.requestAnimationFrame(function () {
                entry.node.__sortableAnimationFrame = null;
                entry.node.style.transition = 'transform 105ms ease-out';
                entry.node.style.transform = '';
                entry.node.__sortableAnimationTimer = window.setTimeout(function () {
                    entry.node.style.transition = '';
                    entry.node.style.transform = '';
                    entry.node.style.willChange = '';
                }, 120);
            });
        });
    }

    // 清掉排序期間留下的 transition/transform/will-change。
    function cleanupAnimationStyles(listNode) {
        Array.prototype.slice.call(listNode.querySelectorAll('[data-id]')).forEach(function (node) {
            window.clearTimeout(node.__sortableAnimationTimer);
            if (node.__sortableAnimationFrame) {
                window.cancelAnimationFrame(node.__sortableAnimationFrame);
            }
            node.__sortableAnimationFrame = null;
            node.style.transition = '';
            node.style.transform = '';
            node.style.willChange = '';
        });
        document.body.classList.remove('is-sorting');
    }

    // 從 DOM 目前順序讀回 data-id 陣列。
    function readOrder(listNode) {
        return Array.prototype.slice.call(listNode.querySelectorAll('[data-id]')).map(function (node) {
            return node.dataset.id;
        });
    }
})(window.DitherApp);
