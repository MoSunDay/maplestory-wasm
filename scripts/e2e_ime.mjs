// E2E test for the browser-IME input path (hidden textarea bridge + UTF-8
// rendering). Drives a headless Chrome over CDP against a locally running
// web stack and asserts the full round trip: focus -> IME bridge active,
// composition commit -> C++ field state echoed back, byte-limit truncation,
// CJK glyph rendering (pixel diff), codepoint backspace, password-field
// exclusion and blur deactivation.
//
// Prerequisites:
//   1. Client built:            ./scripts/build_wasm.sh
//   2. Web stack running:       web-server on :8000 (assets-server :8765,
//      ws-proxy :8080); the login screen must be reachable at E2E_URL.
//
// Run:   node scripts/e2e_ime.mjs
// Env overrides:
//   CHROME_BIN      Chrome binary (default: google-chrome)
//   E2E_URL         page under test (default: http://127.0.0.1:8000/web/index.html)
//   E2E_DEBUG_PORT  Chrome remote-debugging port (default: 9224)
//
// Exit code 0 = all 12 checks passed.

import { spawn } from 'child_process';
import fs from 'fs';
import os from 'os';
import path from 'path';

const CHROME_BIN = process.env.CHROME_BIN || 'google-chrome';
const PAGE_URL = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const DEBUG_PORT = process.env.E2E_DEBUG_PORT || '9224';
// Screenshots are diagnostic evidence; keep them out of the repo tree.
const ARTIFACT_DIR = fs.mkdtempSync(path.join(os.tmpdir(), 'ime-e2e-'));

const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const chrome = spawn(CHROME_BIN, ['--headless=new', '--no-sandbox', '--enable-unsafe-swiftshader',
  `--remote-debugging-port=${DEBUG_PORT}`, '--window-size=820,640', '--mute-audio', 'about:blank'],
  { stdio: 'ignore' });
process.on('exit', () => { try { chrome.kill('SIGKILL'); } catch (e) {} });
for (let i = 0; i < 30; i++) {
  await sleep(500);
  try { await fetch(`http://127.0.0.1:${DEBUG_PORT}/json/version`); break; } catch (e) {}
}
const targets = await (await fetch(`http://127.0.0.1:${DEBUG_PORT}/json/list`)).json();
const page = targets.find(t => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
let id = 0; const pending = new Map();
const logs = []; const errors = [];
function send(method, params = {}) {
  return new Promise((resolve, reject) => {
    const mid = ++id; pending.set(mid, { resolve, reject });
    ws.send(JSON.stringify({ id: mid, method, params }));
  });
}
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) {
    const { resolve, reject } = pending.get(m.id);
    pending.delete(m.id);
    m.error ? reject(new Error(JSON.stringify(m.error))) : resolve(m.result);
    return;
  }
  if (m.method === 'Runtime.consoleAPICalled')
    logs.push(m.params.args.map(a => a.value ?? a.description ?? '').join(' '));
  if (m.method === 'Runtime.exceptionThrown')
    errors.push(JSON.stringify(m.params.exceptionDetails).slice(0, 300));
};
await new Promise(r => { ws.onopen = r; });
await send('Runtime.enable');
await send('Page.enable');

const js = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result.value;

let pass = 0, fail = 0;
function check(name, cond, detail = '') {
  if (cond) { pass++; console.log(`PASS  ${name}`); }
  else { fail++; console.log(`FAIL  ${name} ${detail}`); }
}

await send('Page.navigate', { url: PAGE_URL });
for (let i = 0; i < 40; i++) {
  await sleep(1000);
  try { if ((await js(`typeof Module !== 'undefined' && Module.LazyFS ? 'ready' : 'wait'`)) === 'ready') break; } catch (e) {}
}
console.log('module ready, waiting for login screen...');
await sleep(12000);
check('glue: Module.ccall available', await js(`typeof Module.ccall === 'function'`) === true);
check('glue: msime_input exported', await js(`typeof Module._msime_input === 'function'`) === true);
check('glue: MapleWasmIME attached', await js(`!!window.MapleWasmIME`) === true);
check('glue: textarea exists', await js(`!!document.getElementById('ime-input')`) === true);

const click = async (gx, gy) => js(`(() => {
  const c = document.getElementById('canvas');
  const r = c.getBoundingClientRect();
  // The game canvas renders at a fixed 800x600; scale click targets from
  // game space to whatever size the browser laid the element out at.
  const mk = (x, y, type, buttons) => c.dispatchEvent(new MouseEvent(type, { bubbles: true, cancelable: true,
    clientX: r.left + x * r.width / 800, clientY: r.top + y * r.height / 600, button: 0, buttons, view: window }));
  mk(${gx}, ${gy}, 'mousemove', 0); mk(${gx}, ${gy}, 'mousedown', 1); mk(${gx}, ${gy}, 'mouseup', 0);
  return 'ok'; })()`);

