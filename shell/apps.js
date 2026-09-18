/* Josh OS built-in apps.
 *
 * Each app is a plain object: identity, default window size, and a mount()
 * that fills a window body. No app touches the window manager directly —
 * that boundary is the whole point, and it is the one to keep when this
 * moves to real processes talking to a real compositor.
 */

(function () {
  'use strict';

  function el(tag, cls, text) {
    var n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text !== undefined) n.textContent = text;
    return n;
  }

  JoshOS.registerApp({
    id: 'about',
    name: 'About Josh OS',
    glyph: '\u25C8',
    width: 460,
    height: 300,
    mount: function (body) {
      var wrap = el('div', 'app');
      wrap.appendChild(el('h2', null, 'Josh OS'));
      wrap.appendChild(el('p', null,
        'A desktop built on the idea that a system should feel simpler after you understand it, not more complicated.'));
      wrap.appendChild(el('p', null,
        'This is the shell prototype: the window system, visual language and interaction model, running in a browser so they can be argued about before any of it is written in Rust.'));
      var r = el('div', 'row');
      r.appendChild(el('span', 'muted', 'Stage 0 — bootable shell prototype'));
      r.appendChild(el('span', 'muted', 'v0.1.0'));
      wrap.appendChild(r);
      body.appendChild(wrap);
    }
  });

  var FS = {
    Home: [
      { n: 'Documents', t: 'dir' },
      { n: 'Downloads', t: 'dir' },
      { n: 'Pictures', t: 'dir' },
      { n: 'Projects', t: 'dir' },
      { n: 'notes.txt', t: 'file' }
    ],
    Documents: [{ n: 'roadmap.md', t: 'file' }, { n: 'budget.ods', t: 'file' }],
    Downloads: [{ n: 'josh-os.iso', t: 'file' }],
    Pictures: [{ n: 'wallpaper.png', t: 'file' }],
    Projects: [{ n: 'josh-os', t: 'dir' }, { n: 'shell.js', t: 'file' }],
    'josh-os': [{ n: 'README.md', t: 'file' }, { n: 'shell', t: 'dir' }]
  };

  JoshOS.registerApp({
    id: 'files',
    name: 'Files',
    glyph: '\u25F0',
    width: 620,
    height: 400,
    mount: function (body) {
      var layout = el('div', 'sidebar-layout');
      var side = el('nav', 'sidebar');
      var grid = el('div', 'file-grid');
      var current = 'Home';

      function draw() {
        grid.innerHTML = '';
        (FS[current] || []).forEach(function (item) {
          var tile = el('button', 'file-tile');
          tile.appendChild(el('div', 'glyph', item.t === 'dir' ? '\u25F1' : '\u25A4'));
          tile.appendChild(el('div', null, item.n));
          tile.ondblclick = function () {
            if (item.t === 'dir' && FS[item.n]) { current = item.n; draw(); markSidebar(); }
            else JoshOS.notify('Files', 'No handler registered for ' + item.n);
          };
          grid.appendChild(tile);
        });
      }

      function markSidebar() {
        [].forEach.call(side.children, function (b) {
          b.setAttribute('aria-current', b.textContent === current ? 'true' : 'false');
        });
      }

      ['Home', 'Documents', 'Downloads', 'Pictures', 'Projects'].forEach(function (name) {
        var b = el('button', 'sidebar-item', name);
        b.onclick = function () { current = name; draw(); markSidebar(); };
        side.appendChild(b);
      });

      layout.appendChild(side);
      layout.appendChild(grid);
      body.appendChild(layout);
      draw();
      markSidebar();
    }
  });

  JoshOS.registerApp({
    id: 'terminal',
    name: 'Terminal',
    glyph: '\u232B',
    width: 560,
    height: 340,
    mount: function (body) {
      var term = el('div', 'terminal');
      var out = el('div');
      var line = el('div', 'line');
      var prompt = el('span', 'prompt', 'josh@joshos:~$ ');
      var input = el('input', 'terminal-input');
      input.setAttribute('spellcheck', 'false');
      input.setAttribute('autocomplete', 'off');

      line.appendChild(prompt);
      line.appendChild(input);
      term.appendChild(out);
      term.appendChild(line);
      body.appendChild(term);

      function print(text) {
        var l = el('div', 'line', text);
        out.appendChild(l);
        term.scrollTop = term.scrollHeight;
      }

      var commands = {
        help: function () {
          print('Commands: help, about, ls, open <app>, theme <light|dark>, wallpaper <name>, apps, clear, echo <text>');
        },
        about: function () {
          print('Josh OS shell prototype v0.1.0 — stage 0 product track.');
        },
        ls: function () {
          print((FS.Home || []).map(function (f) { return f.n; }).join('   '));
        },
        apps: function () {
          print(JoshOS.apps().map(function (a) { return a.id; }).join('   '));
        },
        open: function (args) {
          if (!args[0]) return print('usage: open <app>');
          JoshOS.openApp(args[0]);
        },
        theme: function (args) {
          if (args[0] !== 'light' && args[0] !== 'dark') return print('usage: theme <light|dark>');
          JoshOS.setTheme(args[0]);
          print('theme set to ' + args[0]);
        },
        wallpaper: function (args) {
          var names = Object.keys(JoshOS.wallpapers);
          if (names.indexOf(args[0]) === -1) return print('available: ' + names.join(', '));
          JoshOS.setWallpaper(args[0]);
          print('wallpaper set to ' + args[0]);
        },
        echo: function (args) { print(args.join(' ')); },
        clear: function () { out.innerHTML = ''; }
      };

      input.addEventListener('keydown', function (e) {
        if (e.key !== 'Enter') return;
        var raw = input.value.trim();
        input.value = '';
        print('josh@joshos:~$ ' + raw);
        if (!raw) return;
        var parts = raw.split(/\s+/);
        var cmd = commands[parts[0]];
        if (cmd) cmd(parts.slice(1));
        else print(parts[0] + ': command not found. Try "help".');
      });

      term.addEventListener('click', function () { input.focus(); });
      setTimeout(function () { input.focus(); }, 50);
      print('Josh OS terminal. Type "help".');
    }
  });

  JoshOS.registerApp({
    id: 'editor',
    name: 'Text Editor',
    glyph: '\u270E',
    width: 520,
    height: 380,
    mount: function (body) {
      var ta = el('textarea', 'editor');
      ta.spellcheck = false;
      ta.value = '# notes\n\nJosh OS should feel simpler after you understand it,\nnot more complicated.\n';
      body.appendChild(ta);
    }
  });

  JoshOS.registerApp({
    id: 'settings',
    name: 'Settings',
    glyph: '\u2699',
    width: 480,
    height: 360,
    mount: function (body) {
      var wrap = el('div', 'app');
      wrap.appendChild(el('h2', null, 'Appearance'));

      var themeRow = el('div', 'row');
      themeRow.appendChild(el('span', null, 'Theme'));
      var themeBtns = el('div');
      ['light', 'dark'].forEach(function (mode) {
        var b = el('button', 'btn ghost', mode);
        b.style.marginLeft = '8px';
        b.onclick = function () { JoshOS.setTheme(mode); };
        themeBtns.appendChild(b);
      });
      themeRow.appendChild(themeBtns);
      wrap.appendChild(themeRow);

      var accentRow = el('div', 'row');
      accentRow.appendChild(el('span', null, 'Accent'));
      var sw = el('div', 'swatches');
      ['#48c39f', '#5aa9e6', '#dda43c', '#d9738f', '#9a8cd6'].forEach(function (hex) {
        var b = el('button', 'swatch');
        b.style.background = hex;
        b.onclick = function () {
          JoshOS.setAccent(hex);
          [].forEach.call(sw.children, function (c) { c.setAttribute('aria-current', c === b ? 'true' : 'false'); });
        };
        sw.appendChild(b);
      });
      accentRow.appendChild(sw);
      wrap.appendChild(accentRow);

      var wallRow = el('div', 'row');
      wallRow.appendChild(el('span', null, 'Wallpaper'));
      var wallBtns = el('div');
      Object.keys(JoshOS.wallpapers).forEach(function (name) {
        var b = el('button', 'btn ghost', name);
        b.style.marginLeft = '6px';
        b.onclick = function () { JoshOS.setWallpaper(name); };
        wallBtns.appendChild(b);
      });
      wallRow.appendChild(wallBtns);
      wrap.appendChild(wallRow);

      wrap.appendChild(el('h2', null, 'System'));
      wrap.appendChild(el('p', null,
        'Stage 0 product prototype. Settings that would touch real hardware are intentionally absent rather than faked — a dead toggle is worse than a missing one.'));
      body.appendChild(wrap);
    }
  });

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', JoshOS.boot);
  } else {
    JoshOS.boot();
  }
})();
