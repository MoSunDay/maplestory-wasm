// 自由市场(910000000) 新增两个玩法向导 NPC 的端到端验证。
//
//   * 9010000「玩法向导」x=850（wz life index 6）：Space 打开组队任务(PQ)
//     招募菜单，断言提示含「玩法向导」与「组队任务」，Escape 关闭。
//   * 9010005「BOSS 向导」x=1150（wz life index 7）：Space 打开 BOSS 入口
//     菜单，断言提示含「BOSS 向导」与「入口」；点击首选项「扎昆入口(50级+)」
//     对应 211042300，不在脚本的 DANGEROUS_MAPS 危险名单内 → 单击直接
//     cm.warp 落图、对话关闭，断言 msmap_id === 211042300。
//
// NPC 脚本为 scripts/npc/9010000.js / 9010005.js（服务端支持热更新），life
// 注入位于 wz/Map.wz/Map/Map9/910000000.img.xml 的 <imgdir name="life">
// index 6/7。对话选项是真实鼠标点击：UINpcTalk 的 SELECTION 模式默认
// selected=0，点中首行会发送 selection(type, 首项 id)。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   update accounts set loggedin=0 where name='test2';
//   update characters set map=910000000, spawnpoint=0 where id=219;
//   test2 的 card0 是 test2ILA（character id=219, accountid=64）。
//   keymap 需有 Space/Alt/Ctrl/Q（本机已写入 57→5/54、56→5/53、29→5/52、
//   16(Q)→4/8。注意：keymap 的 `key` 列是客户端 Keytable 的 MS 扫描码
//   下标（Q=16，GLFW 键码 81 对应的是 PAGE_DOWN 槽位），而客户端
//   KeyType 枚举 MENU=4/ACTION=5——打开任务日志必须用 MENU 类型，
//   ACTION/8 不会触发任何分支）。
// 注意：本脚本结束时角色会被传送到扎昆入口一带（实测客户端先落
// 211042300，会话结束后 DB 落点为 211041900），重复运行前必须重新执行
// 上面的 SQL 重置地图。
// 运行：
//   FM_GUIDES_ACCOUNT=test2 FM_GUIDES_PASSWORD=test2 \
//   FM_GUIDES_CARD=0 CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/fm-gameplay-guides.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.FM_GUIDES_DEBUG_PORT || 9256);
const pageUrl = process.env.FM_GUIDES_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.FM_GUIDES_ACCOUNT || 'test2';
const password = process.env.FM_GUIDES_PASSWORD || 'test2';
const cardIndex = Number(process.env.FM_GUIDES_CARD ?? 0);

const HUB = 910000000;
const PQ_X = 850;
const BOSS_X = 1150;
const ZAKUM_DOOR = 211042300;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};
const probe = (driver, name) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const promptText = driver =>
  driver.evaluate("Module.ccall('msnpc_prompt', 'string', [], [])");
const playerX = driver => probe(driver, 'msplayer_x');
const inRange = (v, range) => v >= range[0] && v <= range[1];

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

async function walkTowards(driver, targetX, maxSeconds = 90) {
  const start = Date.now();
  let lastX = null, stuck = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuck += 1;
      if (stuck >= 3) {
        await driver.key('Alt', 'AltLeft', 18);
        await sleep(650);
        const after = await playerX(driver);
        if (Math.abs(after - x) < 2 && stuck >= 6)
          throw new Error(`walk stuck at x=${x} target=${targetX}`);
      }
    } else stuck = 0;
    lastX = x;
    const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  const finalX = await playerX(driver);
  if (Math.abs(finalX - targetX) > 10)
    throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

async function settleShot(driver, name) {
  if (!await waitFor(assetsIdle(driver), 90))
    throw new Error(`assets not idle before ${name}`);
  await sleep(1500);
  await driver.screenshot(name);
}

