#!/usr/bin/env node
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';

const root = '/opt/josh-os/browser-test';
const downloads = '/home/josh/Downloads';
const cdpBase = 'http://127.0.0.1:9222';
const testPort = 18765;
const testOrigin = 'http://127.0.0.1:' + testPort;
const markerValue = 'josh-profile-v1';
const downloadName = 'josh-browser-download.txt';

function out(line) { process.stdout.write(line + '\n'); }
function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

function mime(file) {
  if (file.endsWith('.html')) return 'text/html; charset=utf-8';
  if (file.endsWith('.webm')) return 'video/webm';
  if (file.endsWith('.txt')) return 'text/plain; charset=utf-8';
  return 'application/octet-stream';
}

function serveFile(req, res, file, attachment = false) {
  const st = fs.statSync(file);
  const range = req.headers.range;
  const headers = { 'Content-Type': mime(file), 'Accept-Ranges': 'bytes' };
  if (attachment) headers['Content-Disposition'] = 'attachment; filename="' + path.basename(file) + '"';

  if (range) {
    const m = /^bytes=(\d+)-(\d*)$/.exec(range);
    if (!m) { res.writeHead(416); res.end(); return; }
    const start = Number(m[1]);
    const end = m[2] ? Math.min(Number(m[2]), st.size - 1) : st.size - 1;
    if (start > end || start >= st.size) { res.writeHead(416); res.end(); return; }
    headers['Content-Range'] = 'bytes ' + start + '-' + end + '/' + st.size;
    headers['Content-Length'] = String(end - start + 1);
    res.writeHead(206, headers);
    fs.createReadStream(file, { start, end }).pipe(res);
    return;
  }

  headers['Content-Length'] = String(st.size);
  res.writeHead(200, headers);
  fs.createReadStream(file).pipe(res);
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, testOrigin);
  if (url.pathname === '/' || url.pathname === '/index.html') {
    serveFile(req, res, path.join(root, 'index.html'));
  } else if (url.pathname === '/test.webm') {
    serveFile(req, res, path.join(root, 'test.webm'));
  } else if (url.pathname === '/download.txt') {
    serveFile(req, res, path.join(root, 'download.txt'), true);
  } else {
    res.writeHead(404); res.end('not found');
  }
});
await new Promise((resolve, reject) => {
  server.once('error', reject);
  server.listen(testPort, '127.0.0.1', resolve);
});

async function getJson(url, timeoutMs = 1000) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const r = await fetch(url, { signal: ctrl.signal });
    if (!r.ok) throw new Error(String(r.status) + ' ' + r.statusText);
    return await r.json();
  } finally { clearTimeout(t); }
}

async function waitForCdp(timeoutMs = 30000) {
  const end = Date.now() + timeoutMs;
  let last;
  while (Date.now() < end) {
    try { return await getJson(cdpBase + '/json/version'); }
    catch (e) { last = e; await sleep(250); }
  }
  throw new Error('Chromium DevTools endpoint unavailable: ' + last);
}

class Cdp {
  constructor(url) {
    this.url = url;
    this.id = 0;
    this.pending = new Map();
    this.events = new Map();
  }
  async open() {
    if (typeof WebSocket === 'undefined') throw new Error('Node WebSocket API unavailable');
    this.ws = new WebSocket(this.url);
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('CDP websocket timeout')), 5000);
      this.ws.addEventListener('open', () => { clearTimeout(timer); resolve(); }, { once: true });
      this.ws.addEventListener('error', () => { clearTimeout(timer); reject(new Error('CDP websocket error')); }, { once: true });
    });
    this.ws.addEventListener('message', ev => {
      const msg = JSON.parse(String(ev.data));
      if (msg.id && this.pending.has(msg.id)) {
        const p = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        if (msg.error) p.reject(new Error(msg.error.message)); else p.resolve(msg.result || {});
      } else if (msg.method) {
        const list = this.events.get(msg.method) || [];
        this.events.delete(msg.method);
        for (const r of list) r(msg.params || {});
      }
    });
  }
  send(method, params = {}, timeoutMs = 10000) {
    const id = ++this.id;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(method + ' timeout'));
      }, timeoutMs);
      this.pending.set(id, {
        resolve: v => { clearTimeout(timer); resolve(v); },
        reject: e => { clearTimeout(timer); reject(e); }
      });
      this.ws.send(JSON.stringify({ id, method, params }));
    });
  }
  close() { try { this.ws.close(); } catch {} }
}

