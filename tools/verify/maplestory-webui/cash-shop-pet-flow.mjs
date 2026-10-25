// 商城宠物全链路浏览器端到端验收：购买宠物 → 仓库取出 → 召唤 → 自动捡取 →
// 自动回血回蓝。覆盖：第 8 分类「宠物」过滤、SN 60000001 搜索高亮 5000001、
// 左栏卡片选中 → 右栏 BT_BUY 购买按钮 → 确认框弹窗确认、双击仓库槽取出、
// CASH 页召唤(能力掩码 31)、地图 40001 用 S(Slash Blast) 打怪自动捡取、
// keymap 91/92 绑 2000005 后压低 hp/mp 到 30% 重登自动嗑药恢复 99%+。
// 前置：WASM 构建含宠物分类；产物写 /tmp/cash-shop-pet-flow。
// Run: CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-pet-flow.mjs

import fs from 'fs';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { createRuntimeProbes } from './runtime-flow.mjs';

const artifactDir = process.env.PET_FLOW_ARTIFACT_DIR || '/tmp/cash-shop-pet-flow';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.PET_FLOW_DEBUG_PORT || 9262);
const pageUrl = process.env.PET_FLOW_URL || 'http://127.0.0.1:8000/web/index.html';
const configPath = process.env.MS_SERVER_CONFIG ||
  '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const account = 'test1', password = 'test1', characterName = 'test1Hero', cardIndex = 0;

const PET_ITEM = 5000001, PET_SN = '60000001', PET_PRICE = 4900;
const PET_MASK = 31;               // 磁铁1|物品袋2|HP袋4|MP袋8|飞行靴16
const POTION = 2000005;            // 超级药水：恢复全部 HP/MP
const AUTO_HP_KEY = 91, AUTO_MP_KEY = 92, HUNT_MAP = 40001, MISSING = -32768;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const charCard = index => ({ x: 105 + 120 * index + 25, y: 210 });
// CashShopLayout.h：目录卡片网格 GRID_POS{100,88} 步距{101,148} 4 列、图标中心
// 偏移 (42,40)；viewIndex = 高亮/选中索引 % 12。仓库右栏 7x3 格 (517,88)/39。
const cardCenter = v => ({ x: 100 + (v % 4) * 101 + 42, y: 88 + Math.floor(v / 4) * 148 + 40 });
const lockerSlot = r => ({ x: 517 + (r % 7) * 39 + 18, y: 88 + Math.floor(r / 7) * 39 + 18 });

let failed = false;
const phases = [];
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
const markPhase = label => phases.push(label);
async function waitFor(predicate, seconds = 30) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}
async function requireState(name, predicate, seconds = 30) {
  if (!await waitFor(predicate, seconds)) throw new Error(`Timed out: ${name}`);
  console.log(`PASS  ${name}`);
}
const uiStateIs = (driver, probes, expected) =>
  () => probes.currentUiState().then(value => value === expected);
function assetsIdle(driver, probes) {
  let hiddenSince = 0;
  return async () => {
    const hidden = await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
    if (!hidden || await probes.assetBlocking() !== 0) { hiddenSince = 0; return false; }
    if (!hiddenSince) hiddenSince = Date.now();
    return Date.now() - hiddenSince >= 1000;
  };
}

// ---- DB 预备（hero-combo-e2e.mjs 同款访问方式） ----
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

function prepareDatabase() {
  const db = readDbConfig(configPath);
  const row = mysqlRun(db, `SELECT id, map FROM characters WHERE name='${characterName}' LIMIT 1;`);
  if (!row) throw new Error(`character ${characterName} not found in DB`);
  const [characterId, map] = row.split('\t').map(Number);
  mysqlRun(db, `UPDATE accounts SET loggedin=0, nxCredit=100000 WHERE name='${account}';`);
  report(true, `DB: account ${account} loggedin=0 nxCredit=100000`);
  const schema = mysqlRun(db, 'DESCRIBE keymap;');
  const columns = new Set(schema.split('\n').map(line => line.split('\t')[0]));
  for (const column of ['characterid', 'key', 'type', 'action'])
    if (!columns.has(column)) throw new Error(`keymap schema lacks ${column}:\n${schema}`);
  mysqlRun(db, `DELETE FROM keymap WHERE characterid=${characterId} AND \`key\` IN (${AUTO_HP_KEY},${AUTO_MP_KEY});`);
  mysqlRun(db, `INSERT INTO keymap (characterid, \`key\`, type, action) VALUES (${characterId},${AUTO_HP_KEY},5,${POTION}),(${characterId},${AUTO_MP_KEY},5,${POTION});`);
  report(true, `DB: keymap ${AUTO_HP_KEY}/${AUTO_MP_KEY} -> item ${POTION}`);
  if (map !== HUNT_MAP) {
    mysqlRun(db, `UPDATE characters SET map=${HUNT_MAP}, spawnpoint=0 WHERE id=${characterId};`);
    report(true, `DB: character moved to map ${HUNT_MAP}`);
  }
  return { db, characterId };
}

