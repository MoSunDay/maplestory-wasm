// 自由市场(910000000) 079MAX4 复刻 UI 确认流程。
//
// 全流程用真实交互验证 1:1 复刻：妮娜 9270052(x=34) Space 对话断言、弗兰德里
// 9030000(x=1590) 邻近取证、up00→up01→up02→up03 楼梯传送链
// （店门层 y=-1 →-284→-568→-851 四层）、dn00..dn03 下行链、in01↔out00 房间往返
// (910000001)。每站 readPixels 取证：16×10 亮度 ASCII + 色相桶（ChewChew 岛
// 风格蓝绿主导）。传送门坐标取自 079 版 wz XML（与服务端 910000000.img.xml
// 一致：up00(1227,-3)&up01(1269,-284)&up02(1313,-569)&up03(1354,-851)；
// dn00(2142,-855)..dn03(2281,-5)；in01(1332,-4)；房间 out00(790,35) pt=2）。
//
// 关键交互约定（本客户端）：
//   * NPC 对话 = Space（KeyAction::NPCCHAT, talk_to_nearest ±120px），不是点击；
//   * 所有门都靠站上门点按 Up 触发：本客户端 check_portals() 只在 Up 按下
//     时调用，pt=2 的 touch 门不会自动触发（进房间后不动不会被弹回）；
//   * 角色登录位置由 characters.spawnpoint（门索引）恢复，跑前必须重置
//     （见下方前置条件），否则断言从上次断点开始。
//
// 前置条件（每个新会话跑前执行一次）：
//   update accounts set loggedin=0 where name='fmflow1';
//   update characters set map=910000000, spawnpoint=0 where accountid=42;
//   （accountid=42 是 fmflow1；spawnpoint=0 → sp 门 (-128,-75)，落到地面）
//   fmflow1 角色必须带 Space/Alt/Ctrl 键位（create_accounts.py 已默认写入：
//   key57=5/54 NPCCHAT、key56=5/53 JUMP、key29=5/52 ATTACK），否则服务端
//   KEYMAP 包会把键位表清空，Space/Alt 全部失效。
//   弗兰德里 9030000 在本服是仓库 NPC（storebank），Space 会打开仓库窗口且
//   全局 Escape 名单不含 STORAGE，故只做邻近截图取证、不按 Space。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   FM_FLOW_ACCOUNT=fmflow1 FM_FLOW_PASSWORD=fmflow1 FM_FLOW_CARD=0 \
//   node tools/verify/maplestory-webui/fm-map-flow.mjs
// 环境变量：FM_FLOW_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT（默认 test1/test1/
// card1/http://127.0.0.1:8000/web/index.html/9251）。

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.FM_FLOW_DEBUG_PORT || 9251);
const pageUrl = process.env.FM_FLOW_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.FM_FLOW_ACCOUNT || 'test1';
const password = process.env.FM_FLOW_PASSWORD || 'test1';
const cardIndex = Number(process.env.FM_FLOW_CARD ?? 1);

const HUB = 910000000, ROOM = 910000001;
const NINA_X = 34, FRED_X = 1590, IN01_X = 1332, ROOM_OUT00_X = 790, HUB_RETURN_X = 1332;
const UP = [1227, 1269, 1313], DN = [2142, 2195, 2247];
const FLOORS = { ground: [-30, 60], platform: [-2, 0], f2: [-330, -240], f3: [-610, -530], f4: [-900, -810] };
// 地面 y=23 与 L4 平台 y=-1 之间有 24px 台阶（x=1179 竖墙 fh204），需跳跃上台。
const LEDGE_X = 1160;
const ROOM_FLOOR = [30, 40]; // 房间 910000001 地面 y=34，out00(790,35)

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };
const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const playerAt = async driver => ({
  x: await probe(driver, 'msplayer_x'), y: await probe(driver, 'msplayer_y'),
  sx: await probe(driver, 'msplayer_screen_x'), sy: await probe(driver, 'msplayer_screen_y')
});
const uiStateIs = (driver, state) => async () => await probe(driver, 'msui_state') === state;
const inRange = (v, range) => v >= range[0] && v <= range[1];

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function evaluateAwait(driver, expression) {
  const result = await driver.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (result.exceptionDetails) throw new Error(`page evaluation failed: ${result.exceptionDetails.text}`);
  return result.result.value;
}

