(async () => {
  function render(css, markup) {
    const frame = document.createElement('iframe');
    frame.style.width = '1000px';
    document.body.append(frame);
    const doc = frame.contentDocument;
    doc.body.innerHTML = markup;
    const style = doc.createElement('style');
    style.textContent = css;
    doc.head.append(style);
    return { frame, doc, style };
  }
  function assert(value, message) { if (!value) throw new Error(message); }
  try {
    const source = render(fixture.css, fixture.markup);
    const processed = render(fixture.processed, fixture.markup);
    for (const check of fixture.checks) {
      const read = ({ frame, doc }) => frame.contentWindow.getComputedStyle(
        doc.querySelector(check.selector), check.pseudo || null).getPropertyValue(check.property);
      const expected = read(source);
      assert(read(processed) === expected, `${check.selector} ${check.property}: production differs`);
      if (check.expected) assert(expected === check.expected, `${check.property}: fixture got ${expected}`);
    }
    const markup = '<div class="submenu-item" style="width:300px"><div class="language-submenu"></div></div>'
      + '<div class="page-area" style="--content-width:100px"></div>';
    const original = render(fixture.sourceApp, markup);
    const production = render(fixture.productionApp, markup);
    const cssom = ({ style }) => Array.from(style.sheet.cssRules, rule => rule.cssText);
    assert(JSON.stringify(cssom(original)) === JSON.stringify(cssom(production)), 'app CSSOM differs');
    for (const [selector, property, expected] of [
      ['.language-submenu', 'right', '304px'], ['.page-area', 'max-width', '164px']
    ]) {
      const read = ({ frame, doc }) => frame.contentWindow.getComputedStyle(doc.querySelector(selector))[property];
      assert(read(original) === expected, `source ${selector}: ${read(original)}`);
      assert(read(production) === expected, `production ${selector}: ${read(production)}`);
    }
    const sharedMarkup = '<div class="host"><div class="probe active"></div><div class="sibling"></div></div>';
    const sharedSource = render(fixture.sharedCss, sharedMarkup);
    const sharedOutput = render(fixture.sharedProcessed, sharedMarkup);
    assert(JSON.stringify(cssom(sharedSource)) === JSON.stringify(cssom(sharedOutput)), 'shared v1 CSSOM differs');
    for (const [selector, property, expected] of [
      ['.probe', 'bottom', '108px'], ['.probe', 'padding-top', '20px'],
      ['.probe', 'width', '300px'], ['.probe', 'height', '17px'],
      ['.sibling', 'margin-left', '22px']
    ]) {
      const read = ({ frame, doc }) => frame.contentWindow.getComputedStyle(doc.querySelector(selector)).getPropertyValue(property);
      assert(read(sharedSource) === expected, `shared source ${property}: ${read(sharedSource)}`);
      assert(read(sharedOutput) === expected, `shared gzip ${property}: ${read(sharedOutput)}`);
    }
    document.getElementById('result').textContent = 'PASS';
  } catch (error) {
    document.getElementById('result').textContent = error.stack || String(error);
  }
})();
