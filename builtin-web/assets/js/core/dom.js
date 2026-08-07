const $ = (id) => document.getElementById(id);
const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));
const TRANSIENT_NOTICE_MS = 2200;
const SVG_NAMESPACE = 'http://www.w3.org/2000/svg';

let globalNoticeTimer = 0;
let globalNoticeGeneration = 0;

function appendChildren(node, children) {
  (children || []).forEach((child) => {
    if (child !== null && child !== undefined) node.append(child);
  });
}

function setElementOptions(node, options = {}) {
  if (options.id) node.id = options.id;
  if (options.className) node.setAttribute('class', options.className);
  if (options.text !== undefined) node.textContent = options.text;
  Object.keys(options.attrs || {}).forEach((name) => {
    const value = options.attrs[name];
    if (value !== false && value !== null && value !== undefined) {
      node.setAttribute(name, value === true ? '' : String(value));
    }
  });
  Object.keys(options.dataset || {}).forEach((name) => {
    node.dataset[name] = options.dataset[name];
  });
  Object.keys(options.props || {}).forEach((name) => {
    node[name] = options.props[name];
  });
  appendChildren(node, options.children);
  return node;
}

function el(tagName, options) {
  return setElementOptions(document.createElement(tagName), options);
}

function svgEl(tagName, options) {
  return setElementOptions(document.createElementNS(SVG_NAMESPACE, tagName), options);
}

function icon(symbolId, className) {
  return svgEl('svg', {
    className,
    attrs: { 'aria-hidden': 'true' },
    children: [svgEl('use', { attrs: { href: `#i-${symbolId}` } })]
  });
}

function translated(tagName, key, fallback, options = {}) {
  const attrs = Object.assign({}, options.attrs, { 'data-i18n': key });
  return el(tagName, Object.assign({}, options, { attrs, text: fallback }));
}

function detailRow(labelKey, fallback, valueId) {
  return el('div', {
    children: [
      translated('dt', labelKey, fallback),
      el('dd', { id: valueId, text: '—' })
    ]
  });
}

function setText(id, value) {
  const node = $(id);
  if (node) node.textContent = value ?? '—';
}

function setNotice(message, error = false, target = $('notice')) {
  if (target === $('notice')) {
    globalNoticeGeneration += 1;
    window.clearTimeout(globalNoticeTimer);
    globalNoticeTimer = 0;
  }
  target.textContent = message || '';
  target.classList.toggle('error', Boolean(error));
  target.hidden = !message;
}

function setTransientNotice(message, duration = TRANSIENT_NOTICE_MS) {
  const target = $('notice');
  setNotice(message, false, target);
  const generation = globalNoticeGeneration;
  globalNoticeTimer = window.setTimeout(() => {
    if (generation === globalNoticeGeneration) setNotice('', false, target);
  }, duration);
}

function formatBytes(value) {
  const bytes = Number(value) || 0;
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1048576).toFixed(2)} MB`;
}

function storagePercent(capacity) {
  return capacity && capacity.total_bytes
    ? Math.round(capacity.used_bytes * 100 / capacity.total_bytes)
    : 0;
}

DeviceConsole.utils.dom = {
  detailRow,
  el,
  icon,
  svg: svgEl,
  translated
};