// 场景取证：readPixels 全画面 → 平均 RGB、色相桶占比、16×10 亮度 ASCII。
const forensicExpression = `(() => new Promise(resolve => {
  requestAnimationFrame(() => {
    const canvas = document.getElementById('canvas');
    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
    if (!gl) { resolve({ error: 'no WebGL' }); return; }
    const x0 = Math.round(10 * canvas.width / 800), x1 = Math.round(790 * canvas.width / 800);
    const y0 = Math.round(30 * canvas.height / 600), y1 = Math.round(560 * canvas.height / 600);
    const w = x1 - x0, h = y1 - y0, pixels = new Uint8Array(w * h * 4);
    const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.readPixels(x0, canvas.height - y1, w, h, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
    let r = 0, g = 0, b = 0, n = 0, teal = 0, warm = 0, green = 0, dark = 0;
    const cols = 16, rows = 10, grid = new Array(cols * rows).fill(0), count = new Array(cols * rows).fill(0);
    for (let yy = 0; yy < h; yy += 2) for (let xx = 0; xx < w; xx += 2) {
      const o = (yy * w + xx) * 4, pr = pixels[o], pg = pixels[o + 1], pb = pixels[o + 2];
      r += pr; g += pg; b += pb; n += 1;
      if (pg > 100 && pb > 120 && pb > pr + 30) teal += 1;
      if (pr > 150 && pr > pg && pg > pb) warm += 1;
      if (pg > 120 && pg > pr && pg > pb) green += 1;
      if (pr < 40 && pg < 40 && pb < 40) dark += 1;
      const cx = Math.min(cols - 1, Math.floor(xx / w * cols));
      const cy = Math.min(rows - 1, Math.floor(yy / h * rows));
      grid[cy * cols + cx] += (pr + pg + pb) / 3; count[cy * cols + cx] += 1;
    }
    const ascii = [];
    for (let cy = 0; cy < rows; cy += 1) {
      let line = '';
      for (let cx = 0; cx < cols; cx += 1) {
        const v = grid[cy * cols + cx] / Math.max(1, count[cy * cols + cx]);
        line += ' .:-=+*#%@'[Math.min(9, Math.floor(v / 25.6))];
      }
      ascii.push(line);
    }
    resolve({ mean: [Math.round(r / n), Math.round(g / n), Math.round(b / n)],
      teal: +(teal / n).toFixed(3), warm: +(warm / n).toFixed(3),
      green: +(green / n).toFixed(3), dark: +(dark / n).toFixed(3), ascii });
  });
}))()`;

async function forensic(driver, label) {
  await sleep(250);
  const data = await evaluateAwait(driver, forensicExpression);
  if (data.error) { report(false, `${label}: ${data.error}`); return data; }
  console.log(`INFO  ${label} mean=rgb(${data.mean}) 蓝绿=${data.teal} 暖=${data.warm} 绿=${data.green} 暗=${data.dark}`);
  for (const line of data.ascii) console.log(`      ${line}`);
  return data;
}

async function settleShot(driver, name, label) {
  if (!await waitFor(() => assetsIdle(driver), 90)) throw new Error(`assets not idle before ${name}`);
  await sleep(2500);
  await driver.screenshot(name);
  return forensic(driver, label || name);
}

