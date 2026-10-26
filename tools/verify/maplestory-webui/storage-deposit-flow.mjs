// 仓库取出+存入动作全链路 e2e（storage-flow.mjs 只覆盖开窗/布局/关窗，本文件覆盖
// CP_Storage 动作封包闭环：0x16 全量 / 0x09 取出 / 0x0D 存入）。
// 账号 test2 卡0 test2ILA（charid 219，等级 200），130000200 sp0 走到 x≈1385 距储物
// NPC 后按 Space 开仓。客户端语义（UIStorage::button_pressed）：同行二次点击触发动作
// ——左列（仓库）二次点击=TAKE_OUT(4) 整堆取回（不弹数量框）；右列（背包镜像）非
// EQUIP 且数量>1 时弹 UIEnterNumber（默认 1 全选，Enter=STORE(5)）。链路：
//  1. 前置 SQL：仓库注入 4000001 x5（ItemFactory.STORAGE 以 storageid 为键，accountid
//     列实际存 storageid，test2→17），背包 ETC slot1 注入 4000000 x3（type=1）。
//  2. 登录进图 → Space 开仓 → ETC tab → 像素 diff：左列行0=仓库注入物品（0x16）、
//     右列行0=背包物品。
//  3. 取出：双击左列行0 → TAKE_OUT 整堆（不弹数量框，若弹则输 5+Enter）→ 左列行0 变空；
//     进现金商城（落 saveCharToDB 存档）→ DB 断言：背包 4000001 x5、仓库归零。
//  4. 存入：重开仓 → 双击右列行0 → UIEnterNumber 输 3 + Enter（Esc 关闭）→ 右列行1 变空
//     （右列按非空槽顺序排列，取回的 4000001 前移行0）、左列行0 出现该物品（0x0D）；
//     现金商城存档 → DB 断言：仓库 4000000 x3、背包归零。
//  5. 再开仓：左列行0 仍可见（0x16）、右列仅剩取回物品。finally 恢复 SQL。
// 前置：build/ 最新；java 服务端在跑且 configuration.ini 可读；收尾为游戏内干净登出
// （logout.mjs：PLAYER_DC+WS Close → 服务端清 loggedin），可背靠背连跑，无需重启 ws-proxy。
// 运行：CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/storage-deposit-flow.mjs
// 产物：/tmp/storage-deposit-flow（截图）；exit 0 仅当全部检查通过。

import fs from 'fs';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, waitForAccountLoggedOut } from './logout.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.STORAGE_DEPOSIT_DEBUG_PORT || 9333);
const pageUrl = process.env.STORAGE_DEPOSIT_URL || 'http://127.0.0.1:8000/web/index.html';
const profileDir = process.env.STORAGE_DEPOSIT_PROFILE || '/tmp/maplestory-webui-deposit';
const artifactDir = process.env.STORAGE_DEPOSIT_ARTIFACT_DIR || '/tmp/storage-deposit-flow';
const configPath = process.env.MS_SERVER_CONFIG || '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const account = process.env.STORAGE_ACCOUNT || 'test2', password = process.env.STORAGE_PASSWORD || 'test2';
const cardIndex = Number(process.env.STORAGE_CARD ?? 0);

const CHARACTER_ID = 219;
const STO_ITEM = 4000001;          // ETC 可堆叠（仓库侧注入，供取出链路）
const STO_QTY = 5;
const INV_ITEM = 4000000;          // ETC 可堆叠（slotMax 200，icon 26x27 origin 0,0）
const INV_QTY = 3;
const ETC_INVTYPE = 4;             // MapleInventoryType.ETC
const STORAGE_MAP = 130000200;
const STAND_X = 1385;              // storage-flow 同款站位
const STORAGE_UI = 15, NOTICE_UI = 12;