async function loginAndEnterGame(driver) {
  console.log('INFO  等待 WASM 初始化');
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  console.log('INFO  WASM 就绪');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30))
    throw new Error('login UI did not activate');
  console.log('INFO  登录界面');
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45))
    throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216,
    async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 45))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60))
    throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    entered = await waitFor(
      async () => await probe(driver, 'msui_state') === uiState.game, 5);
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  console.log('INFO  进入游戏，等待资源');
  if (!await waitFor(assetsIdle(driver), 120))
    throw new Error('in-game assets never idle');
}

// UINpcTalk 选项的实际屏幕坐标：窗口在 800x600 设计画布居中，x=340 落在
// TEXT_WIDTH 内。从提示文字下方开始逐行点击，命中第一个选项会改变 prompt
// （主菜单→子菜单/确认框）或关闭对话（warp）。返回被点击的 y 与点击后的
// prompt。
async function clickFirstOption(driver, fromY = 140, toY = 430, step = 5) {
  const before = await promptText(driver);
  for (let y = fromY; y <= toY; y += step) {
    await driver.click(340, y);
    await sleep(450);
    const active = await probe(driver, 'msnpc_active') === 1;
    const after = active ? await promptText(driver) : '';
    if (!active || after !== before) return { y, active, after };
  }
  throw new Error('no selectable dialogue option was hit');
}

async function openNpc(driver, x, label) {
  const finalX = await walkTowards(driver, x, 120);
  if (Math.abs(finalX - x) > 100)
    throw new Error(`${label}: 走位失败（x=${finalX}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    await sleep(500);
  }
  throw new Error(`${label}: Space 未打开对话/商店`);
}

async function closeDialogue(driver, label) {
  await driver.key('Escape', 'Escape', 27);
  const closed = await waitFor(async () => await probe(driver, 'msnpc_active') === 0 &&
    await probe(driver, 'msshop_active') === 0, 8);
  report(closed, `${label}: Escape 关闭窗口`);
  if (!closed) throw new Error(`${label}: 窗口未关闭，后续走位会被 UI 吞键`);
  await sleep(400);
}

async function testPqGuide(driver) {
  await openNpc(driver, PQ_X, '玩法向导 9010000');
  const prompt = await promptText(driver);
  report(prompt.includes('玩法向导') && prompt.includes('组队任务'),
    `玩法向导: PQ 招募菜单出现（prompt=${JSON.stringify(prompt).slice(0, 90)}）`);
  await settleShot(driver, 'fm-guides-pq-menu');
  await closeDialogue(driver, '玩法向导');
}

async function testBossGuideWarp(driver) {
  await openNpc(driver, BOSS_X, 'BOSS 向导 9010005');
  const prompt = await promptText(driver);
  report(prompt.includes('BOSS 向导') && prompt.includes('入口'),
    `BOSS 向导: 入口菜单出现（prompt=${JSON.stringify(prompt).slice(0, 90)}）`);
  await settleShot(driver, 'fm-guides-boss-menu');

  const choice = await clickFirstOption(driver);
  report(!choice.active,
    `BOSS 向导: 首选项扎昆入口(50级+) 非危险地图，单击即 warp（y=${choice.y}）`);
  const warped = await waitFor(
    async () => await probe(driver, 'msmap_id') === ZAKUM_DOOR, 45);
  report(warped, `BOSS 向导: 传送到 211042300（实测 ${await probe(driver, 'msmap_id')}）`);
  if (!warped) throw new Error(`未到达扎昆入口：${await probe(driver, 'msmap_id')}`);
  await settleShot(driver, 'fm-guides-zakum-door');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    const map = await probe(driver, 'msmap_id');
    report(map === HUB, `出生地图 = 910000000（实测 ${map}）`);
    if (map !== HUB) throw new Error(`出生地图不是自由市场：${map}`);
    const spawn = await playerX(driver);
    report(inRange(spawn, [-180, 20]), `出生落点 sp 区（x=${spawn}）`);

    await testPqGuide(driver);
    await testBossGuideWarp(driver);
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    if (driver.errors.length)
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`,
        `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`,
      `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  FM_GUIDES_CARD must be 0..3, got ${process.env.FM_GUIDES_CARD}`);
  process.exitCode = 1;
} else await main();
