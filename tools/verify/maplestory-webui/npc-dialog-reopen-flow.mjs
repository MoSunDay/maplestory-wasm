// NPC 对话 toggle 失步回归（2026-10-29 修复：NpcDialogueHandler makeactive +
// UIStateGame 键盘/鼠标泄漏守卫）。射手村(100000000) 正向对照 NPC 1012000(x=149)：
//   1) Space 首次弹出对话（msnpc_active=1）；
//   2) 对话打开期间再按 Space —— 旧版会二次 TalkToNPC→服务器重发一页→
//      toggle 关窗；现要求 4s 采样内 active 始终为 1（输入被守卫吞掉）；
//   3) Escape 关闭对话（active=0）；
//   4) 再次 Space —— 「无法再对话」症状消失，对话重新弹出（active=1）；
//   5) 再次 Escape 关闭，无残留、无浏览器错误。
// 服务端前置（跑前执行）：
//   update accounts set loggedin=0 where name='test1';
//   update characters set map=100000000, spawnpoint=0 where name='test1Hero';
//   —— 关键：SPACE(57)→ACTION/NPCCHAT 绑定必须存在（skill-keymap-prep.py 会整表
//   覆写 keymap 抹掉默认键，导致 Space 无动作、对话永不弹出）：
//   delete from keymap where characterid=(select id from characters where name='test1Hero') and `key`=57;
//   insert into keymap(characterid,`key`,type,action) select id,57,5,54 from characters where name='test1Hero';
// 端口注意：默认 9263（勿用 9259，与 cash-shop-take-reject-retry.mjs 冲突会互杀浏览器）。
// 用法：node npc-dialog-reopen-flow.mjs   （环境变量 NPCRE_ACCOUNT/PASSWORD/URL/DEBUG_PORT/CARD）
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.NPCRE_DEBUG_PORT || 9263);
const pageUrl = process.env.NPCRE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.NPCRE_ACCOUNT || 'test1';
const password = process.env.NPCRE_PASSWORD || 'test1';
const cardIndex = Number(process.env.NPCRE_CARD ?? 0);
const NPC_X = 149; // 1012000（射手村出生点旁正向对照 NPC）

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };
// 进图期 WASM 主线程繁忙会让 CDP Runtime.evaluate 超时；视为"未就绪"(-1)
// 交由 waitFor 继续轮询，而不是让整轮回归失败。
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

// 资源流空闲 + 输入解锁才允许按键（msasset_blocking!=0 时 UI 层吞输入，
// 冷缓存下 Space 会被整段丢弃）。
const inputReady = async driver =>
  await probe(driver, 'msasset_blocking') === 0
  && await probe(driver, 'msui_enabled') === 1;

// 按 Space 并等待对话弹出；突发期探针超时返回 -1 视为未知继续等，
// 单次采样 30s（对话贴图 LazyFS 突发可占住主线程 10s+），最多重按 3 次。
async function openDialog(driver) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (!await waitFor(() => inputReady(driver), 30)) continue;
    await driver.key('Space', 'Space', 32);
    for (let i = 0; i < 120; i += 1) {
      if (await npcActive(driver) === 1) return true;
      await sleep(250);
    }
  }
  return false;
}

// 对话已打开时再按一次 Space，采样 seconds 秒：只允许 1 或未知(-1，
// 主线程繁忙探针超时)；显式观察到 0（对话被关）即失败。
async function spaceDuringDialogKeepsOpen(driver, seconds = 6) {
  await driver.key('Space', 'Space', 32);
  let sawOpen = false;
  for (let i = 0; i < seconds * 4; i += 1) {
    const state = await npcActive(driver);
    if (state === 0) return false;
    if (state === 1) sawOpen = true;
    await sleep(250);
  }
  return sawOpen;
}

async function escapeClosesDialog(driver) {
  await driver.key('Escape', 'Escape', 27);
  for (let i = 0; i < 60; i += 1) {
    if (await npcActive(driver) === 0) return true;
    await sleep(250);
  }
  return false;
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort,
  artifactDir: process.env.NPCRE_ARTIFACT_DIR || undefined });

try {
  console.log(`WebUI endpoint: ${pageUrl}`);
  await driver.start();
  await driver.navigate(pageUrl);
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(uiStateIs(driver, uiState.login), 240)) throw new Error('login UI did not activate');
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 90)) throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 90)) throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 90))
    throw new Error('char-select input did not unlock (previews settling)');
  // 选角：单击卡片选中，再单击 BtSelect（hit 区中心 (656,453)，
  // 坐标推导见 henesys-cindy-gone-flow.mjs；retryClick 抗事件丢弃）。
  const center = cardCenter(cardIndex);
  await driver.click(center.x, center.y);
  await sleep(400);
  const entered = await driver.retryClick(656, 453, uiStateIs(driver, uiState.game));
  if (!entered && !await waitFor(uiStateIs(driver, uiState.game), 60)) throw new Error('did not enter game');
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('game assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 60))
    throw new Error('game input did not unlock');

  const x = await walkTowards(driver, NPC_X, 120);
  report(Math.abs(x - NPC_X) <= 30, `走到对照 NPC 1012000 旁（x=${x}，目标 ${NPC_X}）`);
  await sleep(600);

  report(await openDialog(driver), '首次 Space：对话弹出（msnpc_active=1）');
  await sleep(400);
  await driver.screenshot('01-dialog-open');
  report(await spaceDuringDialogKeepsOpen(driver), '对话中再按 Space：4s 内对话保持打开（无二次 TalkToNPC→toggle 关窗）');
  await driver.screenshot('02-dialog-still-open');
  report(await escapeClosesDialog(driver), 'Escape：对话关闭（msnpc_active=0）');
  await sleep(400);
  report(await openDialog(driver), '关闭后再次 Space：对话可重新弹出（「无法再对话」症状消失）');
  await driver.screenshot('03-dialog-reopened');
  report(await escapeClosesDialog(driver), '收尾 Escape：对话关闭');
  await sleep(400);
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
console.log(failed ? 'NPC-DIALOG-REOPEN FLOW: FAILED' : 'NPC-DIALOG-REOPEN FLOW: ALL PASS');
process.exit(failed ? 1 : 0);
