// 自由市场(910000000) 079MAX4 复刻 UI 确认流程。
//
// 角色直接出生在自由市场（SQL 已置 map=910000000），无需 GM 命令，全流程用
// 真实交互验证 1:1 复刻：NPC 对话（妮娜 9270052 x=34 / 弗兰德里 9030000
// x=1590）、up00→up01→up02→up03 楼梯传送链（ground→-284→-568→-851 四层）、
// dn00..dn03 下行链、in01↔out00 市场房间往返（910000001），并在每层做
// readPixels 取证：ASCII 结构网格 + 色相分布（ChewChew 岛风格蓝绿主导）。
// 传送门坐标取自 079 版 wz XML（tools/verify 校验过的同一份数据）。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
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
// 079 wz 坐标（见 fm-map-079-max4-1to1.md）
const NINA_X = 34, FRED_X = 1590;
const UP = [1227, 1269, 1313], DN = [2142, 2195, 2247], IN01_X = 1332, OUT00_X = 790;
const FLOORS = { ground: [-30, 60], f2: [-330, -240], f3: [-610, -530], f4: [-900, -810] };

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}
const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const uiStateIs = (driver, expected) => () => probe(driver, 'msui_state').then(v => v === expected);
function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}
async function playerAt(driver) {
  return {
    x: await probe(driver, 'msplayer_x'), y: await probe(driver, 'msplayer_y'),
    sx: await probe(driver, 'msplayer_screen_x'), sy: await probe(driver, 'msplayer_screen_y')
  };
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

async function walkTowards(driver, targetX, maxSeconds = 90) {
  const key = targetX > (await playerAt(driver)).x ? 'ArrowRight' : 'ArrowLeft';
  const code = key, vk = key === 'ArrowRight' ? 39 : 37;
  const start = Date.now();
  let lastX = null, stuckMs = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const { x } = await playerAt(driver);
    if (Math.abs(x - targetX) <= 24) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuckMs += 700;
      if (stuckMs > 2100) { // 卡住：可能是梯子/台阶，点一下 Up 越过
        await driver.keyHold('ArrowUp', 'ArrowUp', 38, 900); stuckMs = 0;
        await sleep(400);
      }
    } else stuckMs = 0;
    lastX = x;
    await driver.keyHold(key, code, vk, 700);
    await sleep(120);
  }
  return (await playerAt(driver)).x;
}

// 站上传送门按 Up，等待 y 落入目标层区间
async function portalUp(driver, floorRange, label, maxSeconds = 25) {
  await driver.keyHold('ArrowUp', 'ArrowUp', 38, 1300);
  const ok = await waitFor(async () => {
    const { y } = await playerAt(driver);
    return y >= floorRange[0] && y <= floorRange[1];
  }, maxSeconds);
  const { x, y } = await playerAt(driver);
  report(ok, `${label}: 传送后 y=${y} x=${x}（期望区间 [${floorRange[0]},${floorRange[1]}]）`);
  return ok;
}

async function npcDialog(driver, npcX, name) {
  const finalX = await walkTowards(driver, npcX, 120);
  if (Math.abs(finalX - npcX) > 90) { report(false, `${name}: 未接近（停在 x=${finalX}）`); return; }
  const { x, sx, sy } = await playerAt(driver);
  const dialogRegion = await sampleLuma(driver, 60, 330, 540, 540);
  let opened = false;
  for (const dy of [-52, -70, -36]) {
    await driver.doubleClick(sx + (npcX - x), sy + dy);
    await sleep(1600);
    const after = await sampleLuma(driver, 60, 330, 540, 540);
    if (after - dialogRegion > 12) { opened = true; break; }
  }
  report(opened, `${name}: 对话框弹出（亮度 ${dialogRegion}→${await sampleLuma(driver, 60, 330, 540, 540)}）`);
  await driver.screenshot(`npc-${name}`);
  await driver.key('Escape', 'Escape', 27);
  await sleep(900);
}