// legacy 463x318 窗口（storage-flow 实测）：窗左上 (168,81)；行带 canvas y 176..211、
// 行距 40；图标锚点左列 x=178 / 右列 x=409、y=167+40i。采样矩形取图标列 x±28、行带内
// y 177+40i 高 30，基线取同列行3（两侧条目均 <=2，必然为空）。
const LEFT_ICON_X = 178, RIGHT_ICON_X = 409, LEFT_CLICK_X = 270, RIGHT_CLICK_X = 500;
const RECT_W = 28, RECT_H = 30, ROW0_Y = 177, ROW_PITCH = 40;
const rowRect = (iconX, row) => ({ x: iconX, y: ROW0_Y + ROW_PITCH * row, w: RECT_W, h: RECT_H });
const rowClick = (colX, row) => ({ x: colX, y: ROW0_Y + 13 + ROW_PITCH * row });
const CURSOR_PARK = { x: 470, y: 340 };  // 行带下方羊皮纸空白，避免 tooltip 污染采样
const DIFF_PRESENT = 10;                 // 行有图标/数量字 vs 空行的平均像素差阈值

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };
const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const frontAt = (driver, x, y) =>
  driver.evaluate(`Module.ccall('msui_front_at', 'number', ['number', 'number'], [${x}, ${y}])`);
