(async function () {
    var passed = [], frames = [];
    try {
        async function fixture(width, theme) {
            var frame = document.createElement('iframe');
            frame.style.width = width + 'px';
            frame.style.height = '500px';
            frame.style.border = '0';
            frames.push(frame);
            frame.srcdoc = '<!doctype html><meta name="viewport" content="width=device-width, initial-scale=1">' +
                '<style>' + testStyleCss.themes + '\n' + testStyleCss.components + '</style>' +
                '<body data-theme="' + theme + '"><div class="modal-backdrop">' +
                '<div class="modal-dialog"><div class="modal-actions"><button>OK</button></div></div></div>' +
                '<div class="palette-add-button"><span class="svg-icon"></span></div></body>';
            var ready = new Promise(function (resolve) { frame.onload = resolve; });
            document.body.appendChild(frame);
            await ready;
            return frame.contentWindow;
        }
        var desktop = await fixture(900, 'light');
        var backdrop = desktop.document.querySelector('.modal-backdrop');
        var dialog = desktop.document.querySelector('.modal-dialog');
        var actions = desktop.document.querySelector('.modal-actions');
        harness.assert(desktop.getComputedStyle(backdrop).position === 'fixed' &&
            desktop.getComputedStyle(backdrop).display === 'grid' &&
            desktop.getComputedStyle(dialog).maxWidth === '420px' &&
            desktop.getComputedStyle(actions).display === 'flex',
            'common modal layout works with themes and components only');
        var lightFilter = desktop.getComputedStyle(desktop.document.querySelector('.svg-icon'))
            .getPropertyValue('--svg-icon-filter').trim();
        harness.assert(lightFilter.indexOf('brightness(') !== -1, 'light accent icon receives theme token');
        passed.push('Desktop modal and light icon computed styles without device.css');

        var mobile = await fixture(375, 'dark');
        backdrop = mobile.document.querySelector('.modal-backdrop');
        dialog = mobile.document.querySelector('.modal-dialog');
        harness.assert(mobile.innerWidth <= 700 &&
            mobile.getComputedStyle(backdrop).paddingLeft === '0px' &&
            mobile.getComputedStyle(backdrop).alignItems === 'end' &&
            mobile.getComputedStyle(dialog).maxWidth === 'none' &&
            mobile.getComputedStyle(dialog).borderBottomLeftRadius === '0px',
            'narrow-screen modal overrides are active');
        var darkFilter = mobile.getComputedStyle(mobile.document.querySelector('.svg-icon'))
            .getPropertyValue('--svg-icon-filter').trim();
        harness.assert(darkFilter.indexOf('brightness(') !== -1 && darkFilter !== lightFilter,
            'dark accent icon receives its own theme token');
        passed.push('Narrow modal and dark icon computed styles without device.css');
        document.getElementById('result').textContent = JSON.stringify({passed: passed});
    } catch (error) {
        document.getElementById('result').textContent = JSON.stringify({passed: passed, error: error.stack});
    } finally {
        frames.forEach(function (frame) { frame.remove(); });
    }
})();