// 走向目标 x：500ms 小步 + 跳(Alt)解卡；超时/连续卡死直接抛错（不静默继续）。
async function walkTowards(driver, targetX, maxSeconds = 90) {
  const start = Date.now();
  let lastX = null, stuckCount = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const { x } = await playerAt(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuckCount += 1;
      if (stuckCount >= 3) {
        await driver.key('Alt', 'AltLeft', 18); // 原地跳（不会触发送门）
        await sleep(650);
        const after = await playerAt(driver);
        if (Math.abs(after.x - x) < 2 && stuckCount >= 6) {
          throw new Error(`walk stuck at x=${x} target=${targetX}`);
        }
      }
    } else stuckCount = 0;
    lastX = x;
    const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  const finalX = (await playerAt(driver)).x;
  if (Math.abs(finalX - targetX) > 10) throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

// 从地面(y=23)跳上 L4 平台(y=-1)：助跑后按 Alt 并持续向右，直到落地稳定在平台层。
async function jumpOntoPlatform(driver) {
  await walkTowards(driver, LEDGE_X, 60);
  await driver.keyHold('ArrowRight', 'ArrowRight', 39, 120); // 助跑
  await driver.key('Alt', 'AltLeft', 18);
  let stable = 0, lastY = null;
  for (let i = 0; i < 30; i += 1) {
    await driver.keyHold('ArrowRight', 'ArrowRight', 39, 60);
    await sleep(60);
    const { y } = await playerAt(driver);
    if (y === lastY) stable += 1; else stable = 0;
    lastY = y;
    if (stable >= 3) break;
  }
  const pos = await playerAt(driver);
  const ok = inRange(pos.y, FLOORS.platform);
  report(ok, `跳上 L4 平台（x=${pos.x} y=${pos.y}，期望 y∈[${FLOORS.platform}]）`);
  if (!ok) throw new Error(`未能跳上平台: x=${pos.x} y=${pos.y}`);
  return pos;
}

// Space 触发最近的 NPC 对话（±120px），用 msnpc_active 探针断言，Escape 关闭并复核。
async function npcTalk(driver, npcX, name) {
  const finalX = await walkTowards(driver, npcX, 120);
  if (Math.abs(finalX - npcX) > 100) throw new Error(`${name}: 走位失败（x=${finalX}）`);
  await sleep(500);
  await driver.key('Space', 'Space', 32);
  const opened = await waitFor(async () => await probe(driver, 'msnpc_active') === 1, 10);
  report(opened, `${name}: 对话弹出（msnpc_active=1）`);
  await driver.key('Escape', 'Escape', 27);
  await sleep(900);
  const closed = await waitFor(async () => await probe(driver, 'msnpc_active') === 0, 6);
  report(closed, `${name}: Escape 关闭对话`);
  if (!closed) throw new Error(`${name}: 对话未能关闭，后续走位会被 UI 吞键`);
}

// 传送门 pt=1：站上门点按 Up，断言落到目标层（同图 y 区间）。
// WARPCD≈0.8s，刚落地时首次 Up 可能被冷却吞掉，故最多重试 3 次。
async function portalUp(driver, portalX, srcRange, dstRange, label) {
  const before = await playerAt(driver);
  if (Math.abs(before.x - portalX) > 22 || !inRange(before.y, srcRange)) {
    throw new Error(`${label}: 传送前位置异常 x=${before.x} y=${before.y}（期望 x≈${portalX} y∈[${srcRange}]）`);
  }
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 400);
    if (await waitFor(async () => inRange((await playerAt(driver)).y, dstRange), 5)) break;
  }
  const after = await playerAt(driver);
  const ok = inRange(after.y, dstRange);
  report(ok, `${label}: 传送后 y=${after.y} x=${after.x}（期望 y∈[${dstRange}]）`);
  return ok;
}

