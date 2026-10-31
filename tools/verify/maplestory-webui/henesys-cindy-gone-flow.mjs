// 射手村(100000000) 辛迪(9200000)移除后的行为级冒烟：
//   1) 正向对照 —— 出生点旁 NPC 1012000(x=149) Space 对话可弹出（NPC 系统正常）；
//   2) 负向断言 —— 辛迪原位置 x=424（±120px 内无其他 NPC）Space 后 msnpc_active
//      保持 0，对话永不弹出 ⇒ 场景中辛迪已不存在。
// 服务端前置（跑前执行）：
//   update accounts set loggedin=0 where name='test1';
//   update characters set map=100000000, spawnpoint=0 where name='test1Hero';
// 用法：node henesys-cindy-gone-flow.mjs   （环境变量 CINDY_ACCOUNT/PASSWORD/URL/DEBUG_PORT/CARD）
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.CINDY_DEBUG_PORT || 9257);
const pageUrl = process.env.CINDY_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.CINDY_ACCOUNT || 'test1';
const password = process.env.CINDY_PASSWORD || 'test1';
const cardIndex = Number(process.env.CINDY_CARD ?? 0);
const MAP = 100000000;
const CONTROL_NPC_X = 149;   // 1012000（正向对照）
const CINDY_X = 424;         // 辛迪 9200000 原位置（已删）

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };
// 进图期 WASM 主线程长时间繁忙会让 CDP Runtime.evaluate 超时；视为
// "未就绪"（返回 -1）交由 waitFor 继续轮询，而不是让整轮冒烟失败。
const probe = async (driver, name) => {
  try { return await driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`); }
  catch { return -1; }
};
const playerX = async driver => await probe(driver, 'msplayer_x');
const uiStateIs = (driver, state) => async () => await probe(driver, 'msui_state') === state;
const npcActive = driver => probe(driver, 'msnpc_active');

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}

const assetsIdle = driver => async () => {
  if (await probe(driver, 'msasset_blocking') !== 0) return false;
  try {
    return await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')") === true;
  } catch { return false; }
};

async function walkTowards(driver, targetX, maxSeconds = 90) {
  const start = Date.now();
  let lastX = null, stuckCount = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    if (Math.abs(targetX - x) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuckCount += 1;
      if (stuckCount >= 3) {
        await driver.key('Alt', 'AltLeft', 18);
        await sleep(650);
      }
    } else stuckCount = 0;
    lastX = x;
    const ms = Math.abs(targetX - x) > 400 ? 500 : Math.abs(targetX - x) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  return await playerX(driver);
}

async function pressSpaceAndSample(driver) {
  // 按下 Space 后采样 4s：返回期间 msnpc_active 是否出现过 1（防首按被吞，
  // 最多重按 3 次；任一次弹出即算“有对话”）。
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    for (let i = 0; i < 16; i += 1) {
      if (await npcActive(driver) === 1) return true;
      await sleep(250);
    }
  }
  return false;
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort,
  artifactDir: process.env.CINDY_ARTIFACT_DIR || undefined });

try {
  console.log(`WebUI endpoint: ${pageUrl}`);
  await driver.start();
  await driver.navigate(pageUrl);
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(uiStateIs(driver, uiState.login), 120)) throw new Error('login UI did not activate');
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 90)) throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 90)) throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 90))
    throw new Error('char-select input did not unlock (previews settling)');
  // 选角：单击卡片选中（AreaButton 热区 (100+120i,160)-(160+120i,280)，
  // UICharSelectLayout.h 实测），再单击 BtSelect。本环境 UI.nx 是
  // post-Chaos 资产（Login.img/WorldSelect/BtChannel marker 存在），按钮
  // 锚点 = charinfopos(662,355)+(-76,72)，sprite 140x52 origin(0,0) →
  // hit 区 (586,427)-(726,479)，中心 (656,453)。不用 doubleClick：
  // Window.cpp 双击窗口 350ms/4px，WASM 忙时两次 glfwPollEvents 间隔
  // 易超窗，服务端日志会停在 CHARLIST。用 retryClick 抗事件丢弃。
  const center = cardCenter(cardIndex);
  await driver.click(center.x, center.y);
  await sleep(400);
  const entered = await driver.retryClick(656, 453, uiStateIs(driver, uiState.game), 8, 4000);
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 150)) throw new Error('in-game assets never idle');

  const mapId = await probe(driver, 'msmap_id');
  report(mapId === MAP, `进入射手村 ${MAP}（实测 msmap_id=${mapId}）`);
  if (mapId !== MAP) throw new Error('wrong map, aborting');
  await sleep(2500);
  await driver.screenshot('00-henesys-spawn');

  // 正向对照：1012000 对话可弹出
  const ctrlX = await walkTowards(driver, CONTROL_NPC_X, 90);
  report(Math.abs(ctrlX - CONTROL_NPC_X) <= 30, `走到对照 NPC 1012000（x=${ctrlX}，目标 ${CONTROL_NPC_X}）`);
  const ctrlOpened = await pressSpaceAndSample(driver);
  report(ctrlOpened, '对照 NPC 1012000：Space 对话弹出（msnpc_active=1，NPC 系统正常）');
  if (ctrlOpened) {
    await driver.key('Escape', 'Escape', 27);
    await waitFor(async () => await npcActive(driver) === 0, 6);
  }

  // 负向断言：辛迪原位置无对话
  const cindyX = await walkTowards(driver, CINDY_X, 120);
  report(Math.abs(cindyX - CINDY_X) <= 30, `走到辛迪原位置（x=${cindyX}，目标 ${CINDY_X}）`);
  await sleep(600);
  const cindyOpened = await pressSpaceAndSample(driver);
  report(!cindyOpened, '辛迪原位置 x=424：Space 后 msnpc_active 保持 0 —— 场景无辛迪（±120px 内亦无其他 NPC）');
  await sleep(800);
  await driver.screenshot('01-cindy-spot-empty');
  report(await npcActive(driver) === 0, '结束时无 NPC 对话残留');

  const fatalLogs = driver.logs.filter(line =>
    /abort|runtimeerror|exception|\berror\b|failed to|offline/i.test(line));
  report(driver.errors.length === 0 && fatalLogs.length === 0,
    `运行时无错误（errors=${driver.errors.length}, fatalLogs=${fatalLogs.length}）`);
} catch (error) {
  failed = true;
  console.error('ERROR', error);
} finally {
  await driver.stop();
}
console.log(failed ? 'CINDY-GONE FLOW: FAILED' : 'CINDY-GONE FLOW: ALL PASS');
process.exit(failed ? 1 : 0);
