// Automated e2e check for the packaged app (requires `npm run dist:dir` first).
// Run: node tests/e2e-settings-flow.mjs
// Covers: first-run settings window, save flow, settings.json persistence, remote-proxy config injection
import { spawn } from 'node:child_process';
import net from 'node:net';
import fs from 'node:fs';
import http from 'node:http';

import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const BASE = join(dirname(fileURLToPath(import.meta.url)), '..');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const fail = (msg) => { console.error('SETTINGS_FLOW_FAIL:', msg); process.exit(1); };
setTimeout(() => fail('global 120s timeout'), 120000);
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
  env: { ...process.env, DISPLAY: display, XDG_CONFIG_HOME: `${BASE}/.tmp-run/xdg-fresh` },
  stdio: ['ignore', 'pipe', 'pipe'],
});
const logChunks = [];
child.stdout.on('data', (d) => logChunks.push(d));
child.stderr.on('data', (d) => logChunks.push(d));
process.on('exit', () => { try { child.kill('SIGKILL'); } catch {}; try { xvfb.kill('SIGKILL'); } catch {} });

const getPages = async () => {
  try {
    const list = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
    return list.filter((t) => t.type === 'page');
  } catch { return []; }
};

// 1. first run: settings window must appear (file:// settings-ui), no game page
let settingsPage = null;
for (let i = 0; i < 60 && !settingsPage; i++) {
  await sleep(500);
  settingsPage = (await getPages()).find((t) => t.url.includes('settings-ui'));
}
if (!settingsPage) fail('settings window never appeared on first run');
console.log('OK settings window shown on first run');
const gameLeak = (await getPages()).find((t) => t.url.includes('/web/index.html'));
if (gameLeak) fail('game window opened before settings were saved');

const ws = new WebSocket(settingsPage.webSocketDebuggerUrl);
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

// 2. fill server url (after prefill settles) and click save
await sleep(1500);
await run(`(() => {
  document.getElementById('server-url').value = 'ws://proxy.example.com:9000';
  document.getElementById('server-url').dispatchEvent(new Event('input'));
})()`);
const saveDisabled = (await run("document.getElementById('save').disabled")).result.value;
if (saveDisabled) fail('save button disabled despite local assets present');
await run("document.getElementById('save').click()");
console.log('OK saved ws://proxy.example.com:9000 via settings UI');

// 3. settings.json persisted
const settingsPath = `${BASE}/.tmp-run/xdg-fresh/JourneyClient/settings.json`;
let persisted = null;
for (let i = 0; i < 20 && !persisted; i++) { try { persisted = fs.readFileSync(settingsPath, 'utf8'); } catch { await sleep(500); } }
if (!persisted || !persisted.includes('ws://proxy.example.com:9000')) fail('settings.json not persisted: ' + persisted);
console.log('OK settings.json persisted:', persisted.replace(/\s+/g, ' ').trim());

// 4. settings window closes and game window appears with remote-proxy config
let gamePage = null;
for (let i = 0; i < 80 && !gamePage; i++) {
  await sleep(500);
  const pages = await getPages();
  gamePage = pages.find((t) => t.url.includes('/web/index.html'));
}
if (!gamePage) fail('game window never appeared after save');
let settingsGone = false;
for (let i = 0; i < 10 && !settingsGone; i++) { settingsGone = !(await getPages()).some((t) => t.url.includes('settings-ui')); if (!settingsGone) await sleep(500); }
console.log('OK game window appeared; settings window closed =', settingsGone);
const ws2 = new WebSocket(gamePage.webSocketDebuggerUrl);
let id2 = 100;
const cdp2 = (method, params = {}) => new Promise((resolve, reject) => {
  const id = id2++;
  const onMsg = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id === id) { ws2.removeEventListener('message', onMsg); m.error ? reject(new Error(JSON.stringify(m.error))) : resolve(m.result); }
  };
  ws2.addEventListener('message', onMsg);
  ws2.send(JSON.stringify({ id, method, params }));
});
await new Promise((res, rej) => { ws2.addEventListener('open', res); ws2.addEventListener('error', rej); });
const run2 = (expr) => cdp2('Runtime.evaluate', { expression: expr, awaitPromise: false, returnByValue: true });
await run2(`(() => { window.__cfg = null; fetch('./config.json').then(r => r.json()).then(v => { window.__cfg = v; }); })()`);
let cfg = null;
for (let i = 0; i < 25 && !cfg; i++) { await sleep(400); cfg = (await run2('window.__cfg')).result.value; }
if (!cfg) fail('game page config fetch never settled');
if (cfg.ProxyIP !== 'proxy.example.com' || cfg.ProxyPort !== '9000') fail('remote proxy config wrong: ' + JSON.stringify(cfg));
if (cfg.MapleStoryServerIp !== 'proxy.example.com' || cfg.MapleStoryServerPort !== '8484') fail('remote target wrong: ' + JSON.stringify(cfg));
if (cfg.AssetsServerIP !== '127.0.0.1' || cfg.AssetsServerProtocol !== 'ws') fail('local assets config wrong: ' + JSON.stringify(cfg));
console.log('OK remote-proxy virtual config:', JSON.stringify(cfg));
console.log('SETTINGS_FLOW_OK');
process.exit(0);
