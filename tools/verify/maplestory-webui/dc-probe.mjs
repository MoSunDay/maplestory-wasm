// 双击选卡探针：修复后二进制（ImeBridge blur + #ime-input pointer-events:none）
// 在角色选择界面双击卡 1 无 CHAR_SELECT 发出。本探针复刻 gm-shop-flow 的
// 登录序列，停在选择卡现场 dump：IME 桥状态、activeElement、卡面命中元素、
// msui_state/msui_enabled/mscharlist_* 全景，然后以不同节奏双击并逐秒采样
// ui_state 变化，定位点击是否到达游戏、双击是否被判进。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser node dc-probe.mjs
//   可选 DC_PROBE_CARD=1 DC_PROBE_GAP=250（两次点击间隔 ms）

import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.DC_PROBE_DEBUG_PORT || 9251);
const pageUrl = process.env.DC_PROBE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.DC_PROBE_ACCOUNT || 'test1';
const password = process.env.DC_PROBE_PASSWORD || 'test1';
const cardIndex = Number(process.env.DC_PROBE_CARD || 1);
const gapMs = Number(process.env.DC_PROBE_GAP || 250);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const ccall = (driver, name) => driver.evaluate(`Module.ccall('${name}','number',[],[])`);

async function waitFor(driver, predicate, timeoutSeconds, label) {
  for (let i = 0; i < timeoutSeconds; i += 1) {
    if (await predicate()) return true;
    await sleep(1000);
  }
  throw new Error(`Timed out: ${label}`);
}

async function scene(driver, tag) {
  const v = {};
  for (const n of ['msui_state', 'msui_enabled', 'mscharlist_phase',
    'mscharlist_character_index', 'mscharlist_preview_total',
    'mscharlist_remaining_bytes', 'msstage_loading'])
    v[n] = await ccall(driver, n);
  const dom = JSON.parse(await driver.evaluate(`(() => {
    const input = document.getElementById('ime-input');
    const canvas = document.getElementById('canvas').getBoundingClientRect();
    const cx = canvas.left + ${105 + 120 * cardIndex + 25} * canvas.width / 800;
    const cy = canvas.top + 210 * canvas.height / 600;
    const hit = document.elementFromPoint(cx, cy);
    return JSON.stringify({ active: document.activeElement ? (document.activeElement.id || document.activeElement.tagName) : null,
      imeActive: !!(window.MapleWasmIME && window.MapleWasmIME.active),
      hitAtCard: hit ? (hit.id || hit.tagName) : null });
  })()`));
  console.log(`[${tag}]`, JSON.stringify(v), JSON.stringify(dom));
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
try {
  await driver.start();
  await driver.navigate(pageUrl);
  await waitFor(driver, async () =>
    await driver.evaluate("typeof Module !== 'undefined' && Module.calledRun === true") &&
    await ccall(driver, 'msui_state') === uiState.login, 90, 'login UI active');
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  await waitFor(driver, async () => await ccall(driver, 'msui_state') === uiState.world,
    45, 'world select');
  await driver.click(510, 216);
  await waitFor(driver, async () => await ccall(driver, 'msui_state') === uiState.characters,
    45, 'character select');
  for (let i = 0; i < 60 && await ccall(driver, 'mscharlist_remaining_bytes') > 0; i += 1)
    await sleep(1000);
  await driver.screenshot('dp0-charselect');
  await scene(driver, 'charselect');

  const x = 105 + 120 * cardIndex + 25, y = 210;
  const blocking = () => driver.evaluate(
    "(window.MapleAssetLoading && window.MapleAssetLoading.isBlocking) ? window.MapleAssetLoading.isBlocking() : 'n/a'");
  console.log('[blocking before]', await blocking());
  await driver.click(x, y);
  console.log('[blocking mid]', await blocking());
  await sleep(Math.min(gapMs, 80));
  await scene(driver, 'after-first-click');
  console.log('[blocking after-1st]', await blocking());
  await driver.click(x, y);
  console.log('[blocking after-2nd]', await blocking());
  await scene(driver, 'after-second-click');
  for (let i = 0; i < 15; i += 1) {
    const st = await ccall(driver, 'msui_state');
    if (st !== uiState.characters) { console.log(`[ui] -> ${st} after ${(i + 1)}s`); break; }
    await sleep(1000);
  }
  await driver.screenshot('dp1-after-double-click');
  await scene(driver, 'final');
  console.log('PASS  dc probe finished');
} finally {
  await driver.stop();
}