async function sampleLuma(driver, x0, y0, x1, y1) {
  const rect = JSON.stringify({ x0, y0, x1, y1 });
  return await evaluateAwait(driver, `(() => new Promise(resolve => {
    requestAnimationFrame(() => {
      const r = ${rect};
      const canvas = document.getElementById('canvas');
      const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
      const sx = canvas.width / 800, sy = canvas.height / 600;
      const w = Math.round((r.x1 - r.x0) * sx), h = Math.round((r.y1 - r.y0) * sy);
      const p = new Uint8Array(w * h * 4), bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.readPixels(Math.round(r.x0 * sx), canvas.height - Math.round(r.y1 * sy), w, h, gl.RGBA, gl.UNSIGNED_BYTE, p);
      gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
      let l = 0, n = 0;
      for (let i = 0; i < p.length; i += 4) { l += (p[i] + p[i + 1] + p[i + 2]) / 3; n += 1; }
      resolve(Math.round(l / n));
    });
  }))()`);
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
  let entry;
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    report(await probe(driver, 'msmap_id') === HUB,
      `出生地图 = 910000000（实测 ${await probe(driver, 'msmap_id')}）`);
    entry = await playerAt(driver);
    report(entry.x >= -180 && entry.x <= 80 && entry.y >= -30 && entry.y <= 60,
      `出生点落在 sp 区（x=${entry.x} y=${entry.y}，期望 x∈[-180,80] 地面层）`);
    await settleShot(driver, 'fm00-entry', '入口层（左）');

    await npcDialog(driver, NINA_X, '妮娜9270052');

    await walkTowards(driver, FRED_X, 150);
    await settleShot(driver, 'fm01-ground-right', '地面层右段（店门+弗兰德里）');
    await npcDialog(driver, FRED_X, '弗兰德里9030000');

    // up 楼梯链：ground→f2→f3→f4
    await walkTowards(driver, UP[0], 150);
    if (await portalUp(driver, FLOORS.f2, 'up00→up01')) await settleShot(driver, 'fm02-floor2', '二层（in07..in12）');
    await walkTowards(driver, UP[1], 60);
    if (await portalUp(driver, FLOORS.f3, 'up01→up02')) await settleShot(driver, 'fm03-floor3', '三层（in13..in17）');
    await walkTowards(driver, UP[2], 60);
    if (await portalUp(driver, FLOORS.f4, 'up02→up03')) await settleShot(driver, 'fm04-floor4', '四层（in18..in22）');

    // dn 下行链回地面
    await walkTowards(driver, DN[0], 90);
    let downOk = await portalUp(driver, FLOORS.f3, 'dn00→dn01');
    if (downOk) { await walkTowards(driver, DN[1], 60); downOk = await portalUp(driver, FLOORS.f2, 'dn01→dn02'); }
    if (downOk) { await walkTowards(driver, DN[2], 60); downOk = await portalUp(driver, FLOORS.ground, 'dn02→dn03'); }

    // in01 ↔ out00 房间往返
    await walkTowards(driver, IN01_X, 120);
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 1300);
    report(await waitFor(async () => await probe(driver, 'msmap_id') === ROOM, 20),
      `in01 进入自由市场<1>（实测 ${await probe(driver, 'msmap_id')}）`);
    await settleShot(driver, 'fm05-room1', '自由市场<1> 房间');
    await walkTowards(driver, OUT00_X, 60);
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 1300);
    const backHub = await waitFor(async () => await probe(driver, 'msmap_id') === HUB, 20);
    report(backHub, `out00 返回自由市场入口（实测 ${await probe(driver, 'msmap_id')}）`);
    if (backHub) {
      const back = await playerAt(driver);
      report(Math.abs(back.x - IN01_X) < 90, `返回落点 in01（x=${back.x}，期望≈${IN01_X}）`);
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