async function createTarget(url) {
  const r = await fetch(cdpBase + '/json/new?' + encodeURIComponent(url), { method: 'PUT' });
  if (!r.ok) throw new Error('create target failed: ' + r.status);
  return await r.json();
}

async function connectTarget(url) {
  const t = await createTarget(url);
  const c = new Cdp(t.webSocketDebuggerUrl);
  await c.open();
  await c.send('Page.enable');
  await c.send('Runtime.enable');
  return { target: t, cdp: c };
}

async function evaluate(cdp, expression, opts = {}) {
  const r = await cdp.send('Runtime.evaluate', {
    expression,
    awaitPromise: opts.awaitPromise ?? true,
    returnByValue: true,
    userGesture: opts.userGesture ?? false
  }, opts.timeoutMs ?? 15000);
  if (r.exceptionDetails) throw new Error('JS exception: ' + (r.exceptionDetails.text || 'unknown'));
  return r.result?.value;
}

async function waitReady(cdp) {
  for (let i = 0; i < 100; i++) {
    const v = await evaluate(cdp, 'document.readyState');
    if (v === 'complete' || v === 'interactive') return;
    await sleep(100);
  }
  throw new Error('page did not become ready');
}

function pactl(...args) {
  return execFileSync('pactl', args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
}

async function ensureNullSink() {
  const modules = pactl('list', 'short', 'modules');
  if (!modules.includes('module-null-sink') || !modules.includes('sink_name=josh_test')) {
    pactl('load-module', 'module-null-sink', 'sink_name=josh_test', 'sink_properties=device.description=Josh_Test_Sink');
  }
  pactl('set-default-sink', 'josh_test');
}

async function testLocalPage() {
  const { target, cdp } = await connectTarget(testOrigin + '/index.html');
  await waitReady(cdp);

  const previous = await evaluate(cdp, "({ls:localStorage.getItem('josh.persist'),cookie:document.cookie})");
  const firstBoot = previous.ls !== markerValue || !String(previous.cookie).includes('josh_cookie=' + markerValue);

  if (firstBoot) {
    await evaluate(cdp, "localStorage.setItem('josh.persist','" + markerValue + "');document.cookie='josh_cookie=" + markerValue + "; Max-Age=31536000; Path=/; SameSite=Lax';true");
    out('JOSHOS_BROWSER_PROFILE_PRIMED');
  } else {
    out('JOSHOS_BROWSER_PROFILE_OK');
  }

  const version = await waitForCdp();
  const browserCdp = new Cdp(version.webSocketDebuggerUrl);
  await browserCdp.open();
  await browserCdp.send('Browser.setDownloadBehavior', { behavior: 'allow', downloadPath: downloads, eventsEnabled: true });

  const downloadPath = path.join(downloads, downloadName);
  if (firstBoot) { try { fs.unlinkSync(downloadPath); } catch {} }
  await evaluate(cdp, "document.getElementById('download').click();true", { userGesture: true });
  for (let i = 0; i < 100 && !fs.existsSync(downloadPath); i++) await sleep(100);
  if (!fs.existsSync(downloadPath)) throw new Error('Chromium download did not appear');
  if (fs.readFileSync(downloadPath, 'utf8').trim() !== 'Josh OS browser download persistence test v1') throw new Error('download content mismatch');
  out('JOSHOS_BROWSER_DOWNLOAD_OK');

  await ensureNullSink();
  await evaluate(cdp, "(async()=>{const v=document.getElementById('media');v.volume=0.7;await v.play();return true})()", { userGesture: true, timeoutMs: 10000 });

  let streamSeen = false;
  for (let i = 0; i < 30; i++) {
    try {
      if (pactl('list', 'short', 'sink-inputs').trim()) { streamSeen = true; break; }
    } catch {}
    await sleep(100);
  }
  if (!streamSeen) throw new Error('Chromium produced no Pulse/PipeWire sink input');
  out('JOSHOS_BROWSER_AUDIO_STREAM_OK');

  await sleep(900);
  const media = await evaluate(cdp, "({time:document.getElementById('media').currentTime,paused:document.getElementById('media').paused,ready:document.getElementById('media').readyState})");
  if (!(media.time > 0.4) || media.ready < 2) throw new Error('WebM playback did not advance');
  out('JOSHOS_BROWSER_MEDIA_OK');

  const gpu = await evaluate(cdp, "(()=>{const c=document.createElement('canvas');const gl=c.getContext('webgl2')||c.getContext('webgl');if(!gl)return 'UNAVAILABLE';const ext=gl.getExtension('WEBGL_debug_renderer_info');return ext?gl.getParameter(ext.UNMASKED_RENDERER_WEBGL):gl.getParameter(gl.RENDERER)})()");
  out('JOSHOS_GPU_RENDERER ' + String(gpu).replace(/[\r\n]+/g, ' '));

  browserCdp.close();
  cdp.close();
  return { firstBoot, targetId: target.id };
}

async function testCrashRecovery(firstBoot) {
  if (!firstBoot) return;
  const before = (await getJson(cdpBase + '/json/list')).filter(t => String(t.url).startsWith(testOrigin));
  if (!before.length) throw new Error('local test tab missing before crash');

  let browserPid = null;
  for (const entry of fs.readdirSync('/proc')) {
    if (!/^\d+$/.test(entry)) continue;
    try {
      const cmd = fs.readFileSync('/proc/' + entry + '/cmdline').toString().replace(/\0/g, ' ');
      if (cmd.includes('chromium') &&
          cmd.includes('--user-data-dir=/home/josh/.config/chromium-josh-browser') &&
          !cmd.includes('--type=')) {
        browserPid = Number(entry);
        break;
      }
    } catch {}
  }
  if (!browserPid) throw new Error('could not identify main Chromium browser process');
  process.kill(browserPid, 'SIGKILL');

  let wentDown = false;
  for (let i = 0; i < 40; i++) {
    try { await getJson(cdpBase + '/json/version', 250); }
    catch { wentDown = true; break; }
    await sleep(100);
  }
  if (!wentDown) throw new Error('Chromium did not crash');

  await waitForCdp(30000);
  let restored = false;
  for (let i = 0; i < 80; i++) {
    const list = await getJson(cdpBase + '/json/list');
    if (list.some(t => String(t.url).startsWith(testOrigin))) { restored = true; break; }
    await sleep(250);
  }
  if (!restored) throw new Error('crashed Chromium session did not restore local test tab');

  const list = await getJson(cdpBase + '/json/list');
  const t = list.find(x => String(x.url).startsWith(testOrigin));
  const c = new Cdp(t.webSocketDebuggerUrl);
  await c.open();
  await c.send('Runtime.enable');
  const state = await evaluate(c, "({ls:localStorage.getItem('josh.persist'),cookie:document.cookie})");
  c.close();
  if (state.ls !== markerValue || !String(state.cookie).includes('josh_cookie=' + markerValue)) throw new Error('browser storage lost across crash relaunch');
  out('JOSHOS_BROWSER_CRASH_RECOVERY_OK');
}

try {
  await waitForCdp();
  const local = await testLocalPage();
  await testCrashRecovery(local.firstBoot);
  out('JOSHOS_BROWSER_ACCEPTANCE_OK');
  server.close();
  process.exit(0);
} catch (e) {
  out('JOSHOS_BROWSER_ACCEPTANCE_FAIL ' + String(e?.stack || e).replace(/[\r\n]+/g, ' '));
  server.close();
  process.exit(1);
}
