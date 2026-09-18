/* Josh OS shell runtime.
 *
 * This is a prototype of the Josh Window System, running in a browser so the
 * interaction model can be designed and argued about at speed. The concepts
 * here — window records, focus stack, snap zones, the app registry — are meant
 * to survive the port to a real Wayland compositor. The rendering is not.
 *
 * Deliberately plain script (no ES modules) so index.html opens straight from
 * disk with file:// and no dev server.
 */

var JoshOS = (function () {
  'use strict';

  var state = {
    apps: {},
    windows: [],
    zTop: 100,
    seq: 0,
    focused: null
  };

  function save(key, value) {
    try { localStorage.setItem('joshos.' + key, JSON.stringify(value)); } catch (e) { }
  }

  function load(key, fallback) {
    try {
      var raw = localStorage.getItem('joshos.' + key);
      return raw === null ? fallback : JSON.parse(raw);
    } catch (e) { return fallback; }
  }

  function registerApp(app) {
    state.apps[app.id] = app;
  }

  function apps() {
    return Object.keys(state.apps).map(function (id) { return state.apps[id]; });
  }

  function desktopBounds() {
    var top = px('--chrome-menubar-height');
    var dock = px('--chrome-dock-height');
    return {
      x: 0,
      y: top,
      w: window.innerWidth,
      h: window.innerHeight - top - dock - 24
    };
  }

  function px(varName) {
    var v = getComputedStyle(document.documentElement).getPropertyValue(varName);
    return parseInt(v, 10) || 0;
  }

  function openApp(id) {
    var app = state.apps[id];
    if (!app) { notify('Josh OS', 'No app registered as "' + id + '"'); return null; }
    var existing = state.windows.filter(function (w) { return w.appId === id; })[0];
    if (existing) {
      if (existing.state === 'minimised') restore(existing);
      focus(existing);
      return existing;
    }
    return createWindow(app);
  }

  function createWindow(app) {
    var b = desktopBounds();
    var w = Math.min(app.width || 520, b.w - 40);
    var h = Math.min(app.height || 360, b.h - 40);
    var offset = (state.seq % 6) * 26;

    var win = {
      id: 'win-' + (++state.seq),
      appId: app.id,
      title: app.name,
      x: Math.round(b.x + (b.w - w) / 2 + offset - 60),
      y: Math.round(b.y + 24 + offset),
      w: w,
      h: h,
      state: 'normal',
      prev: null,
      el: null
    };

    win.el = renderWindow(win, app);
    document.getElementById('desktop').appendChild(win.el);
    state.windows.push(win);
    focus(win);
    app.mount(win.el.querySelector('.window-body'), win);
    syncDock();
    return win;
  }

  function renderWindow(win, app) {
    var el = document.createElement('section');
    el.className = 'window';
    el.id = win.id;
    el.setAttribute('role', 'dialog');
    el.setAttribute('aria-label', win.title);
    el.style.left = win.x + 'px';
    el.style.top = win.y + 'px';
    el.style.width = win.w + 'px';
    el.style.height = win.h + 'px';

    var bar = document.createElement('header');
    bar.className = 'titlebar';
    bar.innerHTML =
      '<button class="win-btn close" title="Close">\u2715</button>' +
      '<button class="win-btn min" title="Minimise">\u2500</button>' +
      '<button class="win-btn max" title="Maximise">\u25A1</button>' +
      '<span class="title"></span>';
    bar.querySelector('.title').textContent = (app.glyph ? app.glyph + '  ' : '') + win.title;

    var body = document.createElement('div');
    body.className = 'window-body';
    var grip = document.createElement('div');
    grip.className = 'resize-handle';

    el.appendChild(bar);
    el.appendChild(body);
    el.appendChild(grip);

    bar.querySelector('.close').onclick = function (e) { e.stopPropagation(); closeWindow(win); };
    bar.querySelector('.min').onclick = function (e) { e.stopPropagation(); minimise(win); };
    bar.querySelector('.max').onclick = function (e) { e.stopPropagation(); toggleMaximise(win); };
    bar.ondblclick = function () { toggleMaximise(win); };

    el.addEventListener('mousedown', function () { focus(win); });
    bar.addEventListener('mousedown', function (e) {
      if (e.target.closest('.win-btn')) return;
      beginDrag(win, e);
    });
    grip.addEventListener('mousedown', function (e) { beginResize(win, e); });

    return el;
  }

  function focus(win) {
    if (state.focused === win) return;
    state.windows.forEach(function (w) { w.el.classList.remove('focused'); });
    win.el.classList.add('focused');
    win.el.style.zIndex = ++state.zTop;
    state.focused = win;
    document.querySelector('#menubar .active-app').textContent = win.title;
  }

  function closeWindow(win) {
    win.el.remove();
    state.windows = state.windows.filter(function (w) { return w !== win; });
    if (state.focused === win) {
      state.focused = null;
      var next = state.windows[state.windows.length - 1];
      if (next) focus(next);
      else document.querySelector('#menubar .active-app').textContent = '';
    }
    syncDock();
  }

  function minimise(win) {
    win.state = 'minimised';
    win.el.dataset.state = 'minimised';
    if (state.focused === win) {
      state.focused = null;
      document.querySelector('#menubar .active-app').textContent = '';
    }
  }

  function restore(win) {
    win.state = 'normal';
    win.el.dataset.state = 'normal';
  }

  function place(win, rect, animate) {
    if (animate) {
      win.el.classList.add('is-animating');
      setTimeout(function () { win.el.classList.remove('is-animating'); }, 220);
    }
    win.x = Math.round(rect.x); win.y = Math.round(rect.y);
    win.w = Math.round(rect.w); win.h = Math.round(rect.h);
    win.el.style.left = win.x + 'px';
    win.el.style.top = win.y + 'px';
    win.el.style.width = win.w + 'px';
    win.el.style.height = win.h + 'px';
  }

  function toggleMaximise(win) {
    if (win.state === 'maximised') {
      place(win, win.prev, true);
      win.state = 'normal';
    } else {
      win.prev = { x: win.x, y: win.y, w: win.w, h: win.h };
      place(win, desktopBounds(), true);
      win.state = 'maximised';
    }
  }

  function snapZoneFor(mx, my) {
    var b = desktopBounds();
    var edge = 24;
    if (my <= b.y + edge) return { x: b.x, y: b.y, w: b.w, h: b.h, name: 'max' };
    if (mx <= b.x + edge) return { x: b.x, y: b.y, w: b.w / 2, h: b.h, name: 'left' };
    if (mx >= b.x + b.w - edge) return { x: b.x + b.w / 2, y: b.y, w: b.w / 2, h: b.h, name: 'right' };
    return null;
  }

  function beginDrag(win, e) {
    if (win.state === 'maximised') {
      var ratio = (e.clientX - win.x) / win.w;
      place(win, { x: e.clientX - win.prev.w * ratio, y: e.clientY - 16, w: win.prev.w, h: win.prev.h });
      win.state = 'normal';
    }
    var startX = e.clientX - win.x;
    var startY = e.clientY - win.y;
    var preview = document.getElementById('snap-preview');
    var zone = null;

    function move(ev) {
      place(win, { x: ev.clientX - startX, y: ev.clientY - startY, w: win.w, h: win.h });
      zone = snapZoneFor(ev.clientX, ev.clientY);
      if (zone) {
        preview.style.display = 'block';
        preview.style.left = zone.x + 'px';
        preview.style.top = zone.y + 'px';
        preview.style.width = zone.w + 'px';
        preview.style.height = zone.h + 'px';
      } else {
        preview.style.display = 'none';
      }
    }

    function up() {
      document.removeEventListener('mousemove', move);
      document.removeEventListener('mouseup', up);
      preview.style.display = 'none';
      if (zone) {
        win.prev = win.prev || { x: win.x, y: win.y, w: win.w, h: win.h };
        place(win, zone, true);
        win.state = zone.name === 'max' ? 'maximised' : 'normal';
      }
    }

    document.addEventListener('mousemove', move);
    document.addEventListener('mouseup', up);
    e.preventDefault();
  }

  function beginResize(win, e) {
    var startW = win.w, startH = win.h, startX = e.clientX, startY = e.clientY;

    function move(ev) {
      place(win, {
        x: win.x,
        y: win.y,
        w: Math.max(260, startW + ev.clientX - startX),
        h: Math.max(160, startH + ev.clientY - startY)
      });
      if (win.onResize) win.onResize();
    }
    function up() {
      document.removeEventListener('mousemove', move);
      document.removeEventListener('mouseup', up);
    }
    document.addEventListener('mousemove', move);
    document.addEventListener('mouseup', up);
    e.preventDefault();
    e.stopPropagation();
  }

  function syncDock() {
    var running = {};
    state.windows.forEach(function (w) { running[w.appId] = true; });
    [].forEach.call(document.querySelectorAll('.dock-item'), function (el) {
      el.dataset.running = running[el.dataset.app] ? 'true' : 'false';
    });
  }

  function buildDock() {
    var dock = document.getElementById('dock');
    dock.innerHTML = '';
    apps().filter(function (a) { return a.inDock !== false; }).forEach(function (app) {
      var b = document.createElement('button');
      b.className = 'dock-item';
      b.dataset.app = app.id;
      b.title = app.name;
      b.textContent = app.glyph;
      b.onclick = function () { openApp(app.id); };
      dock.appendChild(b);
    });
    syncDock();
  }

  function notify(title, body) {
    var host = document.getElementById('notifications');
    var n = document.createElement('div');
    n.className = 'notification';
    var t = document.createElement('div'); t.className = 'n-title'; t.textContent = title;
    var b = document.createElement('div'); b.className = 'n-body'; b.textContent = body;
    n.appendChild(t); n.appendChild(b);
    host.appendChild(n);
    setTimeout(function () { n.remove(); }, 4200);
  }

  function startClock() {
    var el = document.querySelector('#menubar .clock');
    function tick() {
      var d = new Date();
      el.textContent = d.toLocaleDateString(undefined, { weekday: 'short', day: 'numeric', month: 'short' }) +
        '  ' + d.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' });
    }
    tick();
    setInterval(tick, 10000);
  }

  var WALLPAPERS = {
    pine:   'linear-gradient(160deg, #1d3a34, #14171a 62%)',
    slate:  'linear-gradient(160deg, #26303a, #14171a 62%)',
    clay:   'linear-gradient(160deg, #3a2a24, #171412 62%)',
    plain:  'var(--color-bg)'
  };

  function setTheme(mode) {
    document.documentElement.dataset.theme = mode;
    save('theme', mode);
  }

  function theme() { return document.documentElement.dataset.theme || 'dark'; }

  function setWallpaper(name) {
    document.getElementById('desktop').style.setProperty('--wallpaper', WALLPAPERS[name] || WALLPAPERS.pine);
    save('wallpaper', name);
  }

  function setAccent(hex) {
    document.documentElement.style.setProperty('--color-accent', hex);
    save('accent', hex);
  }

  function boot() {
    setTheme(load('theme', 'dark'));
    setWallpaper(load('wallpaper', 'pine'));
    var accent = load('accent', null);
    if (accent) setAccent(accent);

    buildDock();
    startClock();

    document.getElementById('theme-toggle').onclick = function () {
      setTheme(theme() === 'dark' ? 'light' : 'dark');
    };

    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape' && state.focused) closeWindow(state.focused);
    });

    window.addEventListener('resize', function () {
      state.windows.forEach(function (w) {
        if (w.state === 'maximised') place(w, desktopBounds());
      });
    });

    openApp('about');
    setTimeout(function () { notify('Josh OS', 'Prototype shell running. Try the Terminal.'); }, 700);
  }

  return {
    registerApp: registerApp,
    openApp: openApp,
    closeWindow: closeWindow,
    notify: notify,
    setTheme: setTheme,
    theme: theme,
    setWallpaper: setWallpaper,
    setAccent: setAccent,
    wallpapers: WALLPAPERS,
    windows: function () { return state.windows.slice(); },
    apps: apps,
    boot: boot
  };
})();
