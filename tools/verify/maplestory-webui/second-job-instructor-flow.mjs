// 二转教官（武术教练 1072000）全流程 e2e：验证 2026-10-29 对话 toggle 失步
// 修复（commit cc80da1）在真实二转脚本上的行为（对话不消失、可续、可重开，
// 服务器脚本状态不被中途重置），并完整走通 接受考验 → 传送考验地图。
//
// ── 前置 Runbook（复跑必读，全部坑均实测踩过）──────────────────────────
// 1) 账号/角色（test1 / test1Warr id=243，world=4 "Reboot"）：
//    - accounts.characterslots 必须大于账号下角色数（否则 CharlistHandler
//      校验异常 → 客户端 Asyncify park）：update accounts set characterslots=6 ...
//    - 账号只保留 1 张卡（选角 UI PAGESIZE=4，第 5 张卡在第 2 页易点空）。
//    - characters.sp 必须逗号格式 '61,0,0,0'（分号会触发服务端
//      NumberFormatException → 无 WORLD_LIST → park）。
//    - 拉高 maxhp/hp（默认 600 会被路上怪物打死回城 102000000 断流程）。
// 2) 任务/道具：queststatus 100003 status=1（列名 characterid,quest,status,
//    time,expires,forfeited,info；time 为秒级 int）；ETC pos1 放信 4031008。
// 3) plife 刷点（SQL 建的 NPC 站位，MapleMapFactory.loadLifeFromDb）三要素：
//    - world 必须与角色所在 world 一致（本环境 world=4，不是 0！）；
//    - y 必须高于目标 foothold 顶部（客户端 NpcSpawn::instantiate 用
//      physics.get_y_below 向下吸附：y=2385 低于 fh83 顶 2382 会一路掉进
//      y=2805 深坑，NPC 永远不可对话）；cy 同 y；
//    - fh 填该图真实 fh id（本图无 fh0；填 0 无效）。
//    参考行：
//      plife(102020300,1072000,'n',cy=2300,fh=83,x=-800,y=2300,world=4)
//      plife(108000300,1072004,'n',cy=2150,fh=122,x=880,y=2150,world=4)
// 4) SQL 改动（建角/任务/plife/map）后必须 docker restart ms-game-server：
//    - Server.java isFirstAccountLogin 只在启动后首次登录缓存账号→角色集合，
//      SQL 插入的角色不在缓存 → CharSelectedHandler 静默 closeSession；
//    - 地图对象有进程内缓存，plife 改动不重启不生效。
// 5) 重启偶发 world4 绑 7975 失败（Address already in use / TIME_WAIT），
//    登录服仍会引导客户端连 7975 → 页面 park；再 restart 一次即可。
// 6) 每次复跑前复位：update characters set map=102020300, spawnpoint=0,
//    hp=2500, mp=800 ...; update accounts set loggedin=0 where name='test1';
//    （上轮跑完 100003 已 complete、信被扣，需重插 queststatus/信。）
//
// 流程：102020300 走到 1072000(plife x=-800) → Space 弹对话 → 对话中再按
// Space 应保持打开 → Escape 关 → 再 Space 重开 → 探针点 NEXT×3 + YES +
// NEXT 接受考验 → 断言 warp 108000300 → 走到同事 NPC 1072004(plife x=880)
// 对话可再开。
// 用法：node second-job-instructor-flow.mjs（环境变量 SJ_ACCOUNT/PASSWORD/URL/
// SJ_DEBUG_PORT/SJ_CARD；端口默认 9264，勿与 9259/9263 冲突）。
import { BrowserDriver, sleep } from './cdp.mjs';
import { clickDialogButton } from './pq-harness-dialog.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.SJ_DEBUG_PORT || 9264);
const pageUrl = process.env.SJ_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.SJ_ACCOUNT || 'test1';
const password = process.env.SJ_PASSWORD || 'test1';
const cardIndex = Number(process.env.SJ_CARD ?? 0); // 当前 test1 只保留 test1Warr 一张卡
const ENTRY_MAP = 102020300, INSTRUCTOR_X = -800;
const TEST_MAP = 108000300, COLLEAGUE_X = 880;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };

const cc = (driver, fn, ...args) => driver.evaluate(
  `Module.ccall('${fn}','number',[${args.map(() => "'number'").join(',')}],[${args.join(',')}])`).catch(() => -1);
const probe = async (driver, name) => { try { return await cc(driver, name); } catch { return -1; } };
const playerX = async driver => await probe(driver, 'msplayer_x');
const mapId = async driver => await probe(driver, 'msmap_id');
const npcActive = driver => probe(driver, 'msnpc_active');
const npcPrompt = async driver => {
  try { return await driver.evaluate("Module.ccall('msnpc_prompt','string',[],[])"); }
  catch { return null; }
};
const dialogueMode = async driver => {
  const m = await cc(driver, 'msnpctalk_element_field', 4);
  return m;
};

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
const inputReady = async driver =>
  await probe(driver, 'msasset_blocking') === 0
  && await probe(driver, 'msui_enabled') === 1;

async function walkTowards(driver, targetX, maxSeconds = 120) {
  const start = Date.now();
  let lastX = null, stuckCount = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    if (Math.abs(targetX - x) <= 12) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuckCount += 1;
      if (stuckCount >= 3) { await driver.key('Alt', 'AltLeft', 18); await sleep(650); }
    } else stuckCount = 0;
    lastX = x;
    const ms = Math.abs(targetX - x) > 400 ? 500 : Math.abs(targetX - x) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  return await playerX(driver);
}

async function openDialog(driver, targetX) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (!await waitFor(() => inputReady(driver), 30)) continue;
    // 怪物击退可能把角色推出对话射程，每次尝试前先走回目标 NPC
    await walkTowards(driver, targetX, 60);
    await sleep(400);
    await driver.key('Space', 'Space', 32);
    for (let i = 0; i < 120; i += 1) {
      if (await npcActive(driver) === 1) return true;
      await sleep(250);
    }
  }
  return false;
}

// 探针直取 NEXT（which=4）/OK（which=3）按钮中心（相对坐标+元素原点）。
async function probeClickDialogButton(driver, which) {
  const px = await cc(driver, 'msnpctalk_element_field', 0);
  const py = await cc(driver, 'msnpctalk_element_field', 1);
  const bx = await cc(driver, 'msnpctalk_button_field', which, 0);
  const by = await cc(driver, 'msnpctalk_button_field', which, 1);
  const bw = await cc(driver, 'msnpctalk_button_field', which, 2);
  const bh = await cc(driver, 'msnpctalk_button_field', which, 3);
  const active = await cc(driver, 'msnpctalk_button_field', which, 4);
  if ([px, py, bx, by, bw, bh].some(v => v <= -9000) || active !== 1 || bw <= 0 || bh <= 0)
    return false;
  await driver.click(px + bx + (bw >> 1), py + by + (bh >> 1));
  return true;
}

// 通用翻页/确认循环：YesNo 页（mode=1）点 YES，其余优先 NEXT 再 OK；
// 返回是否触发 warp（离开入口图）。
async function advanceConversationToWarp(driver) {
  let lastPrompt = await npcPrompt(driver);
  for (let step = 0; step < 10; step += 1) {
    if (await mapId(driver) === TEST_MAP) return true;
    if (await npcActive(driver) !== 1) return await mapId(driver) === TEST_MAP;
    const mode = await dialogueMode(driver);
    const promptBefore = lastPrompt;
    if (mode === 1) {
      await clickDialogButton(driver, 'yes', async () => {
        const p = await npcPrompt(driver);
        return p !== promptBefore || await mapId(driver) === TEST_MAP || await npcActive(driver) === 0;
      }, 8);
    } else {
      const clicked = await probeClickDialogButton(driver, 4) || await probeClickDialogButton(driver, 3);
      if (!clicked) return false;
    }
    for (let i = 0; i < 32; i += 1) {
      if (await mapId(driver) === TEST_MAP) return true;
      const p = await npcPrompt(driver);
      if (p !== null && p !== promptBefore) { lastPrompt = p; break; }
      if (await npcActive(driver) === 0 && await mapId(driver) !== ENTRY_MAP) return true;
      await sleep(250);
    }
    await sleep(400);
  }
  return await mapId(driver) === TEST_MAP;
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort,
  artifactDir: process.env.SJ_ARTIFACT_DIR || undefined });