// pt=1 进图门：断言地图切换。
async function portalUpToMap(driver, portalX, srcRange, targetMap, label) {
  const before = await playerAt(driver);
  if (Math.abs(before.x - portalX) > 22 || !inRange(before.y, srcRange)) {
    throw new Error(`${label}: 传送前位置异常 x=${before.x} y=${before.y}`);
  }
  let map = null;
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 400);
    if (await waitFor(async () => await probe(driver, 'msmap_id') === targetMap, 5)) break;
  }
  map = await probe(driver, 'msmap_id');
  report(map === targetMap, `${label}: 进入地图 ${targetMap}（实测 ${map}）`);
  return map === targetMap;
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
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
      entered = await uiStateIs(driver, uiState.game)(); if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    report(await probe(driver, 'msmap_id') === HUB,
      `出生地图 = 910000000（实测 ${await probe(driver, 'msmap_id')}）`);
    const entry = await playerAt(driver);
    report(entry.x >= -180 && entry.x <= 80 && inRange(entry.y, FLOORS.ground),
      `出生点落在 sp 区（x=${entry.x} y=${entry.y}，期望 x∈[-180,80] 地面层）`);
    await settleShot(driver, 'fm00-entry', '入口层（左）');

    // 妮娜 Space 对话（脚本 NPC → UINpcTalk）
    await npcTalk(driver, NINA_X, '妮娜9270052');

    // 跳上 L4 平台（up00/in01/dn03 都在 y=-1 的店门层）
    await jumpOntoPlatform(driver);

    // 弗兰德里：邻近截图取证（仓库 NPC，不按 Space）
    await walkTowards(driver, FRED_X, 150);
    const fred = await playerAt(driver);
    report(Math.abs(fred.x - FRED_X) <= 40, `弗兰德里9030000: 邻近（x=${fred.x}）`);
    await settleShot(driver, 'fm01-ground-right', '店门层右段（店门+弗兰德里）');

    // up 链：地面 → 二层 → 三层 → 四层
    await walkTowards(driver, UP[0], 90);
    if (await portalUp(driver, UP[0], FLOORS.platform, FLOORS.f2, 'up00→up01'))
      await settleShot(driver, 'fm02-floor2', '二层（in07..in12）');
    await walkTowards(driver, UP[1], 30);
    if (await portalUp(driver, UP[1], FLOORS.f2, FLOORS.f3, 'up01→up02'))
      await settleShot(driver, 'fm03-floor3', '三层（in13..in17）');
    await walkTowards(driver, UP[2], 30);
    if (await portalUp(driver, UP[2], FLOORS.f3, FLOORS.f4, 'up02→up03'))
      await settleShot(driver, 'fm04-floor4', '四层（in18..in22）');

    // dn 链：四层 → 三层 → 二层 → 地面
    await walkTowards(driver, DN[0], 90);
    if (await portalUp(driver, DN[0], FLOORS.f4, FLOORS.f3, 'dn00→dn01'))
      await walkTowards(driver, DN[1], 20);
    if (await portalUp(driver, DN[1], FLOORS.f3, FLOORS.f2, 'dn01→dn02'))
      await walkTowards(driver, DN[2], 20);
    await portalUp(driver, DN[2], FLOORS.f2, FLOORS.platform, 'dn02→dn03');

    // in01 ↔ 房间往返
    await walkTowards(driver, IN01_X, 120);
    await portalUpToMap(driver, IN01_X, FLOORS.platform, ROOM, 'in01 进入自由市场<1>');
    await settleShot(driver, 'fm05-room1', '自由市场<1> 房间');
    await walkTowards(driver, ROOM_OUT00_X, 30);
    const back = await portalUpToMap(driver, ROOM_OUT00_X, ROOM_FLOOR, HUB, 'out00 返回自由市场入口');
    if (back) {
      const settled = await playerAt(driver);
      report(Math.abs(settled.x - HUB_RETURN_X) <= 60 && inRange(settled.y, FLOORS.platform),
        `返回落点 in01（x=${settled.x} y=${settled.y}，期望 x≈${HUB_RETURN_X} 平台层 y∈[${FLOORS.platform}]）`);
    }
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  FM_FLOW_CARD must be 0..3, got ${process.env.FM_FLOW_CARD}`); process.exitCode = 1;
} else await main();
