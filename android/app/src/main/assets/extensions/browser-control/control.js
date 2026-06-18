/*
 * Veyra Browser Control — content script  (v2.0.0)
 *
 * Injected at document-start into every browsing tab. Exposes
 * window.__VEYRA_CONTROL__, the automation surface the native shell drives over
 * its control channel (the C++ side calls these via evaluate_javascript).
 *
 * Every method returns a JSON string so the native side always receives a single
 * parseable value. Element selectors accept three forms:
 *     "#css .selector"        → querySelector
 *     "text=Sign In"          → first element whose trimmed text equals/contains it
 *     "xpath=//button[@id]"   → XPath
 */
(function () {
  'use strict';
  if (window.__VEYRA_CONTROL__) { return; } // idempotent

  // ── console / error capture (installed once) ─────────────────────
  var LOGS = [];
  var ERRORS = [];
  var LOG_CAP = 500;
  ['log', 'info', 'warn', 'error', 'debug'].forEach(function (level) {
    var orig = console[level];
    console[level] = function () {
      try {
        var parts = [];
        for (var i = 0; i < arguments.length; i++) {
          var a = arguments[i];
          parts.push(typeof a === 'object' ? safeStringify(a) : String(a));
        }
        LOGS.push({ level: level, text: parts.join(' '), ts: Date.now() });
        if (LOGS.length > LOG_CAP) LOGS.shift();
      } catch (e) {}
      return orig && orig.apply(console, arguments);
    };
  });
  window.addEventListener('error', function (ev) {
    ERRORS.push({ message: ev.message, source: ev.filename, line: ev.lineno, ts: Date.now() });
    if (ERRORS.length > LOG_CAP) ERRORS.shift();
  });
  window.addEventListener('unhandledrejection', function (ev) {
    ERRORS.push({ message: 'unhandledrejection: ' + String(ev.reason), ts: Date.now() });
    if (ERRORS.length > LOG_CAP) ERRORS.shift();
  });

  function safeStringify(o) {
    try { return JSON.stringify(o); } catch (e) { return String(o); }
  }
  function ok(extra) { return JSON.stringify(Object.assign({ ok: true }, extra || {})); }
  function fail(message) { return JSON.stringify({ ok: false, error: String(message) }); }

  // ── universal element resolver ───────────────────────────────────
  function resolve(selector) {
    if (selector == null) return null;
    if (selector.indexOf('xpath=') === 0) {
      var xp = selector.slice(6);
      var r = document.evaluate(xp, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null);
      return r.singleNodeValue;
    }
    if (selector.indexOf('text=') === 0) {
      var needle = selector.slice(5).trim();
      var all = document.querySelectorAll('a,button,input,label,span,div,td,th,li,h1,h2,h3,p,[role]');
      var exact = null, partial = null;
      for (var i = 0; i < all.length; i++) {
        var t = (all[i].innerText || all[i].value || '').trim();
        if (t === needle) { exact = all[i]; break; }
        if (!partial && t.indexOf(needle) !== -1) partial = all[i];
      }
      return exact || partial;
    }
    return document.querySelector(selector);
  }
  function resolveAll(selector) {
    if (selector.indexOf('xpath=') === 0) {
      var out = [];
      var r = document.evaluate(selector.slice(6), document, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
      for (var i = 0; i < r.snapshotLength; i++) out.push(r.snapshotItem(i));
      return out;
    }
    return Array.prototype.slice.call(document.querySelectorAll(selector));
  }

  function isVisible(el) {
    if (!el) return false;
    if (el.offsetParent === null && getComputedStyle(el).position !== 'fixed') return false;
    var r = el.getBoundingClientRect();
    var st = getComputedStyle(el);
    return r.width > 0 && r.height > 0 && st.visibility !== 'hidden' && st.display !== 'none' && st.opacity !== '0';
  }
  function rectOf(el) {
    var r = el.getBoundingClientRect();
    return { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height) };
  }
  function fireInput(el) {
    el.dispatchEvent(new Event('input', { bubbles: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }
  function mouse(el, type) {
    var r = el.getBoundingClientRect();
    el.dispatchEvent(new MouseEvent(type, {
      bubbles: true, cancelable: true, view: window,
      clientX: r.x + r.width / 2, clientY: r.y + r.height / 2
    }));
  }
  function shortSelector(el) {
    if (el.id) return '#' + el.id;
    var name = el.getAttribute('name');
    if (name) return el.tagName.toLowerCase() + '[name="' + name + '"]';
    return el.tagName.toLowerCase();
  }

  // ── numbered overlay annotation (for vision/computer-use drivers) ─
  var NODES = [];
  function clearAnnotations() {
    NODES = [];
    var box = document.getElementById('__veyra_overlay__');
    if (box) box.remove();
  }
  function clickableEls() {
    var sel = 'a[href],button,input:not([type=hidden]),select,textarea,[role=button],[role=link],[onclick],[tabindex]';
    return resolveAll(sel).filter(isVisible);
  }

  var api = {
    // ── queries ──────────────────────────────────────────────────
    exists: function (s) { try { return ok({ value: !!resolve(s) }); } catch (e) { return fail(e.message); } },
    count:  function (s) { try { return ok({ value: resolveAll(s).length }); } catch (e) { return fail(e.message); } },
    visible:function (s) { try { return ok({ value: isVisible(resolve(s)) }); } catch (e) { return fail(e.message); } },
    getText: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        return ok({ value: (el.innerText || el.textContent || '').trim() }); } catch (e) { return fail(e.message); }
    },
    getValue: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        return ok({ value: el.value != null ? el.value : '' }); } catch (e) { return fail(e.message); }
    },
    getAttribute: function (s, a) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        return ok({ value: el.getAttribute(a) }); } catch (e) { return fail(e.message); }
    },
    attrs: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        var o = {}; for (var i = 0; i < el.attributes.length; i++) o[el.attributes[i].name] = el.attributes[i].value;
        return ok({ value: o }); } catch (e) { return fail(e.message); }
    },
    html: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        return ok({ value: el.outerHTML.slice(0, 4000) }); } catch (e) { return fail(e.message); }
    },
    bounds: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        return ok({ value: rectOf(el) }); } catch (e) { return fail(e.message); }
    },
    query: function (s) {
      try {
        var els = resolveAll(s).slice(0, 50).map(function (el) {
          return { tag: el.tagName.toLowerCase(), text: (el.innerText || '').trim().slice(0, 60),
                   selector: shortSelector(el), visible: isVisible(el) };
        });
        return ok({ value: els });
      } catch (e) { return fail(e.message); }
    },
    pageInfo: function () {
      try { return ok({ url: location.href, title: document.title, readyState: document.readyState,
        forms: document.forms.length, links: document.links.length,
        scrollY: window.scrollY, scrollHeight: document.body ? document.body.scrollHeight : 0 }); }
      catch (e) { return fail(e.message); }
    },
    links: function (limit) {
      try { var max = limit || 50, out = [], els = document.querySelectorAll('a[href]');
        for (var i = 0; i < els.length && out.length < max; i++)
          out.push({ text: (els[i].innerText || '').trim().slice(0, 80), href: els[i].href });
        return ok({ value: out }); } catch (e) { return fail(e.message); }
    },

    // ── computer-use surface ─────────────────────────────────────
    clickable: function () {
      try {
        var els = clickableEls();
        NODES = els;
        var out = els.slice(0, 200).map(function (el, i) {
          return { index: i, tag: el.tagName.toLowerCase(),
                   text: (el.innerText || el.value || el.getAttribute('aria-label') || el.getAttribute('placeholder') || '').trim().slice(0, 60),
                   selector: shortSelector(el), rect: rectOf(el) };
        });
        return ok({ count: out.length, value: out });
      } catch (e) { return fail(e.message); }
    },
    textDump: function (limit) {
      try {
        var max = limit || 6000;
        var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null);
        var parts = [], n;
        while ((n = walker.nextNode())) {
          var t = n.nodeValue.replace(/\s+/g, ' ').trim();
          if (t && isVisible(n.parentElement)) parts.push(t);
          if (parts.join(' ').length > max) break;
        }
        return ok({ value: parts.join(' ').slice(0, max) });
      } catch (e) { return fail(e.message); }
    },
    annotate: function () {
      try {
        clearAnnotations();
        var els = clickableEls();
        NODES = els;
        var box = document.createElement('div');
        box.id = '__veyra_overlay__';
        box.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:2147483647;';
        els.slice(0, 200).forEach(function (el, i) {
          var r = el.getBoundingClientRect();
          var tag = document.createElement('div');
          tag.textContent = i;
          tag.style.cssText = 'position:absolute;left:' + Math.max(0, r.x) + 'px;top:' + Math.max(0, r.y) +
            'px;background:#ff3b52;color:#fff;font:bold 11px monospace;padding:0 4px;border-radius:3px;' +
            'box-shadow:0 0 0 1px #fff;';
          box.appendChild(tag);
          var outline = document.createElement('div');
          outline.style.cssText = 'position:absolute;left:' + r.x + 'px;top:' + r.y + 'px;width:' + r.width +
            'px;height:' + r.height + 'px;border:1px solid rgba(255,59,82,0.6);border-radius:2px;';
          box.appendChild(outline);
        });
        document.documentElement.appendChild(box);
        return ok({ count: Math.min(els.length, 200) });
      } catch (e) { return fail(e.message); }
    },
    clearAnnotations: function () { try { clearAnnotations(); return ok(); } catch (e) { return fail(e.message); } },
    clickIndex: function (n) {
      try { var el = NODES[n]; if (!el) return fail('no annotated element #' + n);
        el.scrollIntoView({ block: 'center' }); el.click(); return ok({ clicked: shortSelector(el) }); }
      catch (e) { return fail(e.message); }
    },

    // ── actions ──────────────────────────────────────────────────
    click: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.scrollIntoView({ block: 'center' }); el.click(); return ok(); } catch (e) { return fail(e.message); }
    },
    dblclick: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        mouse(el, 'dblclick'); return ok(); } catch (e) { return fail(e.message); }
    },
    rightclick: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        mouse(el, 'contextmenu'); return ok(); } catch (e) { return fail(e.message); }
    },
    hover: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        mouse(el, 'mouseover'); mouse(el, 'mouseenter'); mouse(el, 'mousemove'); return ok(); }
      catch (e) { return fail(e.message); }
    },
    focus: function (s) { try { var el = resolve(s); if (!el) return fail('not found: ' + s); el.focus(); return ok(); } catch (e) { return fail(e.message); } },
    blur:  function (s) { try { var el = resolve(s); if (!el) return fail('not found: ' + s); el.blur(); return ok(); } catch (e) { return fail(e.message); } },
    clear: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.focus(); if ('value' in el) { el.value = ''; fireInput(el); } return ok(); } catch (e) { return fail(e.message); }
    },
    fill: function (s, value) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.focus(); if ('value' in el) { el.value = value; fireInput(el); } else { el.textContent = value; }
        return ok(); } catch (e) { return fail(e.message); }
    },
    type: function (s, text) {
      try {
        var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.focus();
        if ('value' in el) el.value = '';
        for (var i = 0; i < text.length; i++) {
          var ch = text[i];
          el.dispatchEvent(new KeyboardEvent('keydown', { key: ch, bubbles: true }));
          if ('value' in el) el.value += ch;
          el.dispatchEvent(new KeyboardEvent('keypress', { key: ch, bubbles: true }));
          el.dispatchEvent(new Event('input', { bubbles: true }));
          el.dispatchEvent(new KeyboardEvent('keyup', { key: ch, bubbles: true }));
        }
        el.dispatchEvent(new Event('change', { bubbles: true }));
        return ok();
      } catch (e) { return fail(e.message); }
    },
    keypress: function (key) {
      try {
        var el = document.activeElement || document.body;
        el.dispatchEvent(new KeyboardEvent('keydown', { key: key, bubbles: true }));
        el.dispatchEvent(new KeyboardEvent('keyup', { key: key, bubbles: true }));
        if (key === 'Enter' && el.form) {
          if (typeof el.form.requestSubmit === 'function') el.form.requestSubmit(); else el.form.submit();
        }
        return ok();
      } catch (e) { return fail(e.message); }
    },
    setChecked: function (s, checked) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.checked = !!checked; fireInput(el); return ok(); } catch (e) { return fail(e.message); }
    },
    selectOption: function (s, value) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.value = value; fireInput(el); return ok(); } catch (e) { return fail(e.message); }
    },
    submit: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        var form = el.tagName === 'FORM' ? el : el.closest('form');
        if (!form) return fail('no enclosing form: ' + s);
        if (typeof form.requestSubmit === 'function') form.requestSubmit(); else form.submit();
        return ok(); } catch (e) { return fail(e.message); }
    },
    scrollTo: function (x, y) { try { window.scrollTo(x || 0, y || 0); return ok({ scrollY: window.scrollY }); } catch (e) { return fail(e.message); } },
    scrollBy: function (x, y) { try { window.scrollBy(x || 0, y || 0); return ok({ scrollY: window.scrollY }); } catch (e) { return fail(e.message); } },
    scrollIntoView: function (s) {
      try { var el = resolve(s); if (!el) return fail('not found: ' + s);
        el.scrollIntoView({ block: 'center' }); return ok(); } catch (e) { return fail(e.message); }
    },

    // ── storage / cookies ────────────────────────────────────────
    cookies: function () { try { return ok({ value: document.cookie }); } catch (e) { return fail(e.message); } },
    storageGet: function (k) { try { return ok({ value: localStorage.getItem(k) }); } catch (e) { return fail(e.message); } },
    storageSet: function (k, v) { try { localStorage.setItem(k, v); return ok(); } catch (e) { return fail(e.message); } },
    storageClear: function () { try { localStorage.clear(); return ok(); } catch (e) { return fail(e.message); } },

    // ── logs / errors ────────────────────────────────────────────
    getLogs: function () { return ok({ value: LOGS.slice(-100) }); },
    getErrors: function () { return ok({ value: ERRORS.slice(-100) }); },
    clearLogs: function () { LOGS = []; ERRORS = []; return ok(); },

    // ── predicates (used by native wait-* polling) ───────────────
    predExists: function (s) { try { return JSON.stringify(!!resolve(s)); } catch (e) { return 'false'; } },
    predText: function (s, t) {
      try { var el = resolve(s); return JSON.stringify(!!el && (el.innerText || el.textContent || '').indexOf(t) !== -1); }
      catch (e) { return 'false'; }
    },

    // ── assertions ───────────────────────────────────────────────
    assertText: function (s, expected) {
      try { var el = resolve(s); if (!el) return JSON.stringify({ ok: true, pass: false, actual: null, error: 'not found' });
        var actual = (el.innerText || el.textContent || '').trim();
        return JSON.stringify({ ok: true, pass: actual.indexOf(expected) !== -1, actual: actual.slice(0, 200) }); }
      catch (e) { return fail(e.message); }
    },
    assertExists: function (s) {
      try { return JSON.stringify({ ok: true, pass: !!resolve(s) }); } catch (e) { return fail(e.message); }
    }
  };

  Object.defineProperty(window, '__VEYRA_CONTROL__', {
    value: Object.freeze(api), writable: false, configurable: false, enumerable: false
  });
})();