try {
  console.log(`WebUI endpoint: ${pageUrl}`);
  await driver.start();
  await driver.navigate(pageUrl);
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 240))
    throw new Error('login UI did not activate');
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 90))
    throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216, async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 90))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 90))
    throw new Error('char-select input did not unlock');
  const center = cardCenter(cardIndex);
  await driver.click(center.x, center.y);
  await sleep(400);
  const entered = await driver.retryClick(656, 453, async () => await probe(driver, 'msui_state') === uiState.game);
  if (!entered && !await waitFor(async () => await probe(driver, 'msui_state') === uiState.game, 60))
    throw new Error('did not enter game');
  const entryMapId = await mapId(driver);
  report(entryMapId === ENTRY_MAP, `进入入口图 ${ENTRY_MAP}（实测 ${entryMapId}）`);
  if (!await waitFor(assetsIdle(driver), 180)) throw new Error('game assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 60))
    throw new Error('game input did not unlock');

  const x = await walkTowards(driver, INSTRUCTOR_X, 180);
  report(Math.abs(x - INSTRUCTOR_X) <= 40, `走到武术教练 1072000 旁（x=${x}，目标 ${INSTRUCTOR_X}）`);
  await sleep(600);

  report(await openDialog(driver, INSTRUCTOR_X), '与武术教练对话弹出（msnpc_active=1）');
  const firstPrompt = await npcPrompt(driver);
  report(!!firstPrompt, `对话文本就绪（${(firstPrompt || '').slice(0, 24)}…）`);
  await driver.screenshot('sj-01-dialog-open');

  await driver.key('Space', 'Space', 32);
  let sawOpen = false;
  for (let i = 0; i < 24; i += 1) {
    const s = await npcActive(driver);
    if (s === 0) break;
    if (s === 1) sawOpen = true;
    await sleep(250);
  }
  report(sawOpen, '对话中再按 Space：对话保持打开（服务器会话未被重开）');
  await driver.screenshot('sj-02-dialog-still-open');

  await driver.key('Escape', 'Escape', 27);
  report(await waitFor(async () => await npcActive(driver) === 0, 15), 'Escape 关闭对话');
  await sleep(400);
  report(await openDialog(driver, INSTRUCTOR_X), '关闭后再次 Space：对话重新弹出（无法再对话症状消失）');

  const warped = await advanceConversationToWarp(driver);
  if (warped) await waitFor(async () => await mapId(driver) === TEST_MAP, 30); // 切图瞬间 mapId 可能瞬时为 0
  const warpMapId = await mapId(driver);
  report(warped, `对话推进接受考验并传送 ${TEST_MAP}（实测 ${warpMapId}）`);
  await driver.screenshot('sj-03-test-map');
  if (!warped) throw new Error('did not warp to test map');

  if (!await waitFor(assetsIdle(driver), 180)) throw new Error('test-map assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 60))
    throw new Error('test-map input did not unlock');
  const cx = await walkTowards(driver, COLLEAGUE_X, 180);
  report(Math.abs(cx - COLLEAGUE_X) <= 40, `走到考验地图同事 NPC 旁（x=${cx}，目标 ${COLLEAGUE_X}）`);
  await sleep(600);
  report(await openDialog(driver, COLLEAGUE_X), '传送后与同事 NPC 对话正常弹出');
  await driver.screenshot('sj-04-colleague-dialog');
  await driver.key('Escape', 'Escape', 27);
  report(await waitFor(async () => await npcActive(driver) === 0, 15), '收尾 Escape 关闭');

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
console.log(failed ? 'SECOND-JOB-INSTRUCTOR FLOW: FAILED' : 'SECOND-JOB-INSTRUCTOR FLOW: ALL PASS');
process.exit(failed ? 1 : 0);
