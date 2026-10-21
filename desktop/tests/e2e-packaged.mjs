// Automated e2e check for the packaged app (requires `npm run dist:dir` first).
// Run: node tests/e2e-packaged.mjs
// Covers: boot with seeded settings, dynamic ports, COOP/COEP, virtual config.json, graceful quit
import { spawn } from 'node:child_process';
import net from 'node:net';
import http from 'node:http';

import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const BASE = join(dirname(fileURLToPath(import.meta.url)), '..');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const fail = (msg) => { console.error('PACKAGED_FAIL:', msg); process.exit(1); };
setTimeout(() => fail('global 120s timeout'), 120000);

const httpReq = (url) => new Promise((resolve) => {
  const req = http.get(url, { timeout: 3000 }, (res) => {
    const headers = res.headers; res.resume();
    resolve({ status: res.statusCode, headers });
  });
  req.on('timeout', () => req.destroy());
  req.on('error', () => resolve(null));
});
const tcpOpen = (port) => new Promise((resolve) => {
  const s = net.connect({ host: '127.0.0.1', port });
  const done = (v) => { s.destroy(); resolve(v); };
  s.setTimeout(1500, () => done(false));
  s.once('connect', () => done(true));
  s.once('error', () => done(false));
});
const freePort = () => new Promise((resolve) => {
  const s = net.createServer(); s.listen(0, '127.0.0.1', () => { const p = s.address().port; s.close(() => resolve(p)); });
});
const cdpPort = await freePort();

const display = `:${100 + Math.floor(Math.random() * 800)}`;
const xvfb = spawn('Xvfb', [display, '-screen', '0', '1280x1024x24', '-nolisten', 'tcp'], { stdio: 'ignore' });
await sleep(1000);
const child = spawn(`${BASE}/dist/linux-unpacked/journey-client-desktop`,
  ['--no-sandbox', `--remote-debugging-port=${cdpPort}`], {
  env: { ...process.env, DISPLAY: display, XDG_CONFIG_HOME: `${BASE}/.tmp-run/xdg` },
  stdio: 'ignore',
});
process.on('exit', () => { try { child.kill('SIGKILL'); } catch {}; try { xvfb.kill('SIGKILL'); } catch {} });

let page = null;
for (let i = 0; i < 80 && !page; i++) {
  await sleep(500);
  try {
    const list = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
    page = list.find((t) => t.type === 'page' && t.url.includes('/web/index.html'));
  } catch { /* app booting */ }
}
if (!page) fail('game page never appeared (services or boot failed)');

const ws = new WebSocket(page.webSocketDebuggerUrl);
let nextId = 1;
const cdp = (method, params = {}) => new Promise((resolve, reject) => {
  const id = nextId++;
  const onMsg = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id === id) { ws.removeEventListener('message', onMsg); m.error ? reject(new Error(JSON.stringify(m.error))) : resolve(m.result); }
  };
  ws.addEventListener('message', onMsg);
  ws.send(JSON.stringify({ id, method, params }));
});
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
const run = (expr) => cdp('Runtime.evaluate', { expression: expr, awaitPromise: false, returnByValue: true });
const runAwaited = async (expr, ms) => {
  await run(`(() => { window.__awaited = { done: false }; Promise.resolve(${expr}).then((v) => { window.__awaited = { done: true, value: v }; }, (e) => { window.__awaited = { done: true, value: 'ERR ' + e.message }; }); })()`);
  for (let i = 0; i < Math.ceil(ms / 400); i++) {
    await sleep(400);
    const r = await run('window.__awaited');
    if (r.result.value && r.result.value.done) return r.result.value.value;
  }
  throw new Error('page expression never settled: ' + expr);
};

const webPort = await runAwaited('window.location.port', 5000);
const index = await httpReq(`http://127.0.0.1:${webPort}/web/index.html`);
if (!index || index.status !== 200) fail(`web-server not 200 on :${webPort}`);
const coop = (index.headers['cross-origin-opener-policy'] || '').trim();
const coep = (index.headers['cross-origin-embedder-policy'] || '').trim();
if (coop !== 'same-origin' || coep !== 'require-corp') fail(`COOP/COEP wrong: '${coop}'/'${coep}'`);
console.log(`OK web-server :${webPort} 200 + COOP/COEP`);

const stale = await httpReq(`http://127.0.0.1:${webPort}/web/config.json`);
if (stale && stale.status === 200) fail('physical /web/config.json served - should be excluded');
console.log('OK no physical /web/config.json');

const cfg = await runAwaited("fetch('./config.json').then(r => r.json())", 10000);
const expect = {
  ProxyIP: '127.0.0.1', MapleStoryServerIp: 'ms.example.com', MapleStoryServerPort: '7575',
  AssetsServerIP: '127.0.0.1', AssetsServerProtocol: 'ws',
};
for (const [k, v] of Object.entries(expect)) if (cfg[k] !== v) fail(`config.${k}: expected ${v}, got ${cfg[k]} (full: ${JSON.stringify(cfg)})`);
if (!/^\d+$/.test(String(cfg.ProxyPort))) fail('ProxyPort missing');
if (!/^\d+$/.test(String(cfg.AssetsServerPort))) fail('AssetsServerPort missing');
console.log('OK virtual config.json in game page:', JSON.stringify(cfg));

if (!(await tcpOpen(Number(cfg.ProxyPort)))) fail(`ws-proxy :${cfg.ProxyPort} not listening`);
if (!(await tcpOpen(Number(cfg.AssetsServerPort)))) fail(`assets-server :${cfg.AssetsServerPort} not listening`);
console.log('OK ws-proxy + assets-server listening on dynamic ports');

ws.close();
child.kill('SIGTERM');
let closed = false;
for (let i = 0; i < 45 && !closed; i++) { if (!(await tcpOpen(Number(webPort)))) closed = true; else await sleep(1000); }
if (!closed) fail('web-server still listening after app quit (cleanup broken)');
console.log('OK services stopped on quit');
console.log('PACKAGED_OK');
process.exit(0);
