// 自由市场仓库 NPC 全链路 e2e（storage-flow 只测开窗几何、deposit 只测单类型
// ETC 存取；本文件在 910000000 验证新增仓库 NPC 与多类型/满仓场景）:
//  1. plife 注入的仓库 NPC 1100000 可被 Space 唤起开仓；
//  2. 仓库混入装备后 0x16 解析不串位（15 short=30 字节，修复前 skip(28) 错读）；
//  3. 取出发送"页签内索引"：ETC tab 行0 必须命中 4000001（修复前桶映射错位）；
//  4. 满仓存入被 0x11 拒；5. 空位存入成功；6. ESC 关窗。前置：test2 卡0=test2ILA。

import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, waitForAccountLoggedOut } from './logout.mjs';
import { waitForMapLoad } from './support/fm-map-retry.mjs';
import { loginAndEnterGame as pqLogin } from './pq-harness-login.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.FM_STORAGE_DEBUG_PORT || 9298);
const pageUrl = process.env.FM_STORAGE_URL || 'http://127.0.0.1:8000/web/index.html';
const profileDir = process.env.FM_STORAGE_PROFILE || '/tmp/maplestory-webui-fm-storage';
const artifactDir = process.env.FM_STORAGE_ARTIFACT_DIR || '/tmp/fm-storage-flow';
const configPath = process.env.MS_SERVER_CONFIG || '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const account = process.env.STORAGE_ACCOUNT || 'test2';
const password = process.env.STORAGE_PASSWORD || 'test2';
const cardIndex = Number(process.env.STORAGE_CARD ?? 0);
const CHARACTER_ID = 219;
const ETC_INVTYPE = 4, EQUIP_INVTYPE = 1;
const EQUIP_ITEM = 1302001, STO_ITEM = 4000001, STO_QTY = 5, FILLER_COUNT = 28;
const INV_ITEM = 4000003, INV_QTY = 2, FILLER_ITEM = 4000000; // 背包存入物 / 占位 ×28 满仓
const FM_MAP = 910000000;
const NPC_X = 450;                   // plife 注入的 1100000 Kirium
const STAND_X = 450;                 // 站 NPC 正上（dx=0，避免 18 个原生 NPC 抢最近）
const STORAGE_UI = 15, NOTICE_UI = 12;
const MAX_SLOTS = 30;                // MapleStorage.MAX_SLOTS

// legacy 463x318 窗口（storage-flow/deposit-flow 实测同款几何）
const WINDOW_X = 168, WINDOW_Y = 81;
const LEFT_ICON_X = 178, RIGHT_ICON_X = 409, LEFT_CLICK_X = 270, RIGHT_CLICK_X = 500;
const RECT_W = 28, RECT_H = 30, ROW0_Y = 177, ROW_PITCH = 40;
const rowRect = (iconX, row) => ({ x: iconX, y: ROW0_Y + ROW_PITCH * row, w: RECT_W, h: RECT_H });
const rowClick = (colX, row) => ({ x: colX, y: ROW0_Y + 13 + ROW_PITCH * row });
const CURSOR_PARK = { x: 470, y: 340 };
const DIFF_PRESENT = 10;
// 右条带 tab 矩形 local x∈[248+34i,248+34i+27] y∈[59,75]（ETC i=2 与 deposit-flow 同坐标）
const tabCenter = i => ({ x: WINDOW_X + 248 + 34 * i + 13, y: WINDOW_Y + 67 });

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

