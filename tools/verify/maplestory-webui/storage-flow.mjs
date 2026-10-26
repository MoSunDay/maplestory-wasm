// 仓库（Trunk/STORAGE, opcode 309）窗口端到端验证（Ellinia 仓库内室 130000200，
// 储物 NPC 1100000 位于 x=1370, y=84 的平台；出生点 sp0 在 x=1642 同层）。
//
// 登录 → 卡0 进游戏 → 走到 x≈1385（与 NPC dx=15<120, dy=74<80 满足 talk_to_nearest
// 判定）按 Space → 9030100.js 同款 sendStorage 流程由 NPC 1100000 触发，然后验证:
//  1. msui_front_at(400,240)==15（STORAGE）且命中框为完整 463 宽列表窗：
//     x=200/600 命中、x=100/700 未命中；y=60 未命中、y=430 未命中（窗体 y 81..399）。
//     修复前代际判定错误导致窗口只渲染 263px 残骸（x=200/600 均未命中）。
//  2. 像素采样：两侧列表面板亮带与中央分隔（≈x 373..405 暗带）存在。
//  3. 点 ETC tab（右条带 index2）与背包首行后窗口保持打开；
//  4. 点退出按钮（羊皮纸条 BtExit）后窗口关闭 front(400,240)==0。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   update accounts set loggedin=0 where name='test2';
//   update characters set map=130000200, spawnpoint=0 where id=219;
//   test2 的 card0 是 test2ILA（character id=219, level 200 满足储物等级门槛）。
//   注意：收尾已改为游戏内干净登出（logout.mjs：ESC 预选 Quit Game → Enter 发
//   PLAYER_DC+WS Close），服务端 sessionClosed 即清 loggedin，可背靠背连跑，
//   无需再等 2-3 分钟或重启 ws-proxy。
// 运行：
//   STORAGE_ACCOUNT=test2 STORAGE_PASSWORD=test2 STORAGE_CARD=0 \
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/storage-flow.mjs

import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, waitForAccountLoggedOut } from './logout.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.STORAGE_DEBUG_PORT || 9296);
const pageUrl = process.env.STORAGE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.STORAGE_ACCOUNT || 'test2';
const password = process.env.STORAGE_PASSWORD || 'test2';
const cardIndex = Number(process.env.STORAGE_CARD ?? 0);

const STORAGE_MAP = 130000200;
const NPC_X = 1370;       // NPC 1100000（埃纳里斯仓库内室储物员）
const STAND_X = 1385;     // 距 NPC 15px，满足 |dx|<=120
const STORAGE_UI = 15;    // UIElement::Type::STORAGE
// 修复后窗体：463x318 居中于 (400,240) → x 168..631, y 81..399
const WINDOW_X = 168;
const WINDOW_Y = 81;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};
const probe = (driver, name) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const frontAt = (driver, x, y) =>
  driver.evaluate(`Module.ccall('msui_front_at', 'number', ['number', 'number'], [${x}, ${y}])`);

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

async function walkTowards(driver, targetX, maxSeconds = 60) {
  const start = Date.now();
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await probe(driver, 'msplayer_x');
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  const finalX = await probe(driver, 'msplayer_x');
  if (Math.abs(finalX - targetX) > 10)
    throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

async function loginAndEnterGame(driver) {
  console.log('INFO  等待 WASM 初始化');
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30))
    throw new Error('login UI did not activate');
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45)) {
    throw new Error('world-select 未激活：若上一轮测试刚结束，请等待 ws-proxy 会话'
      + '超时（约 2-3 分钟）或重启 ws-proxy 后重试');
  }
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

async function openStorage(driver) {
  const mapId = await probe(driver, 'msmap_id');
  if (mapId !== STORAGE_MAP)
    throw new Error(`地图不符：期望 ${STORAGE_MAP} 实际 ${mapId}（见文件头前置 SQL）`);
  await walkTowards(driver, STAND_X);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () =>
      await frontAt(driver, 400, 240) === STORAGE_UI, 8)) return;
    await sleep(500);
  }
  throw new Error('Space 未打开仓库窗口（确认与 NPC 距离 |dx|<=120, |dy|<=80）');
}

async function testWindowGeometry(driver) {
  const hits = {};
  for (const [x, y] of [[400, 240], [200, 240], [600, 240], [100, 240], [700, 240], [400, 60], [400, 430]])
    hits[`${x},${y}`] = await frontAt(driver, x, y);
  report(hits['400,240'] === STORAGE_UI, `窗口打开 front(400,240)=${hits['400,240']}`);
  report(hits['200,240'] === STORAGE_UI && hits['600,240'] === STORAGE_UI,
    `完整 463 宽命中框（200/600 命中，修复前均未命中）`);
  report(hits['100,240'] === 0 && hits['700,240'] === 0, '水平边界外未命中');
  report(hits['400,60'] === 0 && hits['400,430'] === 0, '垂直边界外未命中（窗体 y 81..399）');
}

