(function (app) {
    // Panel utilities 是 Dither Editor 面板共用的 UI 工廠。
    // 這層只負責建立 DOM 控制元件與綁定 callback，不直接讀寫 editor state。
    // Dither Editor panel 專用的小型 UI factory。
    // 它只建立表單元件與 callback，不直接修改 editor state。
    // 取得面板顯示文字。
    function t(key) {
        return app.i18n.t(key);
    }

    function svgIcon(icon, options) {
        return app.ui.svgIcons.create(icon, options);
    }

    // 建立一個工具面板 section，並可加上 data-tool-panel 方便 page 掛載。
    function section(titleKey, children, toolId) {
        return app.utils.dom.el('section', {
            className: 'panel-section',
            attrs: toolId ? { 'data-tool-panel': toolId } : {},
            children: [
                app.utils.dom.el('h2', { text: t(titleKey) }),
                app.utils.dom.el('div', { className: 'panel-body', children: children })
            ]
        });
    }

    // 建立標籤 + 控制元件的標準列。
    function row(label, input) {
        return app.utils.dom.el('div', {
            className: 'control-row',
            children: [app.utils.dom.el('label', { text: label }), input]
        });
    }

    // 建立一般數字輸入，數值變更時回傳 Number。
    function numberInput(value, min, max, step, onChange) {
        var input = app.utils.dom.el('input', {
            attrs: { type: 'number', value: value, min: min, max: max, step: step || 1 }
        });
        input.addEventListener('input', function () {
            onChange(Number(input.value));
        });
        return input;
    }

    function toggleSwitchInput(value, onChange) {
        var input = app.utils.dom.el('input', {
            attrs: { type: 'checkbox', role: 'switch' }
        });
        input.checked = Boolean(value);
        input.addEventListener('change', function () {
            onChange(input.checked);
        });
        return app.utils.dom.el('label', {
            className: 'toggle-switch',
            children: [
                input,
                app.utils.dom.el('span', { className: 'toggle-switch-track' })
            ]
        });
    }

    function unitNumberInput(value, min, max, step, unit, onChange) {
        // 自製數字輸入是為了在數值旁顯示單位，並讓上下箭頭支援長按連續調整。
        // 原生 number input 在不同瀏覽器的 spinner 樣式和長按行為不一致。
        var currentValue = Number(value);
        step = step || 1;
        if (!Number.isFinite(currentValue)) {
            currentValue = min;
        }

        // 將輸入值轉成有效範圍內的數字。
        function clampValue(nextValue) {
            var numericValue = Number(nextValue);
            if (!Number.isFinite(numericValue)) {
                return currentValue;
            }
            return Math.max(min, Math.min(max, numericValue));
        }

        // 更新內部值；使用者正在輸入時避免覆蓋游標狀態。
        function setValue(nextValue, force) {
            currentValue = clampValue(nextValue);
            if (force || document.activeElement !== input) {
                input.value = currentValue;
            }
        }

        function setRange(nextMin, nextMax) {
            min = Number(nextMin);
            max = Number(nextMax);
            input.min = min;
            input.max = max;
            setValue(currentValue);
        }

        // 確認新值並通知呼叫端。
        function commit(nextValue) {
            setValue(nextValue, true);
            onChange(currentValue);
        }

        // 依 step 往上或往下調整。
        function changeBy(direction) {
            commit(clampValue((Number(input.value) || currentValue) + step * direction));
        }

        // 綁定上下箭頭按鈕與長按連續調整。
        function bindStep(button, direction) {
            var repeatDelay = null;
            var repeatInterval = null;

            // 停止長按重複觸發。
            function stopRepeat() {
                window.clearTimeout(repeatDelay);
                window.clearInterval(repeatInterval);
                window.removeEventListener('pointerup', stopRepeat);
                window.removeEventListener('pointercancel', stopRepeat);
            }

            button.addEventListener('pointerdown', function (event) {
                event.preventDefault();
                input.focus();
                changeBy(direction);
                stopRepeat();
                // 長按上下按鈕時連續調整，讓 zoom/rotation 這類數值輸入有接近原生 stepper 的手感。
                repeatDelay = window.setTimeout(function () {
                    repeatInterval = window.setInterval(function () {
                        changeBy(direction);
                    }, 70);
                }, 260);
                window.addEventListener('pointerup', stopRepeat);
                window.addEventListener('pointercancel', stopRepeat);
            });
        }

        var input = app.utils.dom.el('input', {
            className: 'unit-number-input',
            attrs: { type: 'number', value: currentValue, min: min, max: max, step: step }
        });
        var increase = app.utils.dom.el('button', {
            className: 'unit-number-step is-up',
            attrs: { type: 'button', 'aria-label': 'Increase value' }
        });
        var decrease = app.utils.dom.el('button', {
            className: 'unit-number-step is-down',
            attrs: { type: 'button', 'aria-label': 'Decrease value' }
        });
        var wrapper = app.utils.dom.el('div', {
            className: 'unit-number-field',
            children: [
                input,
                app.utils.dom.el('span', { className: 'unit-number-suffix', text: unit }),
                app.utils.dom.el('div', { className: 'unit-number-stepper', children: [increase, decrease] })
            ]
        });

        input.addEventListener('input', function () {
            if (input.value === '') {
                return;
            }
            currentValue = clampValue(input.value);
            onChange(currentValue);
        });
        input.addEventListener('blur', function () {
            setValue(currentValue, true);
        });
        input.addEventListener('keydown', function (event) {
            if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
                event.preventDefault();
                changeBy(event.key === 'ArrowUp' ? 1 : -1);
            }
        });
        bindStep(increase, 1);
        bindStep(decrease, -1);

        wrapper.setValue = setValue;
        wrapper.setRange = setRange;
        return wrapper;
    }

    function rangeInput(value, min, max, step, onChange, options) {
        // Range input 可以通知 controller「使用者正在拖曳」。
        // Adjust 會利用這個訊號先走即時視覺預覽，放開後才補正式 pipeline。
        options = options || {};
        var input = app.utils.dom.el('input', {
            attrs: { type: 'range', value: value, min: min, max: max, step: step || 1 }
        });
        var holding = false;

        function updateRangeProgress() {
            var range = Number(max) - Number(min);
            var progress = range > 0
                ? (Number(input.value) - Number(min)) / range * 100
                : 0;
            input.style.setProperty('--range-progress', Math.max(0, Math.min(100, progress)) + '%');
        }

        // 滑桿互動結束時通知 controller 可以跑正式 preview。
        function endInteraction() {
            if (!holding) {
                return;
            }
            holding = false;
            window.removeEventListener('pointerup', endInteraction);
            window.removeEventListener('pointercancel', endInteraction);
            if (options.onInteractionEnd) {
                // slider 拖曳結束時讓 controller 補跑正式 preview。
                options.onInteractionEnd();
            }
        }
        input.addEventListener('pointerdown', function () {
            if (holding) {
                return;
            }
            holding = true;
            if (options.onInteractionStart) {
                options.onInteractionStart();
            }
            window.addEventListener('pointerup', endInteraction);
            window.addEventListener('pointercancel', endInteraction);
        });
        input.addEventListener('input', function () {
            updateRangeProgress();
            onChange(Number(input.value));
        });
        input.addEventListener('change', endInteraction);
        input.addEventListener('blur', endInteraction);
        updateRangeProgress();
        return input;
    }

    // 帶數值標籤的 range 控制：數值標籤 + slider，並提供 setValue / setDisabled。
    // normalize 統一數值域（clamp/round），format 決定標籤文字（例如加 % 後綴）。
    // 這是 feature 面板滑桿的唯一樣板；feature 不應再自寫 label+range 組合。
    function labeledRange(config) {
        config = config || {};
        var normalize = config.normalize || Number;
        var format = config.format || String;
        var min = Number(config.min);
        var max = Number(config.max);
        var initial = normalize(config.value);
        var valueLabel = app.utils.dom.el('span', {
            className: config.valueClassName || 'range-value',
            text: format(initial)
        });
        var input = rangeInput(initial, min, max, config.step, function (nextValue) {
            var next = normalize(nextValue);
            valueLabel.textContent = format(next);
            config.onChange(next);
        }, config.interaction);
        var wrapper = app.utils.dom.el('div', {
            className: config.className || 'range-control',
            children: [valueLabel, input]
        });
        wrapper.setValue = function setValue(nextValue) {
            var next = normalize(nextValue);
            var range = max - min;
            var progress = range > 0 ? (next - min) / range * 100 : 0;
            input.value = next;
            input.style.setProperty('--range-progress', Math.max(0, Math.min(100, progress)) + '%');
            valueLabel.textContent = format(next);
        };
        wrapper.setDisabled = function setDisabled(nextDisabled) {
            input.disabled = Boolean(nextDisabled);
            wrapper.classList.toggle('is-disabled', Boolean(nextDisabled));
        };
        return wrapper;
    }

    // 滑桿拖曳期間的 live preview hold 樣板：start/end 成對通知 controller。
    function previewHoldHandlers(controller, id) {
        return {
            onInteractionStart: function () {
                controller.beginPreviewHold(id);
            },
            onInteractionEnd: function () {
                controller.endPreviewHold();
            }
        };
    }

    // 建立下拉選單，options 使用 { value, label }。
    // 回傳 { node, select }：node 是帶自訂 chevron 的共用包裝，select 供讀寫目前值。
    function selectInput(value, options, onChange) {
        var select = app.utils.dom.el('select');
        options.forEach(function (option) {
            select.appendChild(app.utils.dom.option(option.value, option.label));
        });
        select.value = value;
        select.addEventListener('change', function () {
            onChange(select.value);
        });
        return { node: app.ui.selectField.create(select), select: select };
    }

    // 建立 checkbox，變更時回傳 checked boolean。
    function checkboxInput(checked, onChange) {
        var input = app.utils.dom.el('input', { attrs: { type: 'checkbox' } });
        input.checked = checked;
        input.addEventListener('change', function () {
            onChange(input.checked);
        });
        return input;
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.panelUtils = {
        t: t,
        section: section,
        row: row,
        svgIcon: svgIcon,
        numberInput: numberInput,
        toggleSwitchInput: toggleSwitchInput,
        unitNumberInput: unitNumberInput,
        rangeInput: rangeInput,
        labeledRange: labeledRange,
        previewHoldHandlers: previewHoldHandlers,
        selectInput: selectInput,
        checkboxInput: checkboxInput
    };
})(window.DitherApp);