const shot = async () => {
  const r = await send('Page.captureScreenshot', { format: 'png', clip: { x: 240, y: 190, width: 320, height: 110, scale: 1 } });
  return r.data;
};

// --- 1. focus the account field ---
await click(330, 258);
await sleep(600);
check('focus: IME bridge active on account field', await js(`window.MapleWasmIME.active`) === true);
check('focus: textarea focused', await js(`document.activeElement && document.activeElement.id === 'ime-input'`) === true);
const full = await send('Page.captureScreenshot', { format: 'png' });
fs.writeFileSync(path.join(ARTIFACT_DIR, 'e2e_before_click.png'), Buffer.from(full.data, 'base64'));
const baseline = await shot();
fs.writeFileSync(path.join(ARTIFACT_DIR, 'e2e_field_before.png'), Buffer.from(baseline, 'base64'));

// --- 2. simulate IME composition committing Chinese text ---
await js(`(() => {
  const ta = document.getElementById('ime-input');
  ta.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
  ta.value = '中文测试';
  ta.dispatchEvent(new InputEvent('input', { bubbles: true, data: '中文测试', isComposing: true }));
  ta.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: '中文测试' }));
  return 'sent'; })()`);
await sleep(800);
let taValue = await js(`document.getElementById('ime-input').value`);
console.log(`  textarea after commit: "${taValue}"`);
check('ime: Chinese text accepted by game (echoed back)', taValue.startsWith('中文测试') || taValue.includes('中文'), `got "${taValue}"`);

// --- 3. byte limit: account limit is 12 bytes; add more, expect truncation ---
await js(`(() => {
  const ta = document.getElementById('ime-input');
  ta.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
  ta.value = '中文测试甲乙丙丁';
  ta.dispatchEvent(new InputEvent('input', { bubbles: true, isComposing: true }));
  ta.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
  return 'sent'; })()`);
await sleep(800);
taValue = await js(`document.getElementById('ime-input').value`);
console.log(`  textarea after over-limit commit: "${taValue}" (${taValue.length} u16 units)`);
const enc = new TextEncoder();
check('limit: text truncated to 12 bytes', enc.encode(taValue).length <= 12 && taValue.length > 0, `bytes=${enc.encode(taValue).length}`);

// --- 4. glyphs rendered: field area pixels changed vs baseline ---
const afterText = await shot();
fs.writeFileSync(path.join(ARTIFACT_DIR, 'e2e_field_after.png'), Buffer.from(afterText, 'base64'));
check('render: field pixels changed after CJK input', baseline !== afterText);

// --- 5. backspace via real key event ---
console.log('  activeElement before backspace:', await js(`document.activeElement ? document.activeElement.id : 'none'`));
const beforeLen = (await js(`document.getElementById('ime-input').value`)).length;
await send('Input.dispatchKeyEvent', { type: 'rawKeyDown', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8, nativeVirtualKeyCode: 8 });
await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8, nativeVirtualKeyCode: 8 });
await sleep(500);
let afterLen = (await js(`document.getElementById('ime-input').value`)).length;
console.log(`  CDP backspace: ${beforeLen} -> ${afterLen}`);
if (afterLen !== beforeLen - 1) {
  // Fallback probe: dispatch a synthetic keydown straight at the textarea to
  // verify the handler logic itself, independent of CDP routing.
  await js(`(() => {
    const ta = document.getElementById('ime-input');
    ta.dispatchEvent(new KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: 'Backspace', keyCode: 8 }));
    return 'dispatched'; })()`);
  await sleep(500);
  afterLen = (await js(`document.getElementById('ime-input').value`)).length;
  console.log(`  synthetic backspace: -> ${afterLen}`);
}
check('edit: backspace removed one codepoint', afterLen === beforeLen - 1, `before=${beforeLen} after=${afterLen}`);

// --- 6. Tab moves to password field; IME bridge must stay OFF there ---
await send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Tab', code: 'Tab', windowsVirtualKeyCode: 9, nativeVirtualKeyCode: 9 });
await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Tab', code: 'Tab', windowsVirtualKeyCode: 9, nativeVirtualKeyCode: 9 });
await sleep(600);
check('password: IME bridge inactive on crypted field', await js(`window.MapleWasmIME.active`) === false);

// --- 7. click away: blur path ---
await click(700, 500);
await sleep(500);
check('blur: bridge inactive after defocus', await js(`window.MapleWasmIME.active`) === false);

console.log(`\nRESULT: ${pass} passed, ${fail} failed`);
console.log(`artifacts: ${ARTIFACT_DIR}`);
console.log('--- console logs ---');
logs.forEach(l => console.log('  ' + l.slice(0, 160)));
if (errors.length) console.log('JS errors:', errors.slice(0, 3));
const crash = logs.filter(l => /abort|exception|RuntimeError/i.test(l));
if (crash.length) console.log('suspicious logs:', crash.slice(0, 3));
process.exit(fail === 0 ? 0 : 1);