function dbConfig(file) {
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
const awaitDbValue = async (db, sql, expect, seconds, label) => {
  let last = '';
  for (let i = 0; i <= seconds * 2 && (last = mysqlRun(db, sql)) !== expect; i += 1) await sleep(500);
  report(last === expect, `${label}（期望 ${expect} 实际 ${last}）`);
};
function prepareDatabase(db) {
  const accountId = Number(mysqlRun(db, `SELECT id FROM accounts WHERE name='${account}';`));
  const storageId = Number(mysqlRun(db, `SELECT storageid FROM storages WHERE accountid=${accountId};`));
  const slots = Number(mysqlRun(db, `SELECT slots FROM storages WHERE storageid=${storageId};`));
  report(slots === MAX_SLOTS, `storages.slots=${MAX_SLOTS}（实际 ${slots}）`);
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  mysqlRun(db, `UPDATE characters SET map=${FM_MAP}, spawnpoint=0 WHERE id=${CHARACTER_ID};`);
  mysqlRun(db, `DELETE FROM inventoryequipment WHERE inventoryitemid IN
      (SELECT inventoryitemid FROM inventoryitems WHERE type=2 AND accountid=${storageId});`
    + ` DELETE FROM inventoryitems WHERE type=2 AND accountid=${storageId};`
    + ` DELETE FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID}
        AND itemid IN (${EQUIP_ITEM},${STO_ITEM},${INV_ITEM});`);
  // 30 行满仓：装备在前（排序后 EQUIP 仍为行0），4000001 为首个 ETC
  mysqlRun(db, `INSERT INTO inventoryitems VALUES (DEFAULT, 2, NULL, ${storageId}, ${EQUIP_ITEM}, ${EQUIP_INVTYPE}, 1, 1, '', -1, 0, -1, '');`
    + ` SET @eq := LAST_INSERT_ID();`
    + ` INSERT INTO inventoryequipment VALUES (DEFAULT, @eq, 7, 0, 4, 0, 0, 0, 0, 0, 15, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, -1);`
    + ` INSERT INTO inventoryitems VALUES (DEFAULT, 2, NULL, ${storageId}, ${STO_ITEM}, ${ETC_INVTYPE}, 2, ${STO_QTY}, '', -1, 0, -1, '');`);
  for (let i = 0; i < FILLER_COUNT; i += 1) {
    mysqlRun(db, `INSERT INTO inventoryitems VALUES (DEFAULT, 2, NULL, ${storageId}, ${FILLER_ITEM}, ${ETC_INVTYPE}, ${3 + i}, 1, '', -1, 0, -1, '');`);
  }
  mysqlRun(db, `INSERT INTO inventoryitems VALUES (DEFAULT, 1, ${CHARACTER_ID}, NULL, ${INV_ITEM}, ${ETC_INVTYPE}, 1, ${INV_QTY}, '', -1, 0, -1, '');`);
  const rows = Number(mysqlRun(db, `SELECT COUNT(*) FROM inventoryitems WHERE type=2 AND accountid=${storageId};`));
  report(rows === MAX_SLOTS, `DB 预备：仓库注入 ${rows} 行满仓（storageId=${storageId}，装备 ${EQUIP_ITEM} + ${STO_ITEM} x${STO_QTY} + ${FILLER_ITEM} x${FILLER_COUNT}）`);
  return { accountId, storageId };
}

function restoreDatabase(db, ids) {
  try {
    mysqlRun(db, `DELETE FROM inventoryequipment WHERE inventoryitemid IN
        (SELECT inventoryitemid FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId});`
      + ` DELETE FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId};`
      + ` DELETE FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID}
          AND itemid IN (${EQUIP_ITEM},${STO_ITEM},${INV_ITEM});`
      + ` UPDATE characters SET map=${FM_MAP}, spawnpoint=0 WHERE id=${CHARACTER_ID};`
      + ` UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
    console.log('INFO  恢复 SQL 已执行（注入行删除、loggedin=0）');
  } catch (error) { report(false, `恢复 SQL 失败: ${error.message}`); }
}

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
            total += Math.abs(a.buf[(y * a.w + x) * 4 + c] - b.buf[(y * a.w + x) * 4 + c]);
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
const doubleClickRow = async (driver, colX, row) => {
  const p = rowClick(colX, row);
  await driver.doubleClick(p.x, p.y);
};
const parkCursor = async driver => { await driver.move(CURSOR_PARK.x, CURSOR_PARK.y); await sleep(250); };

async function walkTowards(driver, targetX, maxSeconds = 60) {
  const start = Date.now();
  let x = await probe(driver, 'msplayer_x');
  while (Date.now() - start < maxSeconds * 1000 && Math.abs(targetX - x) > 10) {
    const ms = Math.abs(targetX - x) > 400 ? 500 : Math.abs(targetX - x) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
    x = await probe(driver, 'msplayer_x');
  }
  if (Math.abs(x - targetX) > 10) throw new Error(`walk timeout: x=${x} target=${targetX}`);
  return x;
};

async function loginAndEnterGame(driver) {
  // 复用 pq-harness 的健壮登录：处理键盘丢字符(3)/账号错(20)/已登录(7)
  // 公告重试与页面重载；FM 地图资源重，进场后地图加载失败用 msmap_retry 兜底。
  try {
    await pqLogin(driver, { account, password, cardIndex, pageUrl });
  } catch (error) {
    if (!/in-game assets never idle/.test(error.message)) throw error;
    console.log(`INFO  进场资源等待超时，转入地图重试：${error.message}`);
  }
  await waitForMapLoad(driver, FM_MAP);
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
}

async function openStorage(driver) {
  const mapId = await probe(driver, 'msmap_id');
  if (mapId !== FM_MAP) throw new Error(`地图不符：期望 ${FM_MAP} 实际 ${mapId}`);
  await walkTowards(driver, 300);           // 两段走位（出生点在 x<0，先到平台中部）
  await walkTowards(driver, STAND_X);
  await sleep(1500);                        // 资源刚就绪时按键可能被丢弃，多等一拍
  for (let attempt = 0; attempt < 4; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(() => frontAt(driver, 400, 240).then(v => v === STORAGE_UI), 8)) return;
    const front = await frontAt(driver, 400, 240);
    console.log(`INFO  Space 尝试 ${attempt + 1}: front=${front} px=${await probe(driver, 'msplayer_x')} py=${await probe(driver, 'msplayer_y')}`);
    if (front !== 0 && front !== STORAGE_UI) await driver.key('Escape', 'Escape', 27);
    await sleep(800);
  }
  throw new Error('Space 未打开仓库窗口（FM 仓库 NPC 1100000 未就位？）');
}

async function selectTab(driver, index) {
  const tab = tabCenter(index);
  await driver.click(tab.x, tab.y);
  await sleep(600);
  return await frontAt(driver, 400, 240) === STORAGE_UI;
}
async function closeStorageByButton(driver) {
  await driver.click(306, 364);   // 羊皮纸条 BtExit（storage-flow 同款）
  const closed = await waitFor(() => frontAt(driver, 400, 240).then(v => v !== STORAGE_UI), 8);
  report(closed, '退出按钮关闭仓库窗口');
  await sleep(400);
}

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

const confirmQuantity = async (driver, qty) => {
  await driver.key(String(qty), `Digit${qty}`, 48 + qty, String(qty));
  await sleep(300);
  await driver.key('Enter', 'Enter', 13);
  await sleep(400);
};
// Enter 提交后 UIEnterNumber 自关；仅当残留错误 notice（front=NOTICE）才逐层 ESC。
// 无条件 ESC 会命中 STORAGE（escape_types）误发 leave 包关掉仓库窗口。
const dismissNotices = async (driver) => {
  for (let layers = 0; layers < 3; layers += 1) {
    if (await frontAt(driver, 400, 240) !== NOTICE_UI) return layers;
    await driver.key('Escape', 'Escape', 27);
    await sleep(400);
  }
  return 3;
};
async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const db = dbConfig(configPath);
  const ids = prepareDatabase(db);
  const driver = new BrowserDriver({ debugPort, binary: chromeBinary, profileDir, artifactDir });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    console.log('INFO  进场 FM 入口，唤醒仓库 NPC（0x16 全量下发段）');
    await openStorage(driver);
    report(true, 'FM 仓库 NPC 1100000 Space 开仓（front=15）');

    // ---- ETC tab：装备混存下解析不串位（修复前 skip(28) 会让后续条目错读） ----
    report(await selectTab(driver, 2), 'ETC tab 点击后窗口保持打开');
    await parkCursor(driver);
    const leftEtc0 = await rowDiff(driver, LEFT_ICON_X, 0);
    const rightEtc0 = await rowDiff(driver, RIGHT_ICON_X, 0);
    report(leftEtc0 !== null && leftEtc0 >= DIFF_PRESENT,
      `ETC tab 左列行0 仓库 ${STO_ITEM} 可见（装备混存解析不串位，diff=${leftEtc0?.toFixed(1)}）`);
    report(rightEtc0 !== null && rightEtc0 >= DIFF_PRESENT,
      `ETC tab 右列行0 背包 ${INV_ITEM} x${INV_QTY} 可见（diff=${rightEtc0?.toFixed(1)}）`);
    await driver.screenshot('01-fm-storage-etc-tab');

    // ---- 满仓存入被拒：双击右列行0 → 数量框 → 0x11 notice「仓库已满」 ----
    console.log('INFO  满仓存入段（30/30 格 → 0x11）');
    await doubleClickRow(driver, RIGHT_CLICK_X, 0);
    await sleep(400);
    if (!await waitFor(() => frontAt(driver, 400, 240).then(v => v === NOTICE_UI), 4)) {
      report(false, '满仓存入：数量框未弹出（front!=12）');
    } else {
      // 提交后 0x11「仓库已满」notice 会叠在数量框上（同为 NOTICE 层），逐层 ESC 收掉
      await confirmQuantity(driver, 1);
      await driver.screenshot('02-storage-full-notice');
      const fullLayers = await dismissNotices(driver);
      report(await frontAt(driver, 400, 240) === STORAGE_UI,
        `满仓存入被拒：notice 已收（ESC x${fullLayers}），仓库窗口回到前台`);
    }

    // ---- 取出 ETC：页签内索引必须命中 4000001 而不是排序在前的装备 ----
    console.log('INFO  取出段（页签内索引映射）');
    await doubleClickRow(driver, LEFT_CLICK_X, 0);
    await sleep(500);
    if (await waitFor(() => frontAt(driver, 400, 240).then(v => v === NOTICE_UI), 2)) {
      await confirmQuantity(driver, STO_QTY);
    }
    await parkCursor(driver);
    report(await frontAt(driver, 400, 240) === STORAGE_UI, '取出后仓库窗口仍在');
    await driver.screenshot('03-after-etc-takeout');

    // ---- EQUIP tab：装备行可见（30 字节属性块解析）→ 双击取出 ----
    console.log('INFO  EQUIP tab 取出段');
    report(await selectTab(driver, 0), 'EQUIP tab 点击后窗口保持打开');
    await parkCursor(driver);
    const leftEq0 = await rowDiff(driver, LEFT_ICON_X, 0);
    report(leftEq0 !== null && leftEq0 >= 4,   // 装备图标像素质量与 ETC 不同，低阈值
      `EQUIP tab 左列行0 仓库 ${EQUIP_ITEM} 可见（diff=${leftEq0?.toFixed(1)}）`);
    await doubleClickRow(driver, LEFT_CLICK_X, 0);
    await sleep(600);
    report(await frontAt(driver, 400, 240) === STORAGE_UI, '装备取出后仓库窗口仍在');
    await driver.screenshot('04-after-equip-takeout');
    await closeStorageByButton(driver);

    // ---- 现金商城存档点：DB 断言取出/拒绝结果 ----
    await enterCashShop(driver);
    console.log('INFO  现金商城存档点，DB 断言');
    await awaitDbValue(db,
      `SELECT IFNULL(SUM(quantity),-1) FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid=${STO_ITEM};`,
      String(STO_QTY), 20, `取出 DB：背包出现 ${STO_ITEM} x${STO_QTY}（页签内索引命中 ETC 而非装备）`);
    await awaitDbValue(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId} AND itemid=${STO_ITEM};`,
      '0', 20, `取出 DB：仓库 ${STO_ITEM} 归零`);
    await awaitDbValue(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid=${EQUIP_ITEM} AND inventorytype=${EQUIP_INVTYPE};`,
      '1', 20, `取出 DB：背包 EQUIP 出现 ${EQUIP_ITEM}（含 inventoryequipment 行）`);
    await awaitDbValue(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId};`,
      String(MAX_SLOTS - 2), 20, `满仓拒存 DB：仓库仍 ${MAX_SLOTS - 2} 行（30 格时 ${INV_ITEM} 未混入）`);
    await driver.screenshot('05-cashshop-save');
    await exitCashShop(driver);

    // ---- 空位后存入成功（trunkPut=100 费用链路）→ ESC 关窗（leave 包） ----
    console.log('INFO  重开仓（存入 + ESC 段）');
    await openStorage(driver);
    report(await selectTab(driver, 2), '重开仓：ETC tab 后窗口保持打开');
    await doubleClickRow(driver, RIGHT_CLICK_X, 0);
    await sleep(400);
    report(await waitFor(() => frontAt(driver, 400, 240).then(v => v === NOTICE_UI), 4),
      '存入：数量框弹出（front=12）');
    await confirmQuantity(driver, 1);
    const storeLayers = await dismissNotices(driver);   // 成功时 0 层，不误关仓库窗
    const storeFront = await frontAt(driver, 400, 240);
    console.log(`INFO  存入后 front=${storeFront}（dismiss x${storeLayers}）`);
    report(storeFront === STORAGE_UI,
      `存入成功无错误 notice，仓库窗口在前（front=${storeFront}，ESC x${storeLayers}）`);
    await parkCursor(driver);
    const leftStored = await rowDiff(driver, LEFT_ICON_X, 1);
    report(leftStored !== null && leftStored >= DIFF_PRESENT,
      `存入后左列行1 出现 ${INV_ITEM}（diff=${leftStored?.toFixed(1)}）`);
    await driver.screenshot('06-after-store');
    await driver.key('Escape', 'Escape', 27);
    report(await waitFor(() => frontAt(driver, 400, 240).then(v => v !== STORAGE_UI), 6),
      'ESC 关闭仓库窗口（escape_types 含 STORAGE）');
    await enterCashShop(driver);
    await awaitDbValue(db,
      `SELECT IFNULL(SUM(quantity),-1) FROM inventoryitems WHERE type=2 AND accountid=${ids.storageId} AND itemid=${INV_ITEM};`,
      '1', 20, `存入 DB：仓库出现 ${INV_ITEM} x1`);
    await awaitDbValue(db,
      `SELECT IFNULL(SUM(quantity),-1) FROM inventoryitems WHERE type=1 AND characterid=${CHARACTER_ID} AND itemid=${INV_ITEM};`,
      String(INV_QTY - 1), 20, `存入 DB：背包 ${INV_ITEM} 剩 x${INV_QTY - 1}`);
    await driver.screenshot('07-cashshop-save-store');
    await exitCashShop(driver);
  } catch (error) {
    report(false, `链路中断: ${error.message}`);
    await driver.screenshot('00-failure');
  } finally {
    if (driver.errors?.length) {
      fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    if (driver.logs?.length) {
      fs.writeFileSync(`${artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    }
    console.log(`INFO  产物目录 ${artifactDir}`);
    try {
      const logoutState = await logoutToLogin(driver, {
        timeoutMs: 30000,
        log: message => console.log(`INFO  [logout] ${message}`),
      });
      report(logoutState === uiState.login, `游戏内干净登出（msui_state=${logoutState}）`);
      const freed = await waitForAccountLoggedOut(account, { timeoutMs: 15000, db });
      report(freed.ok, `登出后 DB loggedin=0（耗时 ${(freed.waitedMs / 1000).toFixed(1)}s）`);
    } catch (error) {
      console.log(`WARN  干净登出失败：${error.message}`);
    }
    await driver.stop();
    restoreDatabase(db, ids);
  }

  console.log(failed ? 'FAILED' : 'ALL CHECKS PASSED');
  process.exit(failed ? 1 : 0);
}
main().catch(error => { console.error(error); process.exit(1); });