// 画布 800x600 被浏览器视口等比缩放；直接读 WebGL canvas 像素采样面板亮带。
async function samplePanels(driver) {
  const result = await driver.send('Runtime.evaluate', {
    returnByValue: true,
    awaitPromise: true,
    expression: `(() => new Promise(resolve => {
      requestAnimationFrame(() => {
    const canvas = document.getElementById('canvas') || document.querySelector('canvas');
    if (!canvas) return null;
    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl')
      || canvas.getContext('experimental-webgl');
    if (!gl) return null;
    const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
    const px = new Uint8Array(4);
    const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    const read = (cx, cy) => {
      gl.readPixels(Math.round(cx * scaleX), canvas.height - 1 - Math.round(cy * scaleY),
        1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
      return (px[0] + px[1] + px[2]) / 3;
    };
    const left = read(280, 200), right = read(500, 200);
    // 中央分隔是 1px 暗线（美术 x≈232），多点采样取最小值防坐标抖动
    let divider = 255;
    for (const x of [399, 400, 401]) for (const y of [160, 200, 240])
      divider = Math.min(divider, read(x, y));
        gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
        resolve({ left, divider, right });
      });
    }))()` }).then(envelope => envelope.result.value);
  if (!result) { report(false, 'canvas 采样失败'); return; }
  report(result.left > 150 && result.right > 150,
    `两侧列表面板亮带（left=${result.left.toFixed(0)} right=${result.right.toFixed(0)}）`);
  report(result.divider < 170 && result.divider < result.left - 30,
    `中央分隔暗线（divider=${result.divider.toFixed(0)}）`);
}

async function testInteractions(driver) {
  // ETC tab（右条带 index2）：canvas (WINDOW_X+319, WINDOW_Y+63)
  await driver.click(WINDOW_X + 319, WINDOW_Y + 63);
  await sleep(500);
  // 背包首行（右列表面板）：canvas (WINDOW_X+337, WINDOW_Y+100)
  await driver.click(WINDOW_X + 337, WINDOW_Y + 100);
  await sleep(500);
  report(await frontAt(driver, 400, 240) === STORAGE_UI, 'tab/行点击后窗口保持打开');
  await driver.screenshot('storage-flow-interact');
  // 退出按钮（羊皮纸条 BtExit 110,271 中心）：canvas (WINDOW_X+138, WINDOW_Y+283)
  await driver.click(WINDOW_X + 138, WINDOW_Y + 283);
  const closed = await waitFor(async () =>
    await frontAt(driver, 400, 240) !== STORAGE_UI, 8);
  report(closed, '退出按钮关闭窗口');
}

const driver = new BrowserDriver({ debugPort, binary: chromeBinary });
try {
  await driver.start();
  await driver.navigate(pageUrl);
  await loginAndEnterGame(driver);
  await openStorage(driver);
  await testWindowGeometry(driver);
  await samplePanels(driver);
  await driver.screenshot('storage-flow-open');
  await testInteractions(driver);
} catch (error) {
  failed = true;
  console.log(`FAIL  ${error.message}`);
} finally {
  // 干净登出（ESC 预选 Quit Game → Enter 发 PLAYER_DC+WS Close，见 logout.mjs 头注）
  // 替代"杀浏览器+等 ws-proxy 残留 2-3 分钟"；失败仅告警，保底仍杀浏览器。
  try {
    const logoutState = await logoutToLogin(driver, {
      timeoutMs: 30000,
      log: message => console.log(`INFO  [logout] ${message}`),
    });
    console.log(logoutState === uiState.login
      ? `PASS  游戏内干净登出（msui_state=${logoutState}）`
      : `WARN  登出后停在 msui_state=${logoutState}（已离开游戏态，WS 已关）`);
    const freed = await waitForAccountLoggedOut(account, { timeoutMs: 15000 });
    console.log(freed.ok
      ? `PASS  登出后 DB loggedin=0（耗时 ${(freed.waitedMs / 1000).toFixed(1)}s）`
      : `WARN  登出后 15s 内 loggedin 未归零（last=${freed.lastValue}），下轮登录可能需重试`);
  } catch (error) {
    console.log(`WARN  干净登出失败：${error.message}（保底杀浏览器）`);
  }
  await driver.stop();
}
console.log(failed ? 'FAILED' : 'ALL CHECKS PASSED');
process.exit(failed ? 1 : 0);
