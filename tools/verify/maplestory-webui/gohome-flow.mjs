// /gohome 快捷回自由市场(910000000) 端到端验证。
//
// 从自由市场房间 910000001 登录（该图 fieldLimit 含 CANNOTMIGRATE，
// 旧版 @goto/@warp 会被拒绝），在聊天框发送 "/gohome"，断言：
//   1. 服务端识别 slash 命令（不是普通聊天，也不会被 / 前缀吞掉）；
//   2. 地图切回 910000000；
//   3. 落点为 in01 传送门附近（x≈1332），而不是随机出生点。
//
// 前置（每次运行前执行一次；测试账号非 GM）：
//   update characters c join accounts a on a.id=c.accountid
//     set c.map=910000001, c.spawnpoint=0
//     where a.name='fmflow1' and c.name='fmflow1Hero';
//   update accounts set loggedin=0 where name='fmflow1';
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/gohome-flow.mjs
//
// 可用 GOHOME_ACCOUNT/PASSWORD/CARD/START_MAP/URL/DEBUG_PORT 覆盖默认值。
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const account = process.env.GOHOME_ACCOUNT || 'fmflow1';
const password = process.env.GOHOME_PASSWORD || 'fmflow1';
const cardIndex = Number(process.env.GOHOME_CARD ?? 0);
const startMap = Number(process.env.GOHOME_START_MAP ?? 910000001);
const homeMap = 910000000;
const homeX = 1332;
const debugPort = Number(process.env.GOHOME_DEBUG_PORT || 9271);
const pageUrl = process.env.GOHOME_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });

const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const uiStateIs = (driver, state) => async () => await probe(driver, 'msui_state') === state;

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function enterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not initialize');
  }
  if (!await waitFor(uiStateIs(driver, uiState.login), 30)) throw new Error('login UI did not activate');
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45)) throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45)) throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');

  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
  await sleep(1500);
}

async function sendChat(driver, message) {
  await driver.key('Enter', 'Enter', 13);
  await sleep(150);
  await driver.typeAscii(message);
  await sleep(200);
  await driver.key('Enter', 'Enter', 13);
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

  const before = await probe(driver, 'msmap_id');
  report(before === startMap, `起始地图 = ${startMap}（实测 ${before}）`);
  if (before !== startMap) throw new Error(`请先按文件头 SQL 把角色放到 ${startMap}`);

  await sendChat(driver, '/gohome');
  const warped = await waitFor(async () => await probe(driver, 'msmap_id') === homeMap, 15);
  const after = await probe(driver, 'msmap_id');
  report(warped, `/gohome 回到 ${homeMap}（实测 ${after}）`);
  if (!warped) throw new Error('/gohome did not warp');

  await sleep(1500);
  const x = await probe(driver, 'msplayer_x');
  const y = await probe(driver, 'msplayer_y');
  report(Math.abs(x - homeX) <= 80 && y >= -30 && y <= 60,
    `落点 in01 附近（x=${x} y=${y}，期望 x≈${homeX} y∈[-30,60]）`);
  await driver.screenshot('gohome-after');
} catch (error) {
  report(false, `flow aborted: ${error.message}`);
} finally {
  if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
  console.log(`artifacts: ${driver.artifactDir}`);
  await driver.stop();
}
if (failed) process.exitCode = 1;
