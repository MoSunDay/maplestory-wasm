// 默认键位官方 v83 运行时冒烟（dc8f433 后）。
//
// 登录进图后逐键验证：
//   1. I/E/S/K 在各自官方默认落位打开对应窗口（按窗口落位采样像素，
//      错位类 bug 会因窗口开在别处而 FAIL），ESC 关闭后像素回落；
//   2. Q 走 msui_questlog_active 探针开合任务窗；
//   3. F1..F7 表情键：全部按下后不弹窗、不掉状态、不出系统通知
//      （修复前 F4 空绑、表情 4..7 被顶到 F5..F8）；
//   4. CTRL 攻击仍可用（9bfbf218 的原始目标，msattack_frame/stance 变化）。
//
// 用法（栈需在跑：html-server :8000、ws-proxy、assets-server、游戏服）：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/keymap-defaults-flow.mjs
//
// 可用 KEYMAP_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT 覆盖默认值。
// 键表本身的逐条正确性由 tools/verify/default-keymap.cpp 守门，本流程
// 只覆盖运行时分发链（GLFW→Keyboard→UIStateGame→窗口）。
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { sampleLuminance } from './layout-pixels.mjs';

const account = process.env.KEYMAP_ACCOUNT || 'fmflow1';
const password = process.env.KEYMAP_PASSWORD || 'fmflow1';
const cardIndex = Number(process.env.KEYMAP_CARD ?? 0);
const debugPort = Number(process.env.KEYMAP_DEBUG_PORT || 9283);
const pageUrl = process.env.KEYMAP_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const view = { width: 800, height: 600 };

const probe = (driver, name) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function enterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not initialize');
  }
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30)) {
    throw new Error('login UI did not activate');
  }
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45)) {
    throw new Error('world-select UI did not activate');
  }
  await driver.retryClick(510, 216,
    async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 45)) {
    throw new Error('character-select UI did not activate');
  }
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('char-select assets never idle');
  }

  // 2023 花名册：卡片 i 热区 x∈[100+120i,160+120i]、y∈[160,280]。
  const cardX = 130 + 120 * (cardIndex % 4);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(cardX, 210);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await probe(driver, 'msui_state') === uiState.game;
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
  await sleep(1500);
}

// 窗口落位（Configuration.h PointEntry 默认值）。band 覆盖标题带+首排槽位，
// 并向下放宽以吸收渲染管线的 VIEWYOFFSET 行移。
const windowRects = Object.freeze({
  I: { name: 'inv', x0: 308, y0: 158, x1: 368, y1: 268 },      // PosINV (300,150)
  E: { name: 'eqp', x0: 258, y0: 198, x1: 318, y1: 308 },      // PosEQINV (250,190)
  S: { name: 'sta', x0: 108, y0: 158, x1: 168, y1: 268 },      // PosSTATS (100,150)
  K: { name: 'skl', x0: 58, y0: 158, x1: 118, y1: 268 }        // PosSKILL (50,150)
});

async function windowRoundtrip(driver, report, letter, code, vk) {
  const rect = windowRects[letter];
  const before = (await sampleLuminance(driver, [rect], view))[rect.name];
  await driver.key(letter, code, vk);
  await sleep(700);
  const opened = (await sampleLuminance(driver, [rect], view))[rect.name];
  const delta = Math.round((opened - before) * 100) / 100;
  await driver.screenshot(`keymap-${letter.toLowerCase()}-open`);
  await driver.key('Escape', 'Escape', 27);
  await sleep(600);
  const closed = (await sampleLuminance(driver, [rect], view))[rect.name];
  const settled = Math.round(Math.abs(closed - before) * 100) / 100;
  const ok = Math.abs(delta) >= 18 && settled <= 15;
  report(ok, `${letter} 键窗口往返 @${rect.x0},${rect.y0}：Δopen=${delta}（需|Δ|≥18），关后回落=${settled}（需≤15）`);
  if (!ok) throw new Error(`window roundtrip failed for key ${letter}`);
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
let failed = false;
const report = (ok, message) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};

try {
  await driver.start();
  await driver.navigate(pageUrl);
  await enterGame(driver);
  const mapId = await probe(driver, 'msmap_id');
  console.log(`INFO  in-game map=${mapId}`);

  // 1) 窗口键往返（顺序无关，每键独立以全关状态为基线）。
  await windowRoundtrip(driver, report, 'I', 'KeyI', 73);
  await windowRoundtrip(driver, report, 'E', 'KeyE', 69);
  await windowRoundtrip(driver, report, 'S', 'KeyS', 83);
  await windowRoundtrip(driver, report, 'K', 'KeyK', 75);

  // 2) Q 任务窗（精确探针）。
  await driver.key('q', 'KeyQ', 81);
  const questOpened = await waitFor(async () => await probe(driver, 'msui_questlog_active') === 1, 8);
  await driver.key('Escape', 'Escape', 27);
  const questClosed = await waitFor(async () => await probe(driver, 'msui_questlog_active') === 0, 8);
  report(questOpened && questClosed, `Q 键任务窗开合（open=${questOpened} close=${questClosed}）`);
  if (!questOpened) throw new Error('Q did not open the quest log');

  // 3) F1..F7 表情键：不弹窗、不掉状态、不出系统通知。
  let faceOk = true;
  for (let n = 1; n <= 7; n += 1) {
    await driver.key(`F${n}`, `F${n}`, 111 + n);
    await sleep(250);
    const state = await probe(driver, 'msui_state');
    const notice = await probe(driver, 'msui_notice_active');
    const quest = await probe(driver, 'msui_questlog_active');
    if (state !== uiState.game || notice !== 0 || quest !== 0) {
      faceOk = false;
      report(false, `F${n} 后状态异常：state=${state} notice=${notice} quest=${quest}`);
    }
  }
  report(faceOk, 'F1..F7 表情键全部安全（无弹窗/无掉状态/无通知）');

  // 4) CTRL 攻击（9bfbf218 原始目标，不应被本次修复回退）。
  const frame0 = await probe(driver, 'msattack_frame');
  const stance0 = await probe(driver, 'msattack_stance');
  await driver.key('Control', 'ControlLeft', 17);
  const attacked = await waitFor(async () =>
    await probe(driver, 'msattack_frame') !== frame0 ||
    await probe(driver, 'msattack_stance') !== stance0, 4);
  report(attacked, `CTRL 攻击触发（frame ${frame0}→${await probe(driver, 'msattack_frame')}，stance ${stance0}→${await probe(driver, 'msattack_stance')}）`);

  await driver.screenshot('keymap-final');
} catch (error) {
  report(false, `flow aborted: ${error.message}`);
} finally {
  if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
  console.log(`artifacts: ${driver.artifactDir}`);
  await driver.stop();
}
if (failed) process.exitCode = 1;