// ---- 登录链（take-equip-flow 同款，附登录公告重试） ----
async function loginAndEnterGame(driver, probes) {
  await requireState('WASM module initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await requireState('login UI active', uiStateIs(driver, probes, uiState.login), 30);
  let advanced = false;
  for (let attempt = 0; attempt < 4 && !advanced; attempt += 1) {
    await driver.clickField(340, 260);
    await driver.typeAscii(account);
    await driver.clickField(340, 285);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () =>
      [uiState.world, uiState.characters].includes(await probes.currentUiState()), 20);
    if (!advanced && await probes.loginNoticeActive() === 1) {
      await driver.key('Enter', 'Enter', 13);
      await sleep(2000);
    }
  }
  if (!advanced) throw new Error('login never advanced past the login UI');
  await driver.retryClick(510, 216, uiStateIs(driver, probes, uiState.characters));
  await requireState('character-select UI active', uiStateIs(driver, probes, uiState.characters), 45);
  await requireState('character-select assets ready', assetsIdle(driver, probes), 60);
  const center = charCard(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await probes.currentUiState() === uiState.game;
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game field did not activate');
  await requireState('game assets ready', assetsIdle(driver, probes), 90);
}

async function enterCashShop(driver, probes) {
  // 商城入口随 HUD 世代移动：新版 (597,571)，传统 v83 (562,583)。
  let active = false;
  for (const [x, y] of [[597, 571], [562, 583]]) {
    await driver.click(x, y);
    active = await waitFor(uiStateIs(driver, probes, uiState.cashShop), 15);
    if (active) break;
  }
  if (!active) throw new Error('cash-shop UI did not activate');
  console.log('PASS  cash-shop UI active');
  await requireState('cash-shop assets ready', assetsIdle(driver, probes), 60);
}

// ---- 宠物分类 + SN 搜索 + 购买 ----
async function buyPet(driver, probes, cash, initial) {
  // 左栏第 8 项「宠物」：category_position(7)={4,212} 尺寸 90x22 → 中心 (49,223)。
  await driver.click(49, 223);
  const filtered = await cash('mscashshop_filtered_count');
  if (filtered <= 0)
    throw new Error(`pet category filtered nothing (filtered_count=${filtered}); ` +
      'WASM 可能仍是旧构建，等 /tmp/wasm-build.log 出现 Build finished successfully 后重试');
  report(true, `pet category shows ${filtered} commodities`);
  // 宠物目录图标按需流式加载：等加载屏退下，否则搜索框点击会被吞掉。
  await requireState('pet category assets ready', assetsIdle(driver, probes), 45);
  await driver.screenshot('01-pet-category');

  await driver.clickField(300, 50);
  await requireState('cash-shop search focused', () =>
    driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose(PET_SN);
  await requireState('cash-shop pet SN entered', () =>
    driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(PET_SN)}`), 10);
  await sleep(250);
  await driver.key('Enter', 'Enter', 13);
  await requireState('SN search highlights pet card', async () =>
    await cash('mscashshop_filtered_count') === 1 &&
    await cash('mscashshop_highlighted_item') === PET_ITEM, 10);
  await driver.screenshot('02-pet-search-highlight');

  const first = cardCenter(0);
  await driver.click(first.x, first.y);
  await requireState('pet catalog card selected', async () =>
    await cash('mscashshop_selected_left') >= 0, 10);
  // 真实购买路径：左栏卡片选中 → 右栏 BT_BUY 按钮 → 确认框确认。
  const buyX = await cash('mscashshop_buy_x');
  const buyY = await cash('mscashshop_buy_y');
  if (buyX === MISSING || buyY === MISSING)
    throw new Error(`BT_BUY purchase button missing (buy_x=${buyX}, buy_y=${buyY}); ` +
      'WASM 可能仍是旧构建，等 /tmp/wasm-build.log 出现 Build finished successfully 后重试');
  report(true, `BT_BUY purchase button at (${buyX},${buyY})`);
  await driver.click(buyX, buyY);
  await requireState('pet purchase confirmation visible', async () =>
    await cash('msui_notice_active') === 1, 10);
  // 确认框底图按需流式加载：等资源退下再截图并点击确认。
  await requireState('purchase confirmation assets ready', assetsIdle(driver, probes), 45);
  await sleep(500);
  await driver.screenshot('03-pet-purchase-confirm');
  const width = await cash('msui_resolution_width');
  const height = await cash('msui_resolution_height');
  const uiX = Math.max(0, Math.floor((width - 800) / 2));
  const uiY = Math.max(0, Math.floor((height - 600) / 2));
  await driver.click(380 + uiX, 285 + uiY);
  await requireState('pet purchase settles into locker', async () =>
    await cash('mscashshop_pending') === 0 &&
    await cash('mscashshop_nx_credit') === initial.nx - PET_PRICE &&
    await cash('mscashshop_locker_count') === initial.locker + 1, 30);
  report(true, `pet purchased for ${PET_PRICE} NX (nx ${initial.nx} -> ${await cash('mscashshop_nx_credit')})`);
  await driver.screenshot('04-pet-purchased');
  markPhase('买宠物');
}

// ---- 双击仓库槽取出到角色现金栏 ----
async function takePet(driver, probes, cash, initial) {
  const row = await cash('mscashshop_selected_right_row');
  report(row >= 0, `purchased pet selected in locker (row=${row})`);
  const slot = lockerSlot(row);
  await driver.doubleClick(slot.x, slot.y);
  await requireState('double-click takes pet exactly once', async () =>
    await cash('mscashshop_pending') === 0 &&
    await cash('mscashshop_locker_count') === initial.locker &&
    await cash('mscashshop_inventory_count') === initial.inventory + 1, 30);
  const cashSlot = await probes.inventorySlot(PET_ITEM);
  report(cashSlot >= 1 && cashSlot <= 24, `pet reached character cash inventory (slot=${cashSlot})`);
  await driver.screenshot('05-pet-taken');
  markPhase('取出');
}

// ---- 召唤（pet-automation ensurePetSummoned 序列） ----
async function summonPet(driver, probes) {
  if (await probes.petActive() !== 1) {
    const cashSlot = await probes.inventorySlot(PET_ITEM);
    if (cashSlot < 1 || cashSlot > 24)
      throw new Error(`pet unavailable in visible cash inventory: slot=${cashSlot}`);
    if (await probes.itemInventoryActive() !== 1) {
      await driver.click(711, 574);
      await sleep(400);
      const menuX = await probes.itemsMenuX();
      const menuY = await probes.itemsMenuY();
      if (menuX < 0 || menuY < 0) throw new Error(`items menu point unavailable: ${menuX},${menuY}`);
      await driver.click(menuX, menuY);
    }
    await requireState('item inventory active', async () =>
      await probes.itemInventoryActive() === 1, 30);
    await requireState('item inventory assets ready', assetsIdle(driver, probes), 45);
    const windowX = await probes.itemInventoryX();
    const windowY = await probes.itemInventoryY();
    // CASH is tab index 4; each v83 tab sprite is 32px apart from x+9.
    await driver.retryClick(windowX + 9 + 32 * 4 + 13, windowY + 24 + 8,
      async () => await probes.itemInventoryTab() === 5, 3, 2000);
    await requireState('cash inventory tab active', async () =>
      await probes.itemInventoryTab() === 5, 10);
    await requireState('cash inventory assets ready', assetsIdle(driver, probes), 45);
    await sleep(500);
    const column = (cashSlot - 1) % 4;
    const row = Math.floor((cashSlot - 1) / 4);
    // Socket art begins at window-relative (8,50); click its centre.
    const slotX = windowX + 8 + column * 36 + 21;
    const slotY = windowY + 50 + row * 34 + 18;
    const hitSlot = await probes.itemInventorySlotAt(slotX, slotY);
    report(hitSlot === cashSlot,
      `cash inventory slot ${cashSlot} hit test (got ${hitSlot} at ${slotX},${slotY}, window ${windowX},${windowY})`);
    for (let attempt = 0; attempt < 3; attempt += 1) {
      await driver.doubleClick(slotX, slotY);
      if (await waitFor(async () => await probes.petActive() === 1 &&
        await probes.petItemId() === PET_ITEM, 10)) break;
    }
    await requireState('purchased pet summoned', async () =>
      await probes.petActive() === 1 && await probes.petItemId() === PET_ITEM, 10);
    await driver.key('i', 'KeyI', 73, 'i');
    await sleep(300);
  }
  report(await probes.petItemId() === PET_ITEM, `active pet is ${await probes.petItemId()}`);
  const mask = await probes.petCapabilityMask();
  report(mask === PET_MASK, `pet intrinsic capability mask ${mask} == ${PET_MASK}`);
  await driver.screenshot('06-pet-summoned');
  markPhase('召唤');
}

// ---- 打怪 → 宠物自动捡取（verifyRuntimePickup 主循环） ----
async function verifyPickup(driver, probes) {
  if (await probes.mobCount() < 1)
    throw new Error(`pet pickup requires live mobs; map=${await probes.mapId()}`);
  const before = { pickups: await probes.petConfirmedPickups(),
    drops: await probes.dropCount(), meso: await probes.playerMeso(), mapId: await probes.mapId() };
  // rAF 采样宠物磁铁目标 OID 与服务端确认计数，锁定被捡掉落。
  await driver.evaluate(`(() => {
    const ev = { active: true, maxDrops: 0, lastTarget: 0,
      lastPickups: ${before.pickups}, confirmed: 0 };
    const tick = () => {
      if (!ev.active) return;
      const target = Module.ccall('mspet_loot_target_oid', 'number', [], []);
      const pickups = Module.ccall('mspet_confirmed_pickups', 'number', [], []);
      if (pickups > ev.lastPickups && !ev.confirmed) ev.confirmed = ev.lastTarget || target;
      ev.lastPickups = pickups;
      if (target) ev.lastTarget = target;
      ev.maxDrops = Math.max(ev.maxDrops, Module.ccall('msdrop_count', 'number', [], []));
      requestAnimationFrame(tick);
    };
    window.petLootEvidence = ev;
    requestAnimationFrame(tick);
  })()`);
  const trace = [];
  for (let attempt = 0; attempt < 100 &&
      await probes.petConfirmedPickups() === before.pickups; attempt += 1) {
    if (await probes.petHasLootTarget() === 1) { await sleep(300); continue; }
    const mobX = await probes.nearestMobX();
    const playerX = await probes.playerX();
    if (attempt % 5 === 0) trace.push({ attempt, playerX, mobX,
      mobs: await probes.mobCount(), drops: await probes.dropCount() });
    if (mobX <= MISSING) {
      await driver.keyHold('ArrowRight', 'ArrowRight', 39, 180);
      await sleep(220);
      continue;
    }
    const delta = mobX - playerX;
    const right = delta >= 0;
    const code = right ? 'ArrowRight' : 'ArrowLeft';
    await driver.keyHold(code, code, right ? 39 : 37,
      Math.abs(delta) > 65 ? Math.min(350, Math.max(80, Math.abs(delta) * 2)) : 35);
    await driver.keyHold('S', 'KeyS', 83, 60);
    await sleep(400);
  }
  // take-equip 同款：多等 15s 兜底，让宠物磁铁完成一次服务端确认捡取。
  await waitFor(async () => await probes.petConfirmedPickups() !== before.pickups, 15);
  const sampled = await driver.evaluate(
    "(() => (window.petLootEvidence.active = false, window.petLootEvidence))()");
  const targetOid = sampled.confirmed;
  let removal = { targetOid, removed: false };
  if (targetOid > 0) {
    const started = Date.now();
    while (Date.now() - started < 2000 && await probes.dropExists(targetOid) === 1)
      await sleep(50);
    removal = { targetOid, removed: await probes.dropExists(targetOid) === 0 };
  }
  const after = { pickups: await probes.petConfirmedPickups(),
    drops: await probes.dropCount(), meso: await probes.playerMeso() };
  fs.writeFileSync(`${artifactDir}/pet-pickup-evidence.json`,
    `${JSON.stringify({ before, after, sampled, removal, trace }, null, 2)}\n`);
  await driver.screenshot('07-pet-pickup');
  if (after.pickups <= before.pickups)
    throw new Error(`server-confirmed pet pickup never observed: ${JSON.stringify({ after, sampled, trace })}`);
  report(true, `pet auto loot confirmed (${before.pickups} -> ${after.pickups} pickups, meso ${before.meso} -> ${after.meso})`);
  if (targetOid > 0 && !removal.removed)
    throw new Error(`pickup did not erase drop oid=${targetOid}: ${JSON.stringify(removal)}`);
  markPhase('自动捡取');
}

// ---- 断线重登 + 低血蓝 → 宠物自动回血回蓝 ----
async function verifyAutoPotion(driver, probes, db, characterId) {
  const baseline = { potion: await probes.inventoryQuantity(POTION),
    hp: await probes.playerHp(), maxHp: await probes.playerMaxHp(),
    mp: await probes.playerMp(), maxMp: await probes.playerMaxMp() };
  if (baseline.potion < 1)
    throw new Error(`no ${POTION} potions left for the auto-potion phase`);
  try { await driver.evaluate('location.reload()'); } catch (error) {}
  await sleep(3000);  // 断线退出，让服务端存档
  mysqlRun(db, `UPDATE characters SET hp=ROUND(maxhp*0.3), mp=ROUND(maxmp*0.3) WHERE id=${characterId};`);
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  report(true, `DB drained vitals before relogin: ${mysqlRun(db,
    `SELECT hp, mp, maxhp, maxmp FROM characters WHERE id=${characterId};`).replace(/\t/g, '/')}`);
  await loginAndEnterGame(driver, probes);
  // 宠物嗑药可能在毫秒级发生：进游戏后立即采样登录时的真实血蓝。
  const atEntry = { hp: await probes.playerHp(), mp: await probes.playerMp() };
  await requireState('pet stays summoned after relogin', async () =>
    await probes.petActive() === 1, 30);
  report(await probes.petItemId() === PET_ITEM, `persisted pet item id ${await probes.petItemId()}`);
  await requireState('keymap auto HP potion wired', async () =>
    await probes.petAutoHpItem() === POTION, 30);
  await requireState('keymap auto MP potion wired', async () =>
    await probes.petAutoMpItem() === POTION, 30);
  const maxHp = await probes.playerMaxHp(), maxMp = await probes.playerMaxMp();
  await requireState('pet auto potion restores hp/mp to >=99%', async () =>
    await probes.playerHp() >= maxHp * 0.99 && await probes.playerMp() >= maxMp * 0.99, 30);
  const consumed = baseline.potion - await probes.inventoryQuantity(POTION);
  const restored = { hp: await probes.playerHp(), mp: await probes.playerMp() };
  report(consumed >= 1, `pet auto potion consumed ${consumed} x ${POTION} (${baseline.potion} -> ` +
    `${baseline.potion - consumed}), hp ${atEntry.hp}/${maxHp} -> ` +
    `${restored.hp}, mp ${atEntry.mp}/${maxMp} -> ${restored.mp}`);
  await driver.screenshot('08-auto-potion-restored');
  fs.writeFileSync(`${artifactDir}/auto-potion-evidence.json`,
    `${JSON.stringify({ baseline, atEntry, restored, consumed }, null, 2)}\n`);
  markPhase('自动回血回蓝');
}

async function dumpDiagnostics(driver, probes) {
  const names = ['msui_state', 'msmap_id', 'mspet_active', 'mspet_item_id',
    'mspet_capability_mask', 'mspet_confirmed_pickups', 'mscashshop_nx_credit',
    'mscashshop_filtered_count', 'mscashshop_highlighted_item'];
  const parts = [];
  for (const name of names)
    parts.push(`${name}=${await driver.evaluate(
      `Module.ccall(${JSON.stringify(name)}, 'number', [], [])`).catch(() => '?')}`);
  console.log(`diagnostics: ${parts.join(' ')}`);
}

async function main() {
  const prep = prepareDatabase();
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir });
  const probes = createRuntimeProbes(driver);
  const cash = name => probes.cashShopProbe(name);
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver, probes);
    await enterCashShop(driver, probes);
    const initial = { locker: await cash('mscashshop_locker_count'),
      inventory: await cash('mscashshop_inventory_count'), nx: await cash('mscashshop_nx_credit') };
    report(initial.nx === 100000, `initial NX credit is ${initial.nx}`);
    await buyPet(driver, probes, cash, initial);
    await takePet(driver, probes, cash, initial);
    await driver.key('Escape', 'Escape', 27);
    await requireState('cash shop exits back to game', uiStateIs(driver, probes, uiState.game), 30);
    await requireState('returned map assets ready', assetsIdle(driver, probes), 60);
    await requireState('game UI enabled', async () => await cash('msui_enabled') === 1, 60);
    report(await probes.mapId() === HUNT_MAP, `hunting map is ${await probes.mapId()}`);
    await summonPet(driver, probes);
    await verifyPickup(driver, probes);
    await verifyAutoPotion(driver, probes, prep.db, prep.characterId);
    for (const phase of phases) console.log(`PASS  cash-shop pet flow: ${phase}`);
  } catch (error) {
    report(false, `cash-shop pet flow aborted: ${error.message}`);
    console.error(error);
    try { await driver.screenshot('zz-failure'); await dumpDiagnostics(driver, probes); }
    catch (nested) {}
  } finally {
    if (driver.errors.length)
      fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

await main();