async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}
function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 && await driver.evaluate(
    "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

// ---- DB 访问（cash-shop-pet-flow 同款） ----
function readDbConfig(file) {
  const values = Object.fromEntries(fs.readFileSync(file, 'utf8').split(/\r?\n/)
    .filter(line => line.indexOf('=') > 0)
    .map(line => [line.slice(0, line.indexOf('=')).trim(), line.slice(line.indexOf('=') + 1).trim()]));
  const match = /\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(values.URL || '');
  return { host: match?.[1] || '127.0.0.1', port: match?.[2] || '3306',
    database: match?.[3] || 'maplestory', user: values.DB_USER || 'root', password: values.DB_PASS || '' };
}
function mysqlRun(db, sql) {
  return execFileSync('mysql', ['-h', db.host, '-P', db.port, '-u', db.user, db.database,
    '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password }, stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}
const accountIdOf = db => Number(mysqlRun(db, `SELECT id FROM accounts WHERE name='${account}';`));

// 仓库物品行落在 `type=2 AND accountid=<storageid>`（ItemFactory.STORAGE 以
// storageid 为键而非账号 id），注入与断言都必须用 storageid 检索仓库行。
function storageIdOf(db, accountId) {
  const rows = mysqlRun(db, `SELECT storageid FROM storages WHERE accountid=${accountId};`);
  const ids = rows.split('\n').filter(Boolean);
  report(ids.length === 1, `storages 唯一行（实际 ${ids.length} 行：${rows.replace(/\n/g, ',')}）`);
  return Number(ids[0]);
}

function prepareDatabase(db) {
  const accountId = accountIdOf(db);
  const storageId = storageIdOf(db, accountId);
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  mysqlRun(db, `UPDATE characters SET map=${STORAGE_MAP}, spawnpoint=0 WHERE id=${CHARACTER_ID};`);
  mysqlRun(db, `DELETE FROM inventoryitems WHERE type=2 AND accountid IN (${accountId},${storageId});`);
  mysqlRun(db, `DELETE FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid IN (${INV_ITEM},${STO_ITEM});`);
  mysqlRun(db, `INSERT INTO inventoryitems VALUES (DEFAULT, 2, NULL, ${storageId}, ${STO_ITEM}, ${ETC_INVTYPE}, 1, ${STO_QTY}, '', -1, 0, -1, '');`);
  mysqlRun(db, `INSERT INTO inventoryitems VALUES (DEFAULT, 1, ${CHARACTER_ID}, NULL, ${INV_ITEM}, ${ETC_INVTYPE}, 1, ${INV_QTY}, '', -1, 0, -1, '');`);
  const storage = mysqlRun(db, `SELECT itemid, quantity FROM inventoryitems WHERE type=2 AND accountid=${storageId};`);
  const invEtc = mysqlRun(db, `SELECT itemid, position, quantity FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND inventorytype=${ETC_INVTYPE};`);
  report(storage === `${STO_ITEM}\t${STO_QTY}` && invEtc === `${INV_ITEM}\t1\t${INV_QTY}`,
    `DB 预备：仓库注入 ${STO_ITEM} x${STO_QTY}（storageId=${storageId}），背包ETC=[${invEtc.replace(/\t/g, ' ')}]`);
  return { accountId, storageId };
}

function restoreDatabase(db, ids) {
  try {
    mysqlRun(db, `DELETE FROM inventoryitems WHERE type=2 AND accountid IN (${ids.accountId},${ids.storageId});`
      + ` DELETE FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid IN (${INV_ITEM},${STO_ITEM});`
      + ` UPDATE characters SET map=910000000, spawnpoint=0 WHERE id=${CHARACTER_ID};`
      + ` UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
    console.log('INFO  恢复 SQL 已执行（注入行删除、map 复位、loggedin=0）');
  } catch (error) { report(false, `恢复 SQL 失败: ${error.message}`); }
}

async function awaitDbValue(db, sql, expect, seconds, label) {
  let last = '';
  for (let i = 0; i <= seconds * 2 && (last = mysqlRun(db, sql)) !== expect; i += 1) await sleep(500);
  report(last === expect, `${label}（期望 ${expect} 实际 ${last}）`);
}

// ---- WebGL 行像素差（storage-flow 采样同款：rAF 内 readPixels + Y 翻转 + 缩放） ----
async function rectDiff(driver, rectA, rectB) {
  const expression = `(() => new Promise(resolve => {
    requestAnimationFrame(() => {
      const canvas = document.getElementById('canvas') || document.querySelector('canvas');
      const gl = canvas && (canvas.getContext('webgl2') || canvas.getContext('experimental-webgl'));
      if (!gl) return resolve(null);
      const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
      const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      const read = (r) => {
        const w = Math.max(1, Math.round(r.w * scaleX)), h = Math.max(1, Math.round(r.h * scaleY));
        const buf = new Uint8Array(w * h * 4);
        gl.readPixels(Math.round(r.x * scaleX), canvas.height - Math.round((r.y + r.h) * scaleY),
          w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        return { w, h, buf };
      };
      const a = read(${JSON.stringify(rectA)}), b = read(${JSON.stringify(rectB)});
      gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
      let total = 0, count = 0;
      for (let y = 0; y < Math.min(a.h, b.h); y += 1)
        for (let x = 0; x < Math.min(a.w, b.w); x += 1)
          for (let c = 0; c < 3; c += 1) {
            total += Math.abs(a.buf[(y * a.w + x) * 4 + c] - b.buf[(y * b.w + x) * 4 + c]);
            count += 1;
          }
      resolve(count ? total / count : null);
    });
  }))()`;
  const envelope = await driver.send('Runtime.evaluate',
    { returnByValue: true, awaitPromise: true, expression });
  return envelope.result.value;
}

const rowDiff = (driver, iconX, row) => rectDiff(driver, rowRect(iconX, row), rowRect(iconX, 3));
const rowEmpty = (driver, iconX, row = 0) => waitFor(async () => {
  const d = await rowDiff(driver, iconX, row);
  return d !== null && d < DIFF_PRESENT;
}, 8);
const parkCursor = async driver => { await driver.move(CURSOR_PARK.x, CURSOR_PARK.y); await sleep(250); };

// ---- 登录/行走/开仓（storage-flow 同款） ----
async function walkTowards(driver, targetX, maxSeconds = 60) {
  const start = Date.now();
  let x = await probe(driver, 'msplayer_x');
  while (Date.now() - start < maxSeconds * 1000 && Math.abs(targetX - x) > 10) {
    const dx = targetX - x, ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (dx > 0) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
    x = await probe(driver, 'msplayer_x');
  }
  if (Math.abs(x - targetX) > 10) throw new Error(`walk timeout: x=${x} want=${targetX}`);
  return x;
}

// 被杀浏览器经 ws-proxy 留下半开 TCP，服务端 anti-multiclient 会拒绝下一次
// CHAR_SELECT，故登录/进图整体重试（每轮先 Enter/Escape 清错误框）。
async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30))
    throw new Error('login UI did not activate');
  const stateIs = async target => await probe(driver, 'msui_state') === target;
  for (let round = 0; round < 3; round += 1) {
    if (round > 0) { await driver.key('Escape', 'Escape', 27); await driver.key('Enter', 'Enter', 13); await sleep(1500); }
    // 登录资产流式加载时 CDP 点击会被丢弃，先等 idle 再点字段。
    await waitFor(assetsIdle(driver), 60);
    try {
      await driver.clickField(340, 260);
    } catch (error) {
      console.log(`INFO  第${round}轮账号框未获焦（state=${await probe(driver, 'msui_state')}）重试`);
      continue;
    }
    await driver.typeAscii(account);
    await driver.clickField(340, 285);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    if (!await waitFor(() => stateIs(uiState.world), 25)) continue;
    await driver.retryClick(510, 216, () => stateIs(uiState.characters));
    if (!await waitFor(() => stateIs(uiState.characters), 25)) continue;
    if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
    const center = cardCenter(cardIndex);
    let entered = false;
    for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
      await driver.doubleClick(center.x, center.y);
      entered = await waitFor(() => stateIs(uiState.game), 6);
      if (!entered) { await driver.key('Enter', 'Enter', 13); await sleep(800); }
      if (await stateIs(uiState.login)) break;   // 被弹框打回登录，重开一轮
    }
    if (entered) {
      if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
      return;
    }
    await sleep(1500);
  }
  throw new Error('login/enter-game 三轮均失败（ws-proxy 会话残留？等 2-3 分钟重试）');
}

async function openStorage(driver) {
  const mapId = await probe(driver, 'msmap_id');
  if (mapId !== STORAGE_MAP)
    throw new Error(`地图不符：期望 ${STORAGE_MAP} 实际 ${mapId}`);
  await walkTowards(driver, STAND_X);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(() => frontAt(driver, 400, 240).then(v => v === STORAGE_UI), 8)) return;
    await sleep(500);
  }
  throw new Error('Space 未打开仓库窗口（|dx|<=120, |dy|<=80 判定失败？）');
}

async function selectEtcTab(driver) {
  // ETC tab（右条带 index2）标签锚 local (319,59) 取中心 canvas (503,146)；
  // 左边缘 (319,63) 实测无效。
  await driver.click(168 + 335, 81 + 65);
  await sleep(600);
  return await frontAt(driver, 400, 240) === STORAGE_UI;
}

async function closeStorage(driver) {
  // 退出按钮（羊皮纸条 BtExit）：canvas (306, 364)，storage-flow 同款
  await driver.click(306, 364);
  const closed = await waitFor(() => frontAt(driver, 400, 240).then(v => v !== STORAGE_UI), 8);
  report(closed, '退出按钮关闭仓库窗口');
  await sleep(400);
}

// 现金商城入口=服务端存档点（EnterCashShopHandler → saveCharToDB 立即写库）
async function enterCashShop(driver) {
  for (const [x, y] of [[597, 571], [562, 583]]) {
    await driver.click(x, y);
    if (await waitFor(() => probe(driver, 'msui_state').then(v => v === uiState.cashShop), 15)) {
      if (!await waitFor(assetsIdle(driver), 60)) throw new Error('cash-shop assets never idle');
      return;
    }
  }
  throw new Error('cash-shop UI did not activate（无法触发服务端存档）');
}

async function exitCashShop(driver) {
  await driver.key('Escape', 'Escape', 27);
  if (!await waitFor(() => probe(driver, 'msui_state').then(v => v === uiState.game), 30))
    throw new Error('cash-shop exit did not return to game');
  if (!await waitFor(assetsIdle(driver), 90)) throw new Error('post-cashshop assets never idle');
  await sleep(500);
}

// 数量确认：输数量 + Enter（提交），Esc 关闭对话框（提交后不自动关）
async function confirmQuantity(driver, qty) {
  await driver.key(String(qty), `Digit${qty}`, 48 + qty, String(qty));
  await sleep(300);
  await driver.key('Enter', 'Enter', 13);
  await sleep(400);
  await driver.key('Escape', 'Escape', 27);
  await sleep(400);
}

async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const db = readDbConfig(configPath);
  const ids = prepareDatabase(db);

  const driver = new BrowserDriver({ debugPort, binary: chromeBinary, profileDir, artifactDir });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    console.log('INFO  进入游戏，开仓（0x16 全量下发段）');
    await openStorage(driver);
    report(await selectEtcTab(driver), 'ETC tab 点击后窗口保持打开');
    await parkCursor(driver);
    const left0 = await rowDiff(driver, LEFT_ICON_X, 0);
    const right0 = await rowDiff(driver, RIGHT_ICON_X, 0);
    report(left0 !== null && left0 >= DIFF_PRESENT,
      `左列行0 仓库 ${STO_ITEM} x${STO_QTY} 可见（diff=${left0?.toFixed(1)}）`);
    report(right0 !== null && right0 >= DIFF_PRESENT,
      `右列行0 背包 ${INV_ITEM} x${INV_QTY} 可见（diff=${right0?.toFixed(1)}）`);
    await driver.screenshot('01-storage-etc-tab');

    // ---- 取出：双击左列行0（一次选中、二次 TAKE_OUT(4) 整堆取回） ----
    const take = rowClick(LEFT_CLICK_X, 0);
    await driver.doubleClick(take.x, take.y);
    await sleep(500);
    // request_take_selected 直接发 TAKE_OUT 不弹数量框；若语义变化弹出则按全量确认
    if (await waitFor(() => frontAt(driver, 400, 240).then(v => v === NOTICE_UI), 2)) {
      console.log('INFO  取出弹出了数量框（非预期路径），按全量确认');
      await confirmQuantity(driver, STO_QTY);
    }
    await parkCursor(driver);
    const taken = await rowEmpty(driver, LEFT_ICON_X);
    const left0Taken = await rowDiff(driver, LEFT_ICON_X, 0);
    report(taken, `取出后左列行0 变空（0x09 下发，diff=${left0Taken?.toFixed(1)}）`);
    report(await frontAt(driver, 400, 240) === STORAGE_UI, '取出后仓库窗口仍在');
    await driver.screenshot('02-after-takeout');

    await closeStorage(driver);
    await enterCashShop(driver);
    console.log('INFO  现金商城存档点#1，DB 断言取出结果');
    await awaitDbValue(db,
      `SELECT IFNULL(SUM(quantity),-1) FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid=${STO_ITEM};`,
      String(STO_QTY), 20, `取出 DB：背包出现 ${STO_ITEM} x${STO_QTY}`);
    await awaitDbValue(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId} AND itemid=${STO_ITEM};`,
      '0', 20, `取出 DB：仓库 ${STO_ITEM} 归零`);
    await driver.screenshot('03-cashshop-save-takeout');
    await exitCashShop(driver);

    // ---- 存入：双击右列行0（一次选中、二次 STORE）→ UIEnterNumber 输 3 ----
    console.log('INFO  重开仓（存入段）');
    await openStorage(driver);
    report(await selectEtcTab(driver), '重开仓：ETC tab 后窗口保持打开');
    await parkCursor(driver);
    const store = rowClick(RIGHT_CLICK_X, 0);
    await driver.doubleClick(store.x, store.y);
    await sleep(500);
    const noticeOpen = await frontAt(driver, 400, 240);
    report(noticeOpen === NOTICE_UI, `数量框弹出（front=${noticeOpen} 期望 ${NOTICE_UI}）`);
    await confirmQuantity(driver, INV_QTY);
    await parkCursor(driver);
    const stored = await rowEmpty(driver, RIGHT_ICON_X, 1);
    const right1Stored = await rowDiff(driver, RIGHT_ICON_X, 1);
    const left0Stored = await rowDiff(driver, LEFT_ICON_X, 0);
    report(stored, `存入后右列行1 变空（${INV_ITEM} 已移出，diff=${right1Stored?.toFixed(1)}）`);
    report(left0Stored !== null && left0Stored >= DIFF_PRESENT,
      `存入后左列行0 出现 ${INV_ITEM}（0x0D 下发，diff=${left0Stored?.toFixed(1)}）`);
    report(await frontAt(driver, 400, 240) === STORAGE_UI, '关闭数量框后仓库窗口仍在');
    await driver.screenshot('04-after-store');

    await closeStorage(driver);
    await enterCashShop(driver);
    console.log('INFO  现金商城存档点#2，DB 断言存入结果');
    await awaitDbValue(db,
      `SELECT IFNULL(SUM(quantity),-1) FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId} AND itemid=${INV_ITEM};`,
      String(INV_QTY), 20, `存入 DB：仓库出现 ${INV_ITEM} x${INV_QTY}`);
    await awaitDbValue(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid=${INV_ITEM};`,
      '0', 20, `存入 DB：背包 ${INV_ITEM} 归零`);
    await driver.screenshot('05-cashshop-save-store');
    await exitCashShop(driver);

    // ---- 再开仓（0x16 全量下发）：仓库侧物品应可见 ----
    console.log('INFO  再开仓（0x16 复核段）');
    await openStorage(driver);
    report(await selectEtcTab(driver), '再开仓：ETC tab 后窗口保持打开');
    await parkCursor(driver);
    const left0Reopen = await rowDiff(driver, LEFT_ICON_X, 0);
    const right1Reopen = await rowDiff(driver, RIGHT_ICON_X, 1);
    report(left0Reopen !== null && left0Reopen >= DIFF_PRESENT,
      `重开仓左列行0 ${INV_ITEM} 可见（diff=${left0Reopen?.toFixed(1)}）`);
    report(right1Reopen !== null && right1Reopen < DIFF_PRESENT,
      `重开仓右列行1 为空（背包仅剩取回的 ${STO_ITEM}，diff=${right1Reopen?.toFixed(1)}）`);
    await driver.screenshot('06-reopen-storage');
    await closeStorage(driver);
  } catch (error) {
    report(false, `链路中断: ${error.message}`);
    await driver.screenshot('00-failure');
  } finally {
    if (driver.errors?.length) {
      fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    console.log(`INFO  产物目录 ${artifactDir}`);
    // 干净登出（ESC 预选 Quit Game → Enter 发 PLAYER_DC+WS Close，见 logout.mjs 头注）
    // 替代"杀浏览器+重启 ws-proxy"；失败仅告警，保底仍杀浏览器 + 恢复 SQL 置 loggedin=0。
    try {
      const logoutState = await logoutToLogin(driver, {
        timeoutMs: 30000,
        log: message => console.log(`INFO  [logout] ${message}`),
      });
      console.log(logoutState === uiState.login
        ? `PASS  游戏内干净登出（msui_state=${logoutState}）`
        : `WARN  登出后停在 msui_state=${logoutState}（已离开游戏态，WS 已关）`);
      const freed = await waitForAccountLoggedOut(account, { timeoutMs: 15000, db });
      console.log(freed.ok
        ? `PASS  登出后 DB loggedin=0（耗时 ${(freed.waitedMs / 1000).toFixed(1)}s）`
        : `WARN  登出后 15s 内 loggedin 未归零（last=${freed.lastValue}），下轮登录可能需重试`);
    } catch (error) {
      console.log(`WARN  干净登出失败：${error.message}（保底杀浏览器）`);
    }
    await driver.stop();
    restoreDatabase(db, ids);
  }
  console.log(failed ? 'FAILED' : 'ALL CHECKS PASSED');
  process.exit(failed ? 1 : 0);
}

await main();
